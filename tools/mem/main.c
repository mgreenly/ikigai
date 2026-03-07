#include "mem.h"

#include "shared/json_allocator.h"

#include "vendor/yyjson/yyjson.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static const char *SCHEMA_JSON =
    "{\"name\":\"mem\","
    "\"description\":\"Persistent document memory. Store, retrieve, list, and delete documents that "
    "survive across conversations.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"action\":{\"type\":\"string\",\"enum\":[\"create\",\"get\",\"list\",\"delete\"],"
    "\"description\":\"The operation to perform\"},"
    "\"id\":{\"type\":\"string\",\"description\":\"Document ID. Required for get and delete.\"},"
    "\"body\":{\"type\":\"string\",\"description\":\"Document content. Required for create.\"},"
    "\"title\":{\"type\":\"string\","
    "\"description\":\"Short label for the document. Optional, used with create.\"}"
    "},\"required\":[\"action\"]}}\n";

/* LCOV_EXCL_START */
int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("%s", SCHEMA_JSON);
        talloc_free(ctx);
        return 0;
    }

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
        fprintf(stderr, "mem: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "mem: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *action_val = yyjson_obj_get(root, "action");
    if (action_val == NULL || !yyjson_is_str(action_val)) {
        fprintf(stderr, "mem: missing or invalid action field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *action_str = yyjson_get_str(action_val);

    mem_params_t params = {0};

    if (strcmp(action_str, "create") == 0) {
        params.action = MEM_ACTION_CREATE;
    } else if (strcmp(action_str, "get") == 0) {
        params.action = MEM_ACTION_GET;
    } else if (strcmp(action_str, "list") == 0) {
        params.action = MEM_ACTION_LIST;
    } else if (strcmp(action_str, "delete") == 0) {
        params.action = MEM_ACTION_DELETE;
    } else {
        fprintf(stderr, "mem: unknown action: %s\n", action_str);
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *id_val = yyjson_obj_get(root, "id");
    if (id_val != NULL && yyjson_is_str(id_val)) {
        params.id = yyjson_get_str(id_val);
    }

    yyjson_val *body_val = yyjson_obj_get(root, "body");
    if (body_val != NULL && yyjson_is_str(body_val)) {
        params.body = yyjson_get_str(body_val);
    }

    yyjson_val *title_val = yyjson_obj_get(root, "title");
    if (title_val != NULL && yyjson_is_str(title_val)) {
        params.title = yyjson_get_str(title_val);
    }

    mem_execute(ctx, &params);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
