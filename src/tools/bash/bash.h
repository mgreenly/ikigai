#ifndef BASH_H
#define BASH_H

#include <inttypes.h>

typedef struct {
    char *output;
    int32_t exit_code;
} bash_result_t;

// Execute shell command and return result
// Returns 0 on success (with result printed to stdout)
int32_t bash_execute(void *ctx, const char *command);

#endif // BASH_H
