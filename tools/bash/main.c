#include "bash.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "json_allocator.h"
#include "panic.h"

#include "vendor/yyjson/yyjson.h"

/* LCOV_EXCL_START */
int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"bash\",\n");
        printf("  \"description\": \"Execute a shell command and return output\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"command\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Shell command to execute\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"command\"]\n");
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

        // If buffer is full, grow it
        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
    }

    // Null-terminate the input
    if (total_read < buffer_size) {
        input[total_read] = '\0';
    } else {
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1));
        if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        input[total_read] = '\0';
    }

    // Check for empty input
    if (total_read == 0) {
        fprintf(stderr, "bash: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "bash: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    // Check for required "command" field
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *command = yyjson_obj_get(root, "command");
    if (command == NULL) {
        fprintf(stderr, "bash: missing command field\n");
        talloc_free(ctx);
        return 1;
    }

    // Check that "command" is a string
    if (!yyjson_is_str(command)) {
        fprintf(stderr, "bash: command must be a string\n");
        talloc_free(ctx);
        return 1;
    }

    const char *cmd_str = yyjson_get_str(command);
    int32_t result = bash_execute(ctx, cmd_str);
    talloc_free(ctx);
    return result;
}

/* LCOV_EXCL_STOP */
