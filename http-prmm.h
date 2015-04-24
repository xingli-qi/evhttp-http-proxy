#ifndef HTTP_PRMM_H
#define HTTP_PRMM_H

typedef void* prmm_conn;
typedef void* prmm_req;

typedef enum {
    prmm_request_success,
    prmm_request_failed,
    prmm_request_on_disk /*requested content already on disk*/
} prmm_request_stat_t;

prmm_conn
get_prmm_conn(
        char* channel_id,
        char* maddress,
        unsigned short mport);

void
del_prmm_conn(
        prmm_conn conn);

prmm_req
prmm_request_new(
        prmm_conn conn,
        const char* filepath,
        void (*cb)(void*),
        void* arg);

void
prmm_request_free(
        prmm_req req);

prmm_request_stat_t
prmm_request_start(
        prmm_req req);

void
http_prmm_init();

#endif
