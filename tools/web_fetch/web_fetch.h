#ifndef WEB_FETCH_H
#define WEB_FETCH_H

#include <inttypes.h>
#include <stdbool.h>

typedef struct {
    const char *url;
    int64_t offset;
    int64_t limit;
    bool has_offset;
    bool has_limit;
} web_fetch_params_t;

// Execute web fetch operation and output result to stdout
// Returns 0 on success (including errors reported as JSON)
int32_t web_fetch_execute(void *ctx, const web_fetch_params_t *params);

#endif
