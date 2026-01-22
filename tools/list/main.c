#include "list.h"

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
        printf("  \"name\": \"list\",\n");
        printf("  \"description\": \"Redis-style deque operations for task lists\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"operation\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Operation: lpush, rpush, lpop, rpop, lpeek, rpeek, list, count\",\n");
        printf("        \"enum\": [\"lpush\", \"rpush\", \"lpop\", \"rpop\", \"lpeek\", \"rpeek\", \"list\", \"count\"]\n");
        printf("      },\n");
        printf("      \"item\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Item to push (required for lpush/rpush)\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"operation\"]\n");
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
        fprintf(stderr, "list: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "list: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    // Check for required "operation" field
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *operation = yyjson_obj_get(root, "operation");
    if (operation == NULL) {
        fprintf(stderr, "list: missing operation field\n");
        talloc_free(ctx);
        return 1;
    }

    // Check that "operation" is a string
    if (!yyjson_is_str(operation)) {
        fprintf(stderr, "list: operation must be a string\n");
        talloc_free(ctx);
        return 1;
    }

    const char *op_str = yyjson_get_str(operation);

    // Get optional "item" field
    const char *item_str = NULL;
    yyjson_val *item = yyjson_obj_get(root, "item");
    if (item != NULL) {
        if (!yyjson_is_str(item)) {
            fprintf(stderr, "list: item must be a string\n");
            talloc_free(ctx);
            return 1;
        }
        item_str = yyjson_get_str(item);
    }

    int32_t result = list_execute(ctx, op_str, item_str);
    talloc_free(ctx);
    return result;
}

/* LCOV_EXCL_STOP */
