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
    "\"description\":\"Persistent document memory. Store, retrieve, list, delete, and update documents that "
    "survive across conversations.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"action\":{\"type\":\"string\","
    "\"enum\":[\"create\",\"get\",\"list\",\"delete\",\"update\",\"revisions\",\"revision_get\"],"
    "\"description\":\"The operation to perform\"},"
    "\"path\":{\"type\":\"string\","
    "\"description\":\"Document path identifier. Required for create, get, and delete.\"},"
    "\"body\":{\"type\":\"string\",\"description\":\"Document content. Required for create and update.\"},"
    "\"scope\":{\"type\":\"string\",\"enum\":[\"global\",\"default\"],"
    "\"description\":\"Document scope. Use global for cross-agent documents, default for "
    "agent/project-scoped documents. Defaults to default.\"},"
    "\"title\":{\"type\":\"string\","
    "\"description\":\"New title for the document. Used with update to rename a document.\"},"
    "\"search\":{\"type\":\"string\","
    "\"description\":\"Full-text search query for list action. Results ordered by relevance.\"},"
    "\"limit\":{\"type\":\"integer\","
    "\"description\":\"Maximum number of documents to return for list action. Default 50, max 100.\"},"
    "\"offset\":{\"type\":\"integer\","
    "\"description\":\"Pagination offset for list action.\"},"
    "\"revision_id\":{\"type\":\"string\","
    "\"description\":\"Revision identifier. Required for revision_get action.\"}"
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
    } else if (strcmp(action_str, "update") == 0) {
        params.action = MEM_ACTION_UPDATE;
    } else if (strcmp(action_str, "revisions") == 0) {
        params.action = MEM_ACTION_REVISIONS;
    } else if (strcmp(action_str, "revision_get") == 0) {
        params.action = MEM_ACTION_REVISION_GET;
    } else {
        fprintf(stderr, "mem: unknown action: %s\n", action_str);
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *path_val = yyjson_obj_get(root, "path");
    if (path_val != NULL && yyjson_is_str(path_val)) {
        params.path = yyjson_get_str(path_val);
    }

    yyjson_val *body_val = yyjson_obj_get(root, "body");
    if (body_val != NULL && yyjson_is_str(body_val)) {
        params.body = yyjson_get_str(body_val);
    }

    yyjson_val *scope_val = yyjson_obj_get(root, "scope");
    if (scope_val != NULL && yyjson_is_str(scope_val)) {
        const char *scope_str = yyjson_get_str(scope_val);
        if (strcmp(scope_str, "global") == 0) {
            params.scope = MEM_SCOPE_GLOBAL;
        } else {
            params.scope = MEM_SCOPE_DEFAULT;
        }
    }

    yyjson_val *title_val = yyjson_obj_get(root, "title");
    if (title_val != NULL && yyjson_is_str(title_val)) {
        params.title = yyjson_get_str(title_val);
    }

    yyjson_val *search_val = yyjson_obj_get(root, "search");
    if (search_val != NULL && yyjson_is_str(search_val)) {
        params.search = yyjson_get_str(search_val);
    }

    yyjson_val *revision_id_val = yyjson_obj_get(root, "revision_id");
    if (revision_id_val != NULL && yyjson_is_str(revision_id_val)) {
        params.revision_id = yyjson_get_str(revision_id_val);
    }

    yyjson_val *limit_val = yyjson_obj_get(root, "limit");
    if (limit_val != NULL && yyjson_is_int(limit_val)) {
        params.limit = (int32_t)yyjson_get_int(limit_val);
    }

    yyjson_val *offset_val = yyjson_obj_get(root, "offset");
    if (offset_val != NULL && yyjson_is_int(offset_val)) {
        params.offset = (int32_t)yyjson_get_int(offset_val);
    }

    mem_execute(ctx, &params);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
