#include <stdlib.h>
#include <assert.h>
#include <sys/queue.h>

#include "hashmap.h"
#include "prmm_rcv.h"
#include "http-prmm.h"
#include "http-common.h"

static map_t channel_map;

typedef struct {
    prmm_r_ctxt_t*  receiver;
    char*           channel_id;
    char*           maddress;
    unsigned short  mport;
    map_t           mem_cache; /*filepath->mem_cache_t*/
} prmm_conn_t;

typedef struct prmm_req_s {
    prmm_r_xact_t*          request;
    prmm_conn_t*            conn;
    void                    (*cb)(void*);
    void*                   arg;
    TAILQ_ENTRY(prmm_req_s) entry;
} prmm_req_t;

typedef struct mem_chunk_s {
    const char*                 mem;
    TAILQ_ENTRY(mem_chunk_s)    entry;
} mem_chunk_t;

typedef struct {
    int                             ref_cnt;
    char                            complete;
    TAILQ_HEAD(mhead, mem_chunk_s)  mem_list;
    TAILQ_HEAD(rhead, prmm_req_s)   req_list;
} mem_cache_t;


void
http_prmm_init()
{
    channel_map = hashmap_new();
    assert(channel_map);
    prmm_r_create();
}

prmm_conn
get_prmm_conn(
        char* channel_id,
        char* maddress,
        unsigned short mport)
{
    void* conn = NULL;
    hashmap_get(channel_map, channel_id, &conn);
    if(conn)
        return conn;

    prmm_conn_t* p_conn         = NULL;
    map_t p_map                 = NULL;

    p_conn = malloc(sizeof(prmm_conn_t));
    if(!p_conn) {
        debug_msg("malloc");
        goto error;
    }

    p_map = hashmap_new();
    if(!p_map) {
        debug_msg("hashmap_new");
        goto error;
    }

    prmm_r_ctxt_t* receiver = NULL;
    int rv;
    rv = prmm_r_start(maddress, mport, root_dir, &receiver);
    if(rv != PRMM_SUCCESS) {
        debug_msg("prmm_r_start");
        goto error;
    }
    assert(receiver);

    p_conn->receiver    = receiver;
    p_conn->channel_id  = channel_id;
    p_conn->maddress    = maddress;
    p_conn->mport       = mport;
    p_conn->mem_cache   = p_map;

    rv = hashmap_put(channel_map, channel_id, p_conn);
    if(rv != MAP_OK)
        debug_msg("hashmap_put failed");

    return p_conn;

error:
    if(p_conn)
        free(p_conn);
    if(p_map)
        hashmap_free(p_map);

    return NULL;
}

void
del_prmm_conn(
        prmm_conn conn)
{
    prmm_conn_t* p_conn = conn;
    prmm_r_stop(p_conn->receiver);
    /*do i need to free p_conn->receiver?*/
    hashmap_remove(channel_map, p_conn->channel_id);
    free(p_conn);
}

prmm_req
prmm_request_new(
        prmm_conn conn,
        const char* filepath,
        void (*cb)(void*),
        void* arg)
{
    return NULL;
}

void
prmm_request_free(
        prmm_req req)
{
}

prmm_request_stat_t
prmm_request_start(
        prmm_req req)
{
    return prmm_request_success;
}
