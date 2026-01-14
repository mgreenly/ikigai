#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
        printf("  \"name\": \"file_read\",\n");
        printf("  \"description\": \"Read contents of a file\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"file_path\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Absolute or relative path to file\"\n");
        printf("      },\n");
        printf("      \"offset\": {\n");
        printf("        \"type\": \"integer\",\n");
        printf("        \"description\": \"Line number to start reading from (1-based)\"\n");
        printf("      },\n");
        printf("      \"limit\": {\n");
        printf("        \"type\": \"integer\",\n");
        printf("        \"description\": \"Number of lines to read\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"file_path\"]\n");
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
        fprintf(stderr, "file_read: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "file_read: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *file_path = yyjson_obj_get(root, "file_path");
    if (file_path == NULL || !yyjson_is_str(file_path)) {
        fprintf(stderr, "file_read: missing or invalid file_path field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *path = yyjson_get_str(file_path);

    // Get optional offset and limit
    yyjson_val *offset_val = yyjson_obj_get(root, "offset");
    yyjson_val *limit_val = yyjson_obj_get(root, "limit");

    int64_t offset = 0;
    int64_t limit = 0;
    bool has_offset = false;
    bool has_limit = false;

    if (offset_val != NULL && yyjson_is_int(offset_val)) {
        offset = yyjson_get_int(offset_val);
        has_offset = true;
    }

    if (limit_val != NULL && yyjson_is_int(limit_val)) {
        limit = yyjson_get_int(limit_val);
        has_limit = true;
    }

    // Open file
    FILE *fp = fopen(path, "r");
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
        talloc_free(ctx);
        return 0;
    }

    // Read file contents
    char *content = NULL;
    size_t content_size = 0;

    if (!has_offset && !has_limit) {
        // Read entire file
        struct stat st;
        if (fstat(fileno(fp), &st) != 0) {
            fclose(fp);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot get file size: %s", path);
            output_error(ctx, error_msg, "SIZE_FAILED");
            talloc_free(ctx);
            return 0;
        }

        content_size = (size_t)st.st_size;
        content = talloc_array(ctx, char, (unsigned int)(content_size + 1));
        if (content == NULL) {
            fclose(fp);
            talloc_free(ctx);
            return 1;
        }

        size_t read_bytes = fread(content, 1, content_size, fp);
        if (read_bytes != content_size) {
            fclose(fp);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Failed to read file: %s", path);
            output_error(ctx, error_msg, "READ_FAILED");
            talloc_free(ctx);
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
        if (content == NULL) {
            fclose(fp);
            talloc_free(ctx);
            return 1;
        }
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
                if (content == NULL) {
                    free(line);
                    fclose(fp);
                    talloc_free(ctx);
                    return 1;
                }
            }

            memcpy(content + content_size, line, line_len);
            content_size += line_len;
            content[content_size] = '\0';
            lines_read++;
        }

        free(line);
    }

    fclose(fp);

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

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, content);
    if (output_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
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
