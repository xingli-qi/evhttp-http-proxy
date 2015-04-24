#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <event2/event.h>

#include "http-common.h"
#include "http-request.h"
#include "http-local.h"
#include "http-buffer.h"
#include "http-prmm.h"

struct event_base* event_base;
const char* root_dir;
const char* upstream;

int main(int argc, const char* argv[])
{
    if(argc != 3) {
        printf("usage: http-proxy root-dir upstream\n");
        exit(-1);
    }
   
    struct sigaction act;
    act.sa_handler  = SIG_IGN;
    act.sa_flags    = 0;
    if(sigfillset(&act.sa_mask) != 0) {
        debug_msg("sigfillset failed!");
        exit(-1);
    }
    if(sigaction(SIGPIPE, &act, NULL) != 0) {
        debug_msg("sigaction failed!");
        exit(-1);
    }

    debug_init();
    xact_init();

    debug_msg("============================");
    debug_msg("======http-proxy start======");
    debug_msg("============================");

    root_dir = argv[1];
    upstream = argv[2];
    debug_msg("root directory: %s", root_dir);
    debug_msg("upstream: %s", upstream);

    event_base = event_base_new();
    
    http_local_init();
    http_buffer_init();
    http_prmm_init();
    
    int rv;
    rv = http_request_init();
    if(rv < 0) {
        printf("http_request_init failed, exit!\n");
        return -1;
    }
    
    debug_msg("http-proxy is up and running...");
    event_base_dispatch(event_base);

    return 0;
}
