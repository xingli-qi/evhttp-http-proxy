#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <event2/buffer.h>

#include "http-common.h"
#include "http-local.h"
#include "hashmap.h"

#define MAX_LINE_LEN                1024
#define MAX_MIME_LEN                63
#define MAX_EXT_LEN                 7
#define MAX_EXT_NUM                 4
#define MAX_MIME_ENTRIES            128
#define DEFAULT_MIME                "application/octet-stream"

typedef struct {
    char    mime[MAX_MIME_LEN + 1];
    char    extensions[MAX_EXT_NUM][MAX_EXT_LEN + 1];
    int     extension_cnt;
} mime_t;

static mime_t mimes[MAX_MIME_ENTRIES];
static int mimes_len = 0;
static map_t mime_map;

void http_local_init()
{
    FILE* mime_file = fopen("mime.types", "r");
    assert(mime_file);
    
    char line[MAX_LINE_LEN];
    while(fgets(line, MAX_LINE_LEN, mime_file)) {
        if(line[0] == '#') //comment line
            continue;

        line[strlen(line)-1] = '\0'; //remove '\n'
        char* p_token = strtok(line, " ");
        if(p_token == NULL) //empty line
            continue;

        strcpy(mimes[mimes_len].mime, p_token);

        int cnt = 0;
        while((p_token = strtok(NULL, " "))) {
            strcpy(mimes[mimes_len].extensions[cnt], p_token);
            cnt++;
        }
        mimes[mimes_len].extension_cnt = cnt;

        mimes_len++;
    }
    fclose(mime_file);

    mime_map = hashmap_new();
    assert(mime_map);
    int i, j;
    for(i = 0; i < mimes_len; i++) {
        for(j = 0; j < mimes[i].extension_cnt; j++)
            hashmap_put(mime_map, mimes[i].extensions[j], mimes[i].mime);
    }
}

static char* get_local_content_ext(
        char* content_path)
{
    char* p = content_path + strlen(content_path);
    while(*p != '.' && *p != '/' && p != content_path)
        p--;

    if(*p == '/' || p == content_path)
        return NULL;

    p++;
    if(*p == '\0') // "/.../xxx." will be in this case
        return NULL;

    return p;
}

static char* ext2mime(
        char* ext)
{
    if(!ext || strlen(ext) == 0)
        return NULL;

    char* mime = NULL;
    hashmap_get(mime_map, ext, (void**)&mime);
    return mime;
}

static char* guess_content_type(
        char* content_path)
{
    char* mime = ext2mime(get_local_content_ext(content_path));
    if(!mime)
        mime = DEFAULT_MIME;

    return mime;
}

static int local_content(
        const char* pathname,
        file_info_t* p_file_info)
{
    debug_msg("local content path: %s", pathname);

    int fd;
    fd = open(pathname, O_RDONLY);
    if(fd == -1) {
        debug_msg("open failed: %s", strerror(errno));
        goto err;
    }

    struct stat info;
    if(fstat(fd, &info) == -1) {
        debug_msg("fstat failed: %s", strerror(errno));
        goto err;
    }

    if(S_ISREG(info.st_mode)) {
        p_file_info->fd     = fd;
        p_file_info->size   = info.st_size;
        p_file_info->lmt    = info.st_mtime;
        return 0;
    }
    else {
        debug_msg("%s is not regular file", pathname);
        goto err;
    }

err:
    if(fd >= 0)
        close(fd);

    return -1;
}

http_delivery_status_t local_delivery(
        struct evhttp_request* req,
        char* decoded_path)
{
    http_delivery_status_t rv = http_delivery_internal_error;
    struct evbuffer* evb = NULL;
    char* whole_path = NULL;

    int len = strlen(decoded_path) + strlen(root_dir) + 2;
    whole_path = malloc(len);
    if(!whole_path) {
        debug_msg("malloc failed");
        goto err;
    }

    evutil_snprintf(whole_path, len, "%s/%s", root_dir, decoded_path);
    file_info_t file_info;
    if(local_content(whole_path, &file_info) < 0) {
        rv = http_delivery_not_found;
        goto err;
    }

    evb = evbuffer_new();
    if(!evb) {
        debug_msg("evbuffer_new failed");
        goto err;
    }

    struct evkeyvalq* output_headers;
    output_headers = evhttp_request_get_output_headers(req);

    char* type = guess_content_type(decoded_path);
    debug_msg("guess content type: %s", type);
    evhttp_add_header(
            output_headers,
            "Content-Type",
            type);

    char time_lmt[TIME_BUFF_LEN];
    get_http_time(&file_info.lmt, time_lmt, TIME_BUFF_LEN);
    evhttp_add_header(
            output_headers,
            "Last-Modified",
            time_lmt);

    char curr_lmt[TIME_BUFF_LEN];
    get_http_time(NULL, curr_lmt, TIME_BUFF_LEN);
    evhttp_add_header(
            output_headers,
            "Date",
            curr_lmt);

    evhttp_add_header(
            evhttp_request_get_output_headers(req),
            "Server",
            SERVER);

    evbuffer_add_file(evb, file_info.fd, 0, file_info.size);
    transaction_log(req, decoded_path, 200, file_info.size, file_info.size, "local");
    evhttp_send_reply(req, 200, "OK", evb);
    rv = http_delivery_success;

err:
    if(whole_path)
        free(whole_path);
    if(evb)
        evbuffer_free(evb);

    return rv;
}
