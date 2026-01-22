#include "file_read.h"

#include "json_allocator.h"
#include "panic.h"
#include "wrapper_posix.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

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

int32_t file_read_execute(void *ctx,
                          const char *path,
                          bool has_offset,
                          int64_t offset,
                          bool has_limit,
                          int64_t limit)
{
    FILE *fp = fopen_(path, "r");
    if (fp == NULL) {
        char error_msg[512];
        if (errno == ENOENT) {
            snprintf(error_msg, sizeof(error_msg), "File not found: %s", path);
            output_error(ctx, error_msg, "FILE_NOT_FOUND");
        } else if (errno == EACCES) {
            snprintf(error_msg, sizeof(error_msg), "Permission denied: %s", path);
            output_error(ctx, error_msg, "PERMISSION_DENIED");
        } else {
            snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", path);
            output_error(ctx, error_msg, "OPEN_FAILED");
        }
        return 0;
    }

    char *content = NULL;
    size_t content_size = 0;

    if (!has_offset && !has_limit) {
        // Read entire file
        struct stat st;
        if (posix_stat_(path, &st) != 0) {
            fclose_(fp);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot get file size: %s", path);
            output_error(ctx, error_msg, "SIZE_FAILED");
            return 0;
        }

        content_size = (size_t)st.st_size;
        content = talloc_array(ctx, char, (unsigned int)(content_size + 1));
        if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        size_t read_bytes = fread_(content, 1, content_size, fp);
        if (read_bytes != content_size) {
            fclose_(fp);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Failed to read file: %s", path);
            output_error(ctx, error_msg, "READ_FAILED");
            return 0;
        }
        content[content_size] = '\0';
    } else {
        // Line-by-line reading with offset/limit
        char *line = NULL;
        size_t line_size = 0;
        int64_t current_line = 0;
        int64_t lines_read = 0;

        size_t output_buffer_size = 4096;
        content = talloc_array(ctx, char, (unsigned int)output_buffer_size);
        if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        content[0] = '\0';
        content_size = 0;

        while (getline(&line, &line_size, fp) != -1) {
            current_line++;

            // Skip lines before offset
            if (has_offset && current_line < offset) {
                continue;
            }

            // Stop if we've read enough lines
            if (has_limit && lines_read >= limit) {
                break;
            }

            // Append line to content
            size_t line_len = strlen(line);
            while (content_size + line_len + 1 > output_buffer_size) {
                output_buffer_size *= 2;
                content = talloc_realloc(ctx, content, char, (unsigned int)output_buffer_size);
                if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            }

            memcpy(content + content_size, line, line_len);
            content_size += line_len;
            content[content_size] = '\0';
            lines_read++;
        }

        free(line);
    }

    fclose_(fp);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, content);
    if (output_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
