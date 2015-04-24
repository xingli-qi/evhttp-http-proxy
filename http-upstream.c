#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <evhttp.h>

#include "http-upstream.h"
#include "http-common.h"

#define     CLIENT_TIMEOUT      10
#define     UPSTREAM_TIMEOUT    5
#define     WATERMARK           (8*4096)


typedef enum {
    xact_stat_none,
    xact_stat_header,
    xact_stat_body
} xact_stat;

typedef struct {
    xact_stat                   state;
    struct evhttp_request*      client_req;
    int                         client_aborted;
    struct evhttp_connection*   connection;
    size_t                      data_sent;
    size_t                      data_total;
    char*                       decoded_path;
    char*                       client_ip;
    char*                       client_user_agent;
    int                         resp_code;
} my_own_ctx;


#define TAILQ_FIRST(head)       ((head)->tqh_first)
#define TAILQ_NEXT(elm, field)  ((elm)->field.tqe_next)
#define TAILQ_END(head)         NULL
#define TAILQ_FOREACH(var, head, field) \
    for ((var) = TAILQ_FIRST(head); \
            (var) != TAILQ_END(head); \
            (var) = TAILQ_NEXT(var, field))

static void copy_header(
        struct evkeyvalq* dst_headers,
        const struct evkeyvalq* src_headers)
{
    struct evkeyval* header;
    
    TAILQ_FOREACH(header, src_headers, next) {
        evhttp_add_header(dst_headers, header->key, header->value);
    }

    //don't relay "Connection" field, libevent will set a proper one.
    evhttp_remove_header(dst_headers, "Connection");
    //we are http reverse proxy, don't relay "Server" field, set our own.
    evhttp_remove_header(dst_headers, "Server");
    evhttp_add_header(dst_headers, "Server", SERVER);
}

void watermark_high_cb(void* arg)
{
    //high watermark reached, disable upstream read.
    streaming_resp_cb_ctx* p_ctx = arg;
    my_own_ctx* p_my_ctx = p_ctx->arg;
    struct evhttp_connection* conn = p_my_ctx->connection;
    struct bufferevent* bufev = evhttp_connection_get_bufferevent(conn);
    bufferevent_disable(bufev, EV_READ);
}

void watermark_low_cb(void* arg)
{
    //low watermark reached, enable upstream read.
    streaming_resp_cb_ctx* p_ctx = arg;
    my_own_ctx* p_my_ctx = p_ctx->arg;
    struct evhttp_connection* conn = p_my_ctx->connection;
    struct bufferevent* bufev = evhttp_connection_get_bufferevent(conn);
    bufferevent_enable(bufev, EV_READ);
}

//we must be careful not to call "cleanup" more than once!
//the order of "close_cb" and "upstream_cb" calls "cleanup"
//should be considered carefully. see comments below.
static void cleanup(
        streaming_resp_cb_ctx* p_ctx)
{
    //for memory leak detect when debugging
    debug_msg("streaming_resp_cb_ctx[%p] destroyed", p_ctx);
    
    my_own_ctx* p_my_ctx = p_ctx->arg;

    //if "upstream_cb" calls "cleanup" before "close_cb" does,
    //un-register the close_cb function which is registered
    //in upstream_delivery, otherwise close_cb will access
    //memory we've freed here.
    if(!p_my_ctx->client_aborted) {
        struct evhttp_connection* client_conn;
        client_conn = evhttp_request_get_connection(p_my_ctx->client_req);
        evhttp_connection_set_closecb(
                client_conn,
                NULL,
                NULL);
    }

    //"req" created in upstream_delivery will be freed within evhttp_connection_free.
    //if "close_cb" calls "cleanup" before "upstream_cb" does,
    //"evhttp_connection_free" makes sure that "upstream_cb" will not be called
    //again, so does "cleanup".
    evhttp_connection_free(p_my_ctx->connection);

    free(p_my_ctx->decoded_path);
    free(p_my_ctx->client_ip);
    free(p_my_ctx->client_user_agent);
    free(p_my_ctx);
    free(p_ctx);
}

