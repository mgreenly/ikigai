#include "glob_tool.h"

#include "json_allocator.h"
#include "panic.h"
#include "wrapper_posix.h"

#include <glob.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

int32_t glob_execute(void *ctx, const char *pattern, const char *path)
{
    // Build full pattern: path/pattern or just pattern
    char *full_pattern = NULL;
    if (path != NULL && path[0] != '\0') {
        size_t path_len = strlen(path);
        size_t pattern_len = strlen(pattern);
        full_pattern = talloc_array(ctx, char, (unsigned int)(path_len + pattern_len + 2));
        if (full_pattern == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        snprintf(full_pattern, path_len + pattern_len + 2, "%s/%s", path, pattern);
    } else {
        full_pattern = talloc_strdup(ctx, pattern);
        if (full_pattern == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    // Execute glob
    glob_t glob_result;
    int32_t glob_ret = glob_(full_pattern, 0, NULL, &glob_result);

    if (glob_ret == GLOB_NOSPACE) {
        globfree_(&glob_result);

        yyjson_alc error_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *error_doc = yyjson_mut_doc_new(&error_allocator);
        if (error_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_obj = yyjson_mut_obj(error_doc);
        if (error_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_val = yyjson_mut_str(error_doc, "Out of memory during glob");
        if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_code_val = yyjson_mut_str(error_doc, "OUT_OF_MEMORY");
        if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(error_doc, error_obj, "error", error_val);
        yyjson_mut_obj_add_val(error_doc, error_obj, "error_code", error_code_val);
        yyjson_mut_doc_set_root(error_doc, error_obj);

        char *error_json = yyjson_mut_write_opts(error_doc, 0, &error_allocator, NULL, NULL);
        if (error_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        printf("%s\n", error_json);
        return 0;
    } else if (glob_ret == GLOB_ABORTED) {
        globfree_(&glob_result);

        yyjson_alc error_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *error_doc = yyjson_mut_doc_new(&error_allocator);
        if (error_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_obj = yyjson_mut_obj(error_doc);
        if (error_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_val = yyjson_mut_str(error_doc, "Read error during glob");
        if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_code_val = yyjson_mut_str(error_doc, "READ_ERROR");
        if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(error_doc, error_obj, "error", error_val);
        yyjson_mut_obj_add_val(error_doc, error_obj, "error_code", error_code_val);
        yyjson_mut_doc_set_root(error_doc, error_obj);

        char *error_json = yyjson_mut_write_opts(error_doc, 0, &error_allocator, NULL, NULL);
        if (error_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        printf("%s\n", error_json);
        return 0;
    } else if (glob_ret != 0 && glob_ret != GLOB_NOMATCH) {
        globfree_(&glob_result);

        yyjson_alc error_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *error_doc = yyjson_mut_doc_new(&error_allocator);
        if (error_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_obj = yyjson_mut_obj(error_doc);
        if (error_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_val = yyjson_mut_str(error_doc, "Invalid glob pattern");
        if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_code_val = yyjson_mut_str(error_doc, "INVALID_PATTERN");
        if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(error_doc, error_obj, "error", error_val);
        yyjson_mut_obj_add_val(error_doc, error_obj, "error_code", error_code_val);
        yyjson_mut_doc_set_root(error_doc, error_obj);

        char *error_json = yyjson_mut_write_opts(error_doc, 0, &error_allocator, NULL, NULL);
        if (error_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        printf("%s\n", error_json);
        return 0;
    }

    // Build output string (one path per line, no trailing newline after last)
    size_t output_size = 0;
    if (glob_ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            output_size += strlen(glob_result.gl_pathv[i]);
            if (i < glob_result.gl_pathc - 1) {
                output_size += 1; // newline
            }
        }
    }

    char *output = talloc_array(ctx, char, (unsigned int)(output_size + 1));
    if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *pos = output;
    if (glob_ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            size_t len = strlen(glob_result.gl_pathv[i]);
            memcpy(pos, glob_result.gl_pathv[i], len);
            pos += len;
            if (i < glob_result.gl_pathc - 1) {
                *pos++ = '\n';
            }
        }
    }
    *pos = '\0';

    int32_t count = (glob_ret == 0) ? (int32_t)glob_result.gl_pathc : 0;
    globfree_(&glob_result);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, output);
    if (output_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *count_val = yyjson_mut_int(output_doc, count);
    if (count_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "count", count_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
