#include "file_write_logic.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "shared/wrapper.h"

#include "vendor/yyjson/yyjson.h"

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);
}

int32_t do_file_write(void *ctx, const char *path, const char *content, size_t content_len)
{
    // Open file for writing
    FILE *fp = fopen_(path, "w");
    if (fp == NULL) {
        if (errno == EACCES) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Permission denied: %s", path);
            output_error(ctx, error_msg, "PERMISSION_DENIED");
        } else if (errno == ENOSPC) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "No space left on device: %s", path);
            output_error(ctx, error_msg, "NO_SPACE");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", path);
            output_error(ctx, error_msg, "OPEN_FAILED");
        }
        return 0;
    }

    // Write content to file
    size_t written = fwrite_(content, 1, content_len, fp);
    if (written != content_len) {
        fclose_(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to write file: %s", path);
        output_error(ctx, error_msg, "WRITE_FAILED");
        return 0;
    }

    fclose_(fp);

    // Extract basename for success message
    char *path_copy = talloc_strdup(ctx, path);
    if (path_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *filename = basename(path_copy);

    // Build success message
    char success_msg[512];
    snprintf(success_msg, sizeof(success_msg), "Wrote %zu bytes to %s", content_len, filename);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, success_msg);
    if (output_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *bytes_val = yyjson_mut_uint(output_doc, (uint64_t)content_len);
    if (bytes_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "bytes", bytes_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
