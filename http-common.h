#ifndef HTTP_COMMON_H
#define HTTP_COMMON_H

#include <stdio.h>
#include <time.h>
#include <event2/http.h>

#define debug_msg(fmt, args...) do { \
    if(debug_file) { \
        fprintf(debug_file, "%s(%d)-%s: " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##args); \
        fflush(debug_file); \
    } \
}while(0)

#define xact_log(fmt, args...) do { \
    if(xact_file) { \
        fprintf(xact_file, fmt "\n", ##args); \
        fflush(xact_file); \
    } \
}while(0)

extern struct event_base* event_base;
extern FILE* debug_file;
extern FILE* xact_file;
extern const char* root_dir;
extern const char* upstream;

void debug_init();

void xact_init();

void transaction_log(
        struct evhttp_request* req,
        const char* decoded_path,
        int resp_code,
        int size_sent,
        int size_total,
        const char* type);

//sometimes "req" has been already inaccessible
//when we want to write trnsaction log
void transaction_log_no_req(
        const char* host,
        const char* user_agent,
        const char* decoded_path,
        int resp_code,
        int size_sent,
        int size_total,
        const char* type);

#define TIME_BUFF_LEN               32
void get_http_time(
        time_t* t,
        char* buff,
        int buff_len
        );

#endif
