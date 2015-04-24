#ifndef HTTP_LOCAL_H
#define HTTP_LOCAL_H

#include <sys/types.h>
#include "http-handler.h"

typedef struct {
    int             fd;
    size_t          size;
    time_t          lmt; //last modification time
} file_info_t;

void http_local_init();

http_delivery_status_t local_delivery(
        struct evhttp_request* req,
        char* decoded_path);


#endif