static void upstream_cb(
        struct evhttp_request* req,
        void* arg)
{
    streaming_resp_cb_ctx* p_ctx = arg;
    my_own_ctx* p_my_ctx = p_ctx->arg;

    if(!req) {
        //this is due to http response header invalid or connection down in most cases.
        debug_msg("header invalid or connection down");
        if(p_my_ctx->state == xact_stat_header ||
                p_my_ctx->state == xact_stat_body)
            transaction_log(
                    p_my_ctx->client_req,
                    p_my_ctx->decoded_path,
                    p_my_ctx->resp_code,
                    p_my_ctx->data_sent,
                    p_my_ctx->data_total,
                    "proxy");
            evhttp_send_reply_end_streaming(p_my_ctx->client_req);
            cleanup(p_ctx);
        return;
    }

    const char* content_length;
    switch(p_ctx->stat) {
        case streaming_resp_stat_none:
            debug_msg("connecting to upstream timeout");
            transaction_log(p_my_ctx->client_req, p_my_ctx->decoded_path, 504, 0, 0,  "proxy");
            evhttp_send_error(p_my_ctx->client_req, 504, "Gateway Timeout");
            cleanup(p_ctx);
            break;
        case streaming_resp_stat_header:
            content_length = evhttp_find_header(
                    evhttp_request_get_input_headers(req),
                    "Content-Length");
            if(content_length)
                p_my_ctx->data_total = atoi(content_length);
            p_my_ctx->resp_code = evhttp_request_get_response_code(req);
            copy_header(
                    evhttp_request_get_output_headers(p_my_ctx->client_req),
                    evhttp_request_get_input_headers(req));
            evhttp_send_reply_header_streaming(
                    p_my_ctx->client_req,
                    p_my_ctx->resp_code,
                    req->response_code_line);
            p_my_ctx->state = xact_stat_header;
            break;
        case streaming_resp_stat_body:
            evhttp_send_reply_body_streaming_watermark(
                    p_my_ctx->client_req,
                    evhttp_request_get_input_buffer(req),
                    WATERMARK,
                    watermark_high_cb,
                    watermark_low_cb,
                    p_ctx);
            p_my_ctx->data_sent += req->body_size;
            if(p_my_ctx->state == xact_stat_header)
                p_my_ctx->state = xact_stat_body;
            break;
        case streaming_resp_stat_end:
            transaction_log(
                    p_my_ctx->client_req,
                    p_my_ctx->decoded_path,
                    p_my_ctx->resp_code,
                    p_my_ctx->data_sent,
                    p_my_ctx->data_total,
                    "proxy");
            evhttp_send_reply_end_streaming(p_my_ctx->client_req);
            cleanup(p_ctx);
            break;
        default:
            debug_msg("?! invalid state: %d", p_ctx->stat);
            cleanup(p_ctx);
    }
}

static void close_cb(
        struct evhttp_connection* evcon,
        void* arg)
{
    debug_msg("client aborted");
    streaming_resp_cb_ctx* p_ctx = arg;
    my_own_ctx* p_my_ctx = p_ctx->arg;
    transaction_log_no_req(
            p_my_ctx->client_ip,
            p_my_ctx->client_user_agent,
            p_my_ctx->decoded_path,
            p_my_ctx->resp_code,
            p_my_ctx->data_sent,
            p_my_ctx->data_total,
            "proxy");
    p_my_ctx->client_aborted = 1;
    cleanup(p_ctx);
}

