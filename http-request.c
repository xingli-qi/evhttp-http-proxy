#include <event2/event.h>
#include <event2/http.h>

#include "http-request.h"
#include "http-common.h"
#include "http-handler.h"


int http_request_init()
{
    struct evhttp* http;

    http = evhttp_new(event_base);
    if(!http) {
        printf("evhttp_new failed\n");
        return -1;
    }

    evhttp_set_allowed_methods(http, EVHTTP_REQ_GET);
    evhttp_set_gencb(http, http_handler_cb, NULL);
    int rv;
    rv = evhttp_bind_socket(
            http,
            "0.0.0.0",
            80);
    if(rv < 0) {
        printf("evhttp_bind_socket failed!\n");
        return -1;
    }

    return 0;
}
