#ifndef FILE_READ_H
#define FILE_READ_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

// Execute file read operation and output result to stdout
// Returns 0 on success (including errors reported as JSON)
int32_t file_read_execute(void *ctx, const char *path, bool has_offset, int64_t offset, bool has_limit, int64_t limit);

#endif
