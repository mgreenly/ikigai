#include "web_fetch.h"

#include "shared/json_allocator.h"

#include "vendor/yyjson/yyjson.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static const char *SCHEMA_JSON =
    "{\"name\":\"web_fetch\","
    "\"description\":\"Fetches content from a specified URL and returns it as markdown. Converts HTML to markdown using libxml2. Supports pagination via offset and limit parameters similar to file_read.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"format\":\"uri\",\"description\":\"The URL to fetch content from\"},"
    "\"offset\":{\"type\":\"integer\",\"description\":\"Line number to start reading from (1-based)\",\"minimum\":1},"
    "\"limit\":{\"type\":\"integer\",\"description\":\"Maximum number of lines to return\",\"minimum\":1}"
    "},\"required\":[\"url\"]}}\n";

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
        fprintf(stderr, "web_fetch: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "web_fetch: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *url_val = yyjson_obj_get(root, "url");
    if (url_val == NULL || !yyjson_is_str(url_val)) {
        fprintf(stderr, "web_fetch: missing or invalid url field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *url = yyjson_get_str(url_val);

    yyjson_val *offset_val = yyjson_obj_get(root, "offset");
    yyjson_val *limit_val = yyjson_obj_get(root, "limit");

    web_fetch_params_t params = {0};
    params.url = url;
    params.offset = 0;
    params.limit = 0;
    params.has_offset = false;
    params.has_limit = false;

    if (offset_val != NULL && yyjson_is_int(offset_val)) {
        params.offset = yyjson_get_int(offset_val);
        params.has_offset = true;
    }

    if (limit_val != NULL && yyjson_is_int(limit_val)) {
        params.limit = yyjson_get_int(limit_val);
        params.has_limit = true;
    }

    web_fetch_execute(ctx, &params);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
