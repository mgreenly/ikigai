#include "http.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <string.h>
#include <talloc.h>

size_t write_callback(void *p, size_t z, size_t m, void *u)
{
    size_t t = z * m;
    struct resp_buf *b = u;
    char *nd = talloc_realloc(b->c, b->d, char, (unsigned int)(b->s + t + 1));
    if (nd == NULL) return 0;  // LCOV_EXCL_BR_LINE
    b->d = nd;
    memcpy(&b->d[b->s], p, t);
    b->s += t;
    b->d[b->s] = '\0';
    return t;
}

char *url_encode(void *c, const char *s)
{
    CURL *h = curl_easy_init();
    if (h == NULL) return NULL;  // LCOV_EXCL_BR_LINE

    char *e = curl_easy_escape(h, s, (int32_t)strlen(s));
    if (e == NULL) {  // LCOV_EXCL_BR_LINE
        curl_easy_cleanup(h);
        return NULL;
    }

    char *r = talloc_strdup(c, e);
    if (r == NULL) {  // LCOV_EXCL_BR_LINE
        curl_free(e);
        curl_easy_cleanup(h);
        return NULL;
    }
    curl_free(e);
    curl_easy_cleanup(h);
    return r;
}
