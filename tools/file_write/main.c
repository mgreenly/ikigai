#include "file_write_logic.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/json_allocator.h"
#include "shared/panic.h"

#include "vendor/yyjson/yyjson.h"

/* LCOV_EXCL_START */
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

    int32_t result = do_file_write(ctx, path, content, content_len);
    talloc_free(ctx);
    return result;
}

/* LCOV_EXCL_STOP */
