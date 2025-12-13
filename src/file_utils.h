#ifndef IK_FILE_UTILS_H
#define IK_FILE_UTILS_H

#include "error.h"
#include <talloc.h>

// Read entire file into null-terminated buffer
// Returns: OK(buffer) where buffer is talloc-allocated on ctx
//          ERR on any failure (file not found, read error, etc.)
// Note: Caller owns returned buffer (freed with ctx)
res_t ik_file_read_all(TALLOC_CTX *ctx, const char *path, char **out_content, size_t *out_size);

#endif
