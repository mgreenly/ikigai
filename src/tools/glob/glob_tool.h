#ifndef GLOB_TOOL_H
#define GLOB_TOOL_H

#include <inttypes.h>

// Execute glob pattern matching and output result to stdout
// Returns 0 on success (including errors reported as JSON)
int32_t glob_execute(void *ctx, const char *pattern, const char *path);

#endif // GLOB_TOOL_H
