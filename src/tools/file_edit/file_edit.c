#include "file_edit.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

#include "json_allocator.h"
#include "panic.h"
#include "wrapper.h"

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

int32_t file_edit_execute(void *ctx, const file_edit_params_t *params)
{
    const char *path = params->file_path;
    const char *old_string = params->old_string;
    const char *new_string = params->new_string;
    bool replace_all = params->replace_all;

    size_t old_len = strlen(old_string);
    size_t new_len = strlen(new_string);

    // Validate old_string is not empty
    if (old_len == 0) {
        output_error(ctx, "old_string cannot be empty", "INVALID_ARG");
        return 0;
    }

    // Validate old_string != new_string
    if (strcmp(old_string, new_string) == 0) {
        output_error(ctx, "old_string and new_string are identical", "INVALID_ARG");
        return 0;
    }

    // Read file contents
    FILE *fp = fopen_(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "File not found: %s", path);
            output_error(ctx, error_msg, "FILE_NOT_FOUND");
        } else if (errno == EACCES) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Permission denied: %s", path);
            output_error(ctx, error_msg, "PERMISSION_DENIED");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", path);
            output_error(ctx, error_msg, "OPEN_FAILED");
        }
        return 0;
    }

    struct stat st;
    if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Cannot get file size: %s", path);
        output_error(ctx, error_msg, "SIZE_FAILED");
        return 0;
    }

    size_t file_size = (size_t)st.st_size;
    char *content = talloc_array(ctx, char, (unsigned int)(file_size + 1));
    if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t read_bytes = fread(content, 1, file_size, fp);
    if (read_bytes != file_size) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to read file: %s", path);
        output_error(ctx, error_msg, "READ_FAILED");
        return 0;
    }
    content[file_size] = '\0';
    fclose(fp);

    // Count occurrences of old_string
    int32_t count = 0;
    const char *pos = content;
    while ((pos = strstr(pos, old_string)) != NULL) {
        count++;
        pos += old_len;
    }

    // Validate match count
    if (!replace_all && count != 1) {
        if (count == 0) {
            output_error(ctx, "String not found in file", "NOT_FOUND");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "String found %d times, use replace_all to replace all", count);
            output_error(ctx, error_msg, "NOT_UNIQUE");
        }
        return 0;
    }

    // Build new content with replacements
    size_t new_content_size = file_size + (size_t)(count) * (new_len - old_len);
    char *new_content = talloc_array(ctx, char, (unsigned int)(new_content_size + 1));
    if (new_content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    const char *src = content;
    char *dst = new_content;
    int32_t replacements = 0;

    while (*src) {
        const char *match = strstr(src, old_string);
        if (match == NULL) {
            // Copy rest of string (including null terminator)
            strcpy(dst, src);
            dst += strlen(src);
            break;
        }

        // Copy content before match
        size_t prefix_len = (size_t)(match - src);
        memcpy(dst, src, prefix_len);
        dst += prefix_len;

        // Copy new_string
        memcpy(dst, new_string, new_len);
        dst += new_len;

        // Advance source past old_string
        src = match + old_len;
        replacements++;
    }

    // Ensure null termination (if loop ended without copying remainder)
    if (*src == '\0') {
        *dst = '\0';
    }

    // Write new content back to file
    fp = fopen_(path, "w");
    if (fp == NULL) {
        if (errno == EACCES) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Permission denied: %s", path);
            output_error(ctx, error_msg, "PERMISSION_DENIED");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", path);
            output_error(ctx, error_msg, "OPEN_FAILED");
        }
        return 0;
    }

    size_t written = fwrite_(new_content, 1, strlen(new_content), fp);
    if (written != strlen(new_content)) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to write file: %s", path);
        output_error(ctx, error_msg, "WRITE_FAILED");
        return 0;
    }

    fclose(fp);

    // Extract basename for success message
    char *path_copy = talloc_strdup(ctx, path);
    if (path_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *filename = basename(path_copy);

    // Build success message
    char success_msg[512];
    snprintf(success_msg, sizeof(success_msg), "Replaced %d occurrence%s in %s",
             replacements, replacements == 1 ? "" : "s", filename);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, success_msg);
    if (output_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *replacements_val = yyjson_mut_int(output_doc, replacements);
    if (replacements_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "replacements", replacements_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
