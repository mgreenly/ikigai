#ifndef FILE_WRITE_LOGIC_H
#define FILE_WRITE_LOGIC_H

#include <inttypes.h>
#include <stddef.h>

// Perform file write operation
// Returns 0 on success or error (with error printed to stdout)
int32_t do_file_write(void *ctx, const char *path, const char *content, size_t content_len);

#endif // FILE_WRITE_LOGIC_H
