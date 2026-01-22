#include "web_search_google.h"

#include "json_allocator.h"
#include "panic.h"
#include "schema.h"

#include "vendor/yyjson/yyjson.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

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
    if (input == NULL) PANIC("Out of memory");

    size_t bytes_read;
    while ((bytes_read = fread(input + total_read, 1, buffer_size - total_read, stdin)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) PANIC("Out of memory");
        }
    }

    if (total_read < buffer_size) {
        input[total_read] = '\0';
    } else {
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1));
        if (input == NULL) PANIC("Out of memory");
        input[total_read] = '\0';
    }

    if (total_read == 0) {
        fprintf(stderr, "web_search_google: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "web_search_google: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *query_val = yyjson_obj_get(root, "query");
    if (query_val == NULL || !yyjson_is_str(query_val)) {
        fprintf(stderr, "web_search_google: missing or invalid query field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *query = yyjson_get_str(query_val);

    int64_t num = 10;
    yyjson_val *num_val = yyjson_obj_get(root, "num");
    if (num_val != NULL && yyjson_is_int(num_val)) {
        num = yyjson_get_int(num_val);
    }

    int64_t start = 1;
    yyjson_val *start_val = yyjson_obj_get(root, "start");
    if (start_val != NULL && yyjson_is_int(start_val)) {
        start = yyjson_get_int(start_val);
    }

    yyjson_val *allowed_domains_val = yyjson_obj_get(root, "allowed_domains");
    yyjson_val *blocked_domains_val = yyjson_obj_get(root, "blocked_domains");

    size_t allowed_count = 0;
    size_t blocked_count = 0;
    if (allowed_domains_val != NULL && yyjson_is_arr(allowed_domains_val)) {
        allowed_count = yyjson_arr_size(allowed_domains_val);
    }
    if (blocked_domains_val != NULL && yyjson_is_arr(blocked_domains_val)) {
        blocked_count = yyjson_arr_size(blocked_domains_val);
    }

    web_search_google_params_t params = {
        .query = query,
        .num = num,
        .start = start,
        .allowed_domains_val = allowed_domains_val,
        .blocked_domains_val = blocked_domains_val,
        .allowed_count = allowed_count,
        .blocked_count = blocked_count
    };

    web_search_google_execute(ctx, &params);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
