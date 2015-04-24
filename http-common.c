#include <string.h>
#include <unistd.h>
#include "http-common.h"

#define DEBUG_FILE  "errorlog"
#define XACT_FILE   "xactlog"


FILE* debug_file = NULL;
FILE* xact_file = NULL;

void debug_init()
{
    if(!debug_file) {
        debug_file = fopen(DEBUG_FILE, "a");
        if(!debug_file)
            printf("debug file open failed!\n");
    }
}

void xact_init()
{
    if(!xact_file) {
        xact_file = fopen(XACT_FILE, "a");
        if(!xact_file)
            printf("xact file open failed!\n");
    }

    xact_log("#Date | Host | User-Agent | URL | Response-Code | Sent/Total | Type(local|buffer|proxy)");
}

void get_http_time(
        time_t* t,
        char* buff,
        int buff_len
        )
{
    time_t* p_time;
    if(t)
        p_time = t;
    else {
        time_t now = time(NULL);
        p_time = &now;
    }

    struct tm tm = *gmtime(p_time);
    strftime(buff, buff_len, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

void transaction_log(
        struct evhttp_request* req,
        const char* decoded_path,
        int resp_code,
        int size_sent,
        int size_total,
        const char* type)
{
    char time[TIME_BUFF_LEN];
    get_http_time(NULL, time, TIME_BUFF_LEN);
    
    char* host;
    uint16_t port;
    evhttp_connection_get_peer(
            evhttp_request_get_connection(req),
            &host,
            &port);

    struct evkeyvalq* input_headers;
    input_headers = evhttp_request_get_input_headers(req);
    const char* user_agent = evhttp_find_header(input_headers, "User-Agent");
    if(!user_agent)
        user_agent = "-";

    xact_log("%s | %s | %s | %s | %d | %d/%d | %s",
            time, host, user_agent, decoded_path, resp_code, size_sent, size_total, type);
}

void transaction_log_no_req(
        const char* host,
        const char* user_agent,
        const char* decoded_path,
        int resp_code,
        int size_sent,
        int size_total,
        const char* type)
{
    char time[TIME_BUFF_LEN];
    get_http_time(NULL, time, TIME_BUFF_LEN);

    if(!user_agent)
        user_agent = "-";
    
    xact_log("%s | %s | %s | %s | %d | %d/%d | %s",
            time, host, user_agent, decoded_path, resp_code, size_sent, size_total, type);
}
