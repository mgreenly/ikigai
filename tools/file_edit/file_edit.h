#ifndef FILE_EDIT_H
#define FILE_EDIT_H

#include <inttypes.h>
#include <stdbool.h>

typedef struct {
    const char *file_path;
    const char *old_string;
    const char *new_string;
    bool replace_all;
} file_edit_params_t;

// Execute file edit operation and output result to stdout
// Returns 0 on success (including validation errors reported as JSON)
int32_t file_edit_execute(void *ctx, const file_edit_params_t *params);

#endif // FILE_EDIT_H
