#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <curl/curl.h>
#include <inttypes.h>

struct response_buffer {
    void *ctx;
    char *data;
    size_t size;
};

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
char *url_encode(void *ctx, const char *str);

#endif
