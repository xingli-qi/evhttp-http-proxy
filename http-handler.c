#include <string.h>
#include <stdlib.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include "http-handler.h"
#include "http-common.h"
#include "http-local.h"
#include "http-buffer.h"
#include "http-upstream.h"


int decode_request_and_check(
        struct evhttp_request* req,
        char** decoded_path)
{
    int rv = -1;
    char* p_decoded_path = NULL;
    struct evhttp_uri* decoded_uri = NULL;

    const char* uri = evhttp_request_get_uri(req);
    debug_msg("got a GET request for <%s>", uri);

    decoded_uri = evhttp_uri_parse(uri);
    if(decoded_uri == NULL) {
        debug_msg("bad url");
        goto err;
    }

    const char* path = evhttp_uri_get_path(decoded_uri);
    if(!path)
        path = "/";

    p_decoded_path = evhttp_uridecode(path, 0, NULL);
    if(p_decoded_path == NULL) {
        debug_msg("uridecode failed");
        goto err;
    }

    //.. in url in unfase for us
    if(strstr(p_decoded_path, "..")) {
        debug_msg("url path constains .. , unsafe");
        goto err;
    }

    *decoded_path = p_decoded_path;
    rv = 0;
    goto done;

err:
    if(p_decoded_path)
        free(p_decoded_path);
done:
    if(decoded_uri)
        free(decoded_uri);

    return rv;
}

void http_handler_cb(
        struct evhttp_request* req,
        void* arg)
{
    char* decoded_path;
    if(decode_request_and_check(req, &decoded_path) < 0) {
        debug_msg("bad url, send BADREQUEST");
        transaction_log(req, decoded_path, HTTP_BADREQUEST, 0, 0, "local");
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }

    http_delivery_status_t status;
    
    status = local_delivery(req, decoded_path);
    switch(status) {
        case http_delivery_success:
            goto done;
        case http_delivery_internal_error:
            transaction_log(req, decoded_path, 500, 0, 0, "local");
            evhttp_send_error(req, 500, "Internal Error");
            goto done;
        case http_delivery_not_found:
            break; //try next
        default:
            debug_msg("invalid status");
            goto done;
    }
   
    debug_msg("%s not found locally, try buffer.", decoded_path);

    status = buffer_delivery(req, decoded_path);
    switch(status) {
        case http_delivery_success:
            goto done;
        case http_delivery_internal_error:
            transaction_log(req, decoded_path, 500, 0, 0, "buffer");
            evhttp_send_error(req, 500, "Internal Error");
            goto done;
        case http_delivery_not_found:
            break; //try next
        default:
            debug_msg("invalid status");
            goto done;
    }

    debug_msg("%s not found in buffer, try upstream.", decoded_path);

    status = upstream_delivery(req, decoded_path);
    switch(status) {
        case http_delivery_success:
            goto done;
        case http_delivery_internal_error:
            transaction_log(req, decoded_path, 500, 0, 0, "proxy");
            evhttp_send_error(req, 500, "Internal Error");
            goto done;
        default:
            debug_msg("invalid status");
            goto done;
    }

done:
    free(decoded_path);
}
