#include "file_utils.h"
#include "error.h"
#include "panic.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
res_t ik_file_read_all(TALLOC_CTX *ctx, const char *path, char **out_content, size_t *out_size)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(path != NULL);        // LCOV_EXCL_BR_LINE
    assert(out_content != NULL); // LCOV_EXCL_BR_LINE

    // Open file
    FILE *f = fopen_(path, "rb");
    if (f == NULL) {
        return ERR(ctx, IO, "Failed to open %s", path);
    }

    // Get file size
    if (fseek_(f, 0, SEEK_END) != 0) {
        fclose_(f);
        return ERR(ctx, IO, "Failed to seek in %s", path);
    }

    long size = ftell_(f);
    if (size < 0) {
        fclose_(f);
        return ERR(ctx, IO, "Failed to get size of %s", path);
    }

    if (fseek_(f, 0, SEEK_SET) != 0) {
        fclose_(f);
        return ERR(ctx, IO, "Failed to seek in %s", path);
    }

    // Allocate buffer (safe cast: size is checked to be >= 0)
    size_t file_size = (size_t)size;

    // Check for overflow when adding 1 for null terminator
    if (file_size > (size_t)UINT_MAX - 1) {
        fclose_(f);
        return ERR(ctx, IO, "File too large: %s", path);
    }

    char *buffer = talloc_array(ctx, char, (unsigned int)(file_size + 1));
    if (buffer == NULL) {      // LCOV_EXCL_BR_LINE
        fclose_(f);             // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Read file contents
    size_t bytes_read = fread_(buffer, 1, file_size, f);
    fclose_(f);

    if (bytes_read != file_size) {
        return ERR(ctx, IO, "Failed to read %s: incomplete read", path);
    }

    // Null-terminate
    buffer[file_size] = '\0';

    // Set output parameters
    *out_content = buffer;
    if (out_size != NULL) {
        *out_size = file_size;
    }

    return OK(buffer);
}
