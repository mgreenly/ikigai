#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "json_allocator.h"

#include "vendor/yyjson/yyjson.h"

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) {
        exit(1);
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) {
        exit(1);
    }

    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) {
        exit(1);
    }

    printf("%s\n", json_str);
    free(json_str);
}

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"file_write\",\n");
        printf("  \"description\": \"Write content to a file (creates or overwrites)\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"file_path\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Absolute or relative path to file\"\n");
        printf("      },\n");
        printf("      \"content\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Content to write to file\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"file_path\", \"content\"]\n");
        printf("  }\n");
        printf("}\n");
        talloc_free(ctx);
        return 0;
    }

    // Read all of stdin into buffer
    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *input = talloc_array(ctx, char, (unsigned int)buffer_size);
    if (input == NULL) {
        talloc_free(ctx);
        return 1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(input + total_read, 1, buffer_size - total_read, stdin)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) {
                talloc_free(ctx);
                return 1;
            }
        }
    }

    if (total_read < buffer_size) {
        input[total_read] = '\0';
    } else {
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1));
        if (input == NULL) {
            talloc_free(ctx);
            return 1;
        }
        input[total_read] = '\0';
    }

    if (total_read == 0) {
        fprintf(stderr, "file_write: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "file_write: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *file_path = yyjson_obj_get(root, "file_path");
    if (file_path == NULL || !yyjson_is_str(file_path)) {
        fprintf(stderr, "file_write: missing or invalid file_path field\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *content_val = yyjson_obj_get(root, "content");
    if (content_val == NULL || !yyjson_is_str(content_val)) {
        fprintf(stderr, "file_write: missing or invalid content field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *path = yyjson_get_str(file_path);
    const char *content = yyjson_get_str(content_val);
    size_t content_len = yyjson_get_len(content_val);

    // Open file for writing
    FILE *fp = fopen(path, "w");
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
        talloc_free(ctx);
        return 0;
    }

    // Write content to file
    size_t written = fwrite(content, 1, content_len, fp);
    if (written != content_len) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to write file: %s", path);
        output_error(ctx, error_msg, "WRITE_FAILED");
        talloc_free(ctx);
        return 0;
    }

    fclose(fp);

    // Extract basename for success message
    char *path_copy = talloc_strdup(ctx, path);
    if (path_copy == NULL) {
        talloc_free(ctx);
        return 1;
    }
    char *filename = basename(path_copy);

    // Build success message
    char success_msg[512];
    snprintf(success_msg, sizeof(success_msg), "Wrote %zu bytes to %s", content_len, filename);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, success_msg);
    if (output_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *bytes_val = yyjson_mut_uint(output_doc, (uint64_t)content_len);
    if (bytes_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "bytes", bytes_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write(output_doc, 0, NULL);
    if (json_str == NULL) {
        talloc_free(ctx);
        return 1;
    }

    printf("%s\n", json_str);
    free(json_str);

    talloc_free(ctx);
    return 0;
}