http_delivery_status_t upstream_delivery(
        struct evhttp_request* client_req,
        const char* decoded_path)
{
    char* my_decoded_path           = NULL;
    char* my_client_ip              = NULL;
    char* my_client_user_agent      = NULL;
    struct evhttp_connection* conn  = NULL;
    streaming_resp_cb_ctx* p_ctx    = NULL;
    my_own_ctx* p_my_ctx            = NULL;
    struct evhttp_request* req      = NULL;

    //doecode_path points to memory owned by libevent,
    //and will be modified later, so we have keep a copy.
    my_decoded_path = strdup(decoded_path);
    if(!my_decoded_path) {
        debug_msg("strdup");
        goto error;
    }

    char* host;
    uint16_t port;
    evhttp_connection_get_peer(
            evhttp_request_get_connection(client_req),
            &host,
            &port);
    my_client_ip = strdup(host);
    if(!my_client_ip) {
        debug_msg("strdup");
        goto error;
    }

    struct evkeyvalq* input_headers;
    input_headers = evhttp_request_get_input_headers(client_req);
    const char* user_agent = evhttp_find_header(input_headers, "User-Agent");
    if(user_agent) {
        my_client_user_agent = strdup(user_agent);
        if(!my_client_user_agent) {
            debug_msg("strdup");
            goto error;
        }
    }

    struct evhttp_connection* client_conn;
    client_conn = evhttp_request_get_connection(client_req);
    evhttp_connection_set_timeout(
            client_conn,
            CLIENT_TIMEOUT);

    conn = evhttp_connection_base_new(
            event_base,
            NULL,
            upstream,
            80); //default http port
    if(!conn) {
        debug_msg("evhttp_connection_base_new");
        goto error;
    }

    //this timeout value means how long after since the connection issue happened
    //will we be notified, i.e., our callback function be called.
    evhttp_connection_set_timeout(conn, UPSTREAM_TIMEOUT);
 
    p_ctx = malloc(sizeof(streaming_resp_cb_ctx));
    if(!p_ctx) {
        debug_msg("malloc");
        goto error;
    }
    p_my_ctx = malloc(sizeof(my_own_ctx));
    if(!p_my_ctx) {
        debug_msg("malloc");
        goto error;
    }
    p_my_ctx->client_req        = client_req;
    p_my_ctx->client_aborted    = 0;
    p_my_ctx->connection        = conn;
    p_my_ctx->state             = xact_stat_none;
    p_my_ctx->data_sent         = 0;
    p_my_ctx->data_total        = 0;
    p_my_ctx->decoded_path      = my_decoded_path;
    p_my_ctx->client_ip         = my_client_ip;
    p_my_ctx->client_user_agent = my_client_user_agent;
    p_my_ctx->resp_code         = -1;
    p_ctx->stat                 = streaming_resp_stat_none;
    p_ctx->arg                  = p_my_ctx;
    
    req = evhttp_request_new(
            upstream_cb,
            p_ctx); // this MUST be a pointer of type streaming_resp_cb_ctx*
    if(!req) {
        debug_msg("evhttp_request_new");
        goto error;
    }

    //we want to be notified when client aborts during http response is being
    //prepared/transferred, so that we could do clean up work.
    evhttp_connection_set_closecb(
            client_conn,
            close_cb,
            p_ctx);

    //libevent doesn't add "Host" filed automatically, we have to do it explicitly.
    //some server, like Apache, will complain the request as invalid if no "Host".
    evhttp_add_header(req->output_headers, "Host", upstream);

    int rv;
    rv = evhttp_make_request_streaming(
            conn,
            req,
            EVHTTP_REQ_GET,
            my_decoded_path);
    if(rv < 0) {
        debug_msg("evhttp_make_request_streaming");
        goto error;
    }

    //for memory leak detect when debugging
    debug_msg("streaming_resp_cb_ctx[%p] created", p_ctx);

    return http_delivery_success;

error:
    if(my_decoded_path)
        free(my_decoded_path);
    if(my_client_ip)
        free(my_client_ip);
    if(my_client_user_agent)
        free(my_client_user_agent);
    if(conn)
        evhttp_connection_free(conn);
    if(p_ctx)
        free(p_ctx);
    if(p_my_ctx)
        free(p_my_ctx);
    if(req)
        evhttp_request_free(req);

    return http_delivery_internal_error; 
}
