#include "file_read.h"

#include "json_allocator.h"
#include "panic.h"

#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

/* LCOV_EXCL_START */
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
    if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t bytes_read;
    while ((bytes_read = fread(input + total_read, 1, buffer_size - total_read, stdin)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
    }

    if (total_read < buffer_size) {
        input[total_read] = '\0';
    } else {
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1));
        if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
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

    // Execute file read (outputs JSON directly)
    file_read_execute(ctx, path, has_offset, offset, has_limit, limit);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
