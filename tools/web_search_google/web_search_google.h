#ifndef IK_WEB_SEARCH_GOOGLE_H
#define IK_WEB_SEARCH_GOOGLE_H

#include <inttypes.h>
#include <stdbool.h>

#include "vendor/yyjson/yyjson.h"

typedef struct {
    const char *query;
    int64_t num;
    int64_t start;
    yyjson_val *allowed_domains_val;
    yyjson_val *blocked_domains_val;
    size_t allowed_count;
    size_t blocked_count;
} web_search_google_params_t;

// Execute web search with given parameters
// Returns 0 on success (outputs JSON to stdout), non-zero on fatal error
int32_t web_search_google_execute(void *ctx, const web_search_google_params_t *params);

#endif // IK_WEB_SEARCH_GOOGLE_H
