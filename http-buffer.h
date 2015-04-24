#ifndef HTTP_BUFFER_H
#define HTTP_BUFFER_H

#include "http-handler.h"

http_delivery_status_t
buffer_delivery(
        struct evhttp_request* req,
        char* decoded_path);

void
http_buffer_init();

#endif
