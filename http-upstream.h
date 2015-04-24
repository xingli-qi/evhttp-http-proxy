#ifndef HTTP_UPSTREAM_H
#define HTTP_UPSTREAM_H

#include "http-handler.h"

http_delivery_status_t upstream_delivery(
        struct evhttp_request* client_req,
        const char* decoded_path);

#endif
