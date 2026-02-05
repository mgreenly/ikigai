#include "glob_tool.h"

#include "shared/json_allocator.h"
#include "shared/panic.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

/* LCOV_EXCL_START */
int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"glob\",\n");
        printf("  \"description\": \"Find files matching a glob pattern\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"pattern\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Glob pattern (e.g., '*.txt', 'src/**/*.c')\"\n");
        printf("      },\n");
        printf("      \"path\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Directory to search in (default: current directory)\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"pattern\"]\n");
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
        fprintf(stderr, "glob: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "glob: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *pattern_val = yyjson_obj_get(root, "pattern");
    if (pattern_val == NULL || !yyjson_is_str(pattern_val)) {
        fprintf(stderr, "glob: missing or invalid pattern field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *pattern = yyjson_get_str(pattern_val);

    // Get optional path
    yyjson_val *path_val = yyjson_obj_get(root, "path");
    const char *path = NULL;
    if (path_val != NULL && yyjson_is_str(path_val)) {
        path = yyjson_get_str(path_val);
    }

    // Call glob_execute with parsed parameters
    int32_t result = glob_execute(ctx, pattern, path);

    talloc_free(ctx);
    return result;
}

/* LCOV_EXCL_STOP */
