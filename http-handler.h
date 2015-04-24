#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <event2/http.h>

#define SERVER  "POC Multicast"

typedef enum {
    http_delivery_success,
    http_delivery_not_found,
    http_delivery_internal_error
} http_delivery_status_t;

void    http_handler_cb(struct evhttp_request*, void*);

#endif
