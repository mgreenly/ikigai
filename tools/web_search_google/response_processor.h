#ifndef RESPONSE_PROCESSOR_H
#define RESPONSE_PROCESSOR_H

#include "http_utils.h"

#include "vendor/yyjson/yyjson.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <stdbool.h>

struct api_call {
    CURL *handle;
    struct response_buffer response;
    char *domain;
    int64_t num_for_domain;
    bool success;
    char *url;
};

char *process_responses(void *ctx,
                        struct api_call *calls,
                        size_t num_calls,
                        size_t allowed_count,
                        size_t blocked_count,
                        yyjson_val *blocked_domains_val,
                        int64_t num);

#endif
