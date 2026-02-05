#include "list.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

#include "shared/json_allocator.h"
#include "shared/panic.h"

#include "vendor/yyjson/yyjson.h"

// Create directory and all parent directories (like mkdir -p)
static int32_t mkdir_p(const char *path)
{
    char *tmp = strdup(path);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *p = NULL;
    size_t len = strlen(tmp);

    // Remove trailing slash
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    // Create directories recursively
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                free(tmp);
                return -1;
            }
            *p = '/';
        }
    }

    // Create final directory
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        free(tmp);
        return -1;
    }

    free(tmp);
    return 0;
}

// Read list.json and return array (or empty array if file doesn't exist)
static yyjson_mut_val *read_list(void *ctx, const char *file_path, yyjson_mut_doc *doc)
{
    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        // File doesn't exist - return empty array
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        if (arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return arr;
    }

    // Read file contents
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size == 0) {
        fclose(f);
        // Empty file - return empty array
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        if (arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return arr;
    }

    char *file_contents = talloc_array(ctx, char, (unsigned int)(file_size + 1));
    if (file_contents == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t read_size = fread(file_contents, 1, (size_t)file_size, f);
    file_contents[read_size] = '\0';
    fclose(f);

    // Parse JSON
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *read_doc = yyjson_read_opts(file_contents, read_size, 0, &allocator, NULL);
    if (read_doc == NULL) {
        // Invalid JSON - return empty array
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        if (arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return arr;
    }

    yyjson_val *root = yyjson_doc_get_root(read_doc);
    if (!yyjson_is_arr(root)) {
        // Not an array - return empty array
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        if (arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return arr;
    }

    // Convert immutable array to mutable array
    yyjson_mut_val *mut_arr = yyjson_val_mut_copy(doc, root);
    if (mut_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return mut_arr;
}

// Write list.json
static int32_t write_list(const char *file_path, yyjson_mut_val *arr, yyjson_mut_doc *doc)
{
    yyjson_mut_doc_set_root(doc, arr);

    yyjson_write_err err;
    char *json_str = yyjson_mut_write_opts(doc, YYJSON_WRITE_PRETTY, NULL, NULL, &err);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    FILE *f = fopen(file_path, "w");
    if (f == NULL) {
        free(json_str);
        return -1;
    }

    fprintf(f, "%s\n", json_str);
    fclose(f);
    free(json_str);

    return 0;
}

// Output JSON response
static void output_json(yyjson_mut_doc *doc, yyjson_mut_val *obj)
{
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);
    free(json_str);
}

int32_t list_execute(void *ctx, const char *operation, const char *item)
{
    // Get environment variables
    const char *agent_id = getenv("IKIGAI_AGENT_ID");
    const char *state_dir = getenv("IKIGAI_STATE_DIR");

    if (agent_id == NULL || agent_id[0] == '\0') {
        fprintf(stderr, "list: IKIGAI_AGENT_ID not set\n");
        return 1;
    }

    if (state_dir == NULL || state_dir[0] == '\0') {
        fprintf(stderr, "list: IKIGAI_STATE_DIR not set\n");
        return 1;
    }

    // Build file path: $IKIGAI_STATE_DIR/agents/<AGENT_ID>/list.json
    char *agent_dir = talloc_asprintf(ctx, "%s/agents/%s", state_dir, agent_id);
    if (agent_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *file_path = talloc_asprintf(ctx, "%s/list.json", agent_dir);
    if (file_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create allocator and document for output
    yyjson_alc out_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(&out_allocator);
    if (out_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Operations that don't modify the list
    if (strcmp(operation, "lpeek") == 0 || strcmp(operation, "rpeek") == 0 ||
        strcmp(operation, "list") == 0 || strcmp(operation, "count") == 0) {

        // Read list
        yyjson_alc list_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *list_doc = yyjson_mut_doc_new(&list_allocator);
        if (list_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *arr = read_list(ctx, file_path, list_doc);
        size_t count = yyjson_mut_arr_size(arr);

        if (strcmp(operation, "count") == 0) {
            // Return count
            yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
            if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, true);
            if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_val *count_val = yyjson_mut_uint(out_doc, count);
            if (count_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);
            yyjson_mut_obj_add_val(out_doc, obj, "count", count_val);

            output_json(out_doc, obj);
            return 0;
        }

        if (strcmp(operation, "list") == 0) {
            // Return all items
            yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
            if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, true);
            if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            // Copy array to output document
            yyjson_mut_val *items = yyjson_mut_arr(out_doc);
            if (items == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_val *item_val;
            yyjson_mut_arr_iter iter;
            yyjson_mut_arr_iter_init(arr, &iter);
            while ((item_val = yyjson_mut_arr_iter_next(&iter))) {
                const char *str = yyjson_mut_get_str(item_val);
                yyjson_mut_val *new_item = yyjson_mut_str(out_doc, str);
                if (new_item == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                yyjson_mut_arr_add_val(items, new_item);
            }

            yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);
            yyjson_mut_obj_add_val(out_doc, obj, "items", items);

            output_json(out_doc, obj);
            return 0;
        }

        // lpeek or rpeek
        if (count == 0) {
            // Empty list
            yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
            if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, false);
            if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);

            output_json(out_doc, obj);
            return 0;
        }

        // Get item
        yyjson_mut_val *peek_item = NULL;
        if (strcmp(operation, "lpeek") == 0) {
            peek_item = yyjson_mut_arr_get(arr, 0);
        } else {
            peek_item = yyjson_mut_arr_get(arr, count - 1);
        }

        const char *item_str = yyjson_mut_get_str(peek_item);

        yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
        if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, true);
        if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *item_val = yyjson_mut_str(out_doc, item_str);
        if (item_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);
        yyjson_mut_obj_add_val(out_doc, obj, "item", item_val);

        output_json(out_doc, obj);
        return 0;
    }

    // Operations that modify the list - need to create directory
    if (mkdir_p(agent_dir) != 0) {
        fprintf(stderr, "list: failed to create directory %s\n", agent_dir);
        return 1;
    }

    // Read list
    yyjson_alc list_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *list_doc = yyjson_mut_doc_new(&list_allocator);
    if (list_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *arr = read_list(ctx, file_path, list_doc);

    // Handle push operations
    if (strcmp(operation, "lpush") == 0 || strcmp(operation, "rpush") == 0) {
        if (item == NULL || item[0] == '\0') {
            fprintf(stderr, "list: item required for %s\n", operation);
            return 1;
        }

        yyjson_mut_val *new_item = yyjson_mut_str(list_doc, item);
        if (new_item == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (strcmp(operation, "lpush") == 0) {
            // Add to front
            yyjson_mut_arr_insert(arr, new_item, 0);
        } else {
            // Add to back
            yyjson_mut_arr_add_val(arr, new_item);
        }

        // Write list
        if (write_list(file_path, arr, list_doc) != 0) {
            fprintf(stderr, "list: failed to write %s\n", file_path);
            return 1;
        }

        // Return count
        size_t count = yyjson_mut_arr_size(arr);

        yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
        if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, true);
        if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *count_val = yyjson_mut_uint(out_doc, count);
        if (count_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);
        yyjson_mut_obj_add_val(out_doc, obj, "count", count_val);

        output_json(out_doc, obj);
        return 0;
    }

    // Handle pop operations
    if (strcmp(operation, "lpop") == 0 || strcmp(operation, "rpop") == 0) {
        size_t count = yyjson_mut_arr_size(arr);

        if (count == 0) {
            // Empty list
            yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
            if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, false);
            if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);

            output_json(out_doc, obj);
            return 0;
        }

        // Get item to return
        yyjson_mut_val *pop_item = NULL;
        if (strcmp(operation, "lpop") == 0) {
            pop_item = yyjson_mut_arr_get(arr, 0);
        } else {
            pop_item = yyjson_mut_arr_get(arr, count - 1);
        }

        const char *item_str = yyjson_mut_get_str(pop_item);

        // Copy item string before modifying array
        char *item_copy = talloc_strdup(ctx, item_str);
        if (item_copy == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // Remove item
        if (strcmp(operation, "lpop") == 0) {
            yyjson_mut_arr_remove(arr, 0);
        } else {
            yyjson_mut_arr_remove(arr, count - 1);
        }

        // Write list
        if (write_list(file_path, arr, list_doc) != 0) {
            fprintf(stderr, "list: failed to write %s\n", file_path);
            return 1;
        }

        // Return item
        yyjson_mut_val *obj = yyjson_mut_obj(out_doc);
        if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *ok_val = yyjson_mut_bool(out_doc, true);
        if (ok_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *item_val = yyjson_mut_str(out_doc, item_copy);
        if (item_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(out_doc, obj, "ok", ok_val);
        yyjson_mut_obj_add_val(out_doc, obj, "item", item_val);

        output_json(out_doc, obj);
        return 0;
    }

    // Unknown operation
    fprintf(stderr, "list: unknown operation %s\n", operation);
    return 1;
}
