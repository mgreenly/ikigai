#ifndef WEB_SEARCH_H
#define WEB_SEARCH_H

#include <inttypes.h>
#include <stdbool.h>

#include "vendor/yyjson/yyjson.h"

typedef struct {
    const char *query;
    int32_t count;
    int32_t offset;
    yyjson_val *allowed_domains;
    yyjson_val *blocked_domains;
} web_search_params_t;

// Execute web search and output result to stdout
// Returns 0 on success (including errors reported as JSON)
int32_t web_search_execute(void *ctx, const web_search_params_t *params);

#endif
