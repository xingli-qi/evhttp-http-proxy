#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "http-buffer.h"
#include "http-common.h"
#include "http-local.h"
#include "http-prmm.h"

#define MAX_LINE_LEN        1024
#define CHANNEL_ID_MAX_LEN  127
#define MADDRESS_MAX_LEN    45
#define CHANNEL_ENTRY_MAX   32


typedef struct {
    char            channel_id[CHANNEL_ID_MAX_LEN + 1]; /* \0 */
    char            maddress[MADDRESS_MAX_LEN + 1]; /* \0 */
    unsigned short  mport;
} channel_info_t;

typedef struct {
    struct evhttp_request*      client_req;
    size_t                      data_sent;
    size_t                      data_total;
    char*                       decoded_path;
    char*                       client_ip;
    char*                       client_user_agent;
    prmm_req                    prmm_r;
} buffer_xact_ctx;

static channel_info_t channel_table[CHANNEL_ENTRY_MAX];
static unsigned short channel_table_size = 0;

void
http_buffer_init()
{
    FILE* channel_file = fopen("channel.info", "r");
    assert(channel_file);
    
    char line[MAX_LINE_LEN];
    char* p_token;
    while(fgets(line, MAX_LINE_LEN, channel_file)) {
        if(line[0] == '#') //comment line
            continue;

        line[strlen(line)-1] = '\0'; //remove '\n'
        p_token = strtok(line, " ");
        if(p_token == NULL) //empty line
            continue;

        assert(strlen(p_token) <= CHANNEL_ID_MAX_LEN);
        strcpy(channel_table[channel_table_size].channel_id, p_token);
        debug_msg("channel id: %s", p_token);

        p_token = strtok(NULL, " ");
        assert(p_token);
        assert(strlen(p_token) <= MADDRESS_MAX_LEN);
        strcpy(channel_table[channel_table_size].maddress, p_token);
        debug_msg("maddress: %s", p_token);

        p_token = strtok(NULL, " ");
        assert(p_token);
        channel_table[channel_table_size].mport = atoi(p_token);
        debug_msg("mport: %d", channel_table[channel_table_size].mport);

        channel_table_size++;
    }
    fclose(channel_file);
}

static void
http_prmm_cb(
        void* arg)
{
}

static
channel_info_t*
get_channel(
        const char* decoded_path)
{
    int i;
    const char* rv;
    for(i = 0; i < channel_table_size; i++) {
        rv = strstr(decoded_path, channel_table[i].channel_id);
        if(rv)
            return channel_table + i;
    }
    return NULL;
}

http_delivery_status_t
buffer_delivery(
        struct evhttp_request* req,
        char* decoded_path)
{
    char* my_decoded_path           = NULL;
    char* my_client_ip              = NULL;
    char* my_client_user_agent      = NULL;
    buffer_xact_ctx* p_ctx          = NULL;
    prmm_req prmm_r                 = NULL;
    int content_on_disk             = 0;

    http_delivery_status_t stat;
    stat = http_delivery_internal_error;

    channel_info_t* p_channel;
    p_channel = get_channel(decoded_path);
    if(!p_channel) {
        debug_msg("channel not identified for %s", decoded_path);
        stat = http_delivery_not_found;
        return stat; /*we don't allocate anything, so just return*/
    }

    /*
     * doecode_path points to memory owned by libevent,
     * and will be modified later, so we have keep a copy.
     */
    my_decoded_path = strdup(decoded_path);
    if(!my_decoded_path) {
        debug_msg("strdup");
        goto error;
    }

    char* host;
    uint16_t port;
    evhttp_connection_get_peer(
            evhttp_request_get_connection(req),
            &host,
            &port);
    my_client_ip = strdup(host);
    if(!my_client_ip) {
        debug_msg("strdup");
        goto error;
    }

    struct evkeyvalq* input_headers;
    input_headers = evhttp_request_get_input_headers(req);
    const char* user_agent = evhttp_find_header(input_headers, "User-Agent");
    if(user_agent) {
        my_client_user_agent = strdup(user_agent);
        if(!my_client_user_agent) {
            debug_msg("strdup");
            goto error;
        }
    }

    p_ctx = malloc(sizeof(buffer_xact_ctx));
    if(!p_ctx) {
        debug_msg("malloc");
        goto error;
    }
    p_ctx->client_req           = req;
    p_ctx->data_sent            = 0;
    p_ctx->data_total           = 0;
    p_ctx->decoded_path         = my_decoded_path;
    p_ctx->client_ip            = my_client_ip;
    p_ctx->client_user_agent    = my_client_user_agent;

    prmm_conn prmm_c;
    prmm_c = get_prmm_conn(
            p_channel->channel_id,
            p_channel->maddress,
            p_channel->mport);
    if(!prmm_c) {
        debug_msg("get_prmm_conn");
        goto error;
    }

    prmm_r = prmm_request_new(
            prmm_c,
            decoded_path,
            http_prmm_cb,
            p_ctx);
    if(!prmm_r) {
        debug_msg("prmm_request_new");
        goto error;
    }
    p_ctx->prmm_r = prmm_r;

    prmm_request_stat_t prmm_stat;
    prmm_stat = prmm_request_start(prmm_r);
    switch(prmm_stat) {
        case prmm_request_success:
            break;
        case prmm_request_failed:
            debug_msg("can't get content via prmm");
            stat = http_delivery_not_found;
            goto error;
        case prmm_request_on_disk:
            debug_msg("content has been written to disk just now,");
            debug_msg("so use local delivery instead.");
            content_on_disk = 1;
            goto error; /*this isn't error actually, we just need to free memory*/
        default:
            debug_msg("?! invalid stat: %d", stat);
            goto error;
    }

    /*for memory leak detect when debugging*/
    debug_msg("buffer_xact_ctx[%p] created", p_ctx);

    return http_delivery_success;

error:
    if(my_decoded_path)
        free(my_decoded_path);
    if(my_client_ip)
        free(my_client_ip);
    if(my_client_user_agent)
        free(my_client_user_agent);
    if(p_ctx)
        free(p_ctx);
    if(prmm_r)
        prmm_request_free(prmm_r);

    if(content_on_disk)
        return local_delivery(req, decoded_path);
        
    return stat;
}
