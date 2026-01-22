#ifndef IK_GREP_H
#define IK_GREP_H

#include <inttypes.h>

typedef struct {
    char *output;
    int32_t count;
} grep_result_t;

typedef struct {
    const char *pattern;
    const char *glob;
    const char *path;
} grep_params_t;

int32_t grep_search(void *ctx, const grep_params_t *params, grep_result_t *out);

#endif
