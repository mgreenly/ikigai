#include "http_utils.h"

#include "panic.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response_buffer *buf = (struct response_buffer *)userp;

    char *new_data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)(buf->size + realsize + 1));
    if (new_data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    buf->data = new_data;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

char *url_encode(void *ctx, const char *str)
{
    CURL *curl = curl_easy_init();
    if (curl == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *encoded = curl_easy_escape(curl, str, (int32_t)strlen(str));
    if (encoded == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *result = talloc_strdup(ctx, encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    return result;
}
