#ifndef HTTP_H
#define HTTP_H

#include <curl/curl.h>
#include <inttypes.h>

struct resp_buf {
    void *c;
    char *d;
    size_t s;
};

size_t write_callback(void *p, size_t z, size_t m, void *u);
char *url_encode(void *c, const char *s);

#endif
