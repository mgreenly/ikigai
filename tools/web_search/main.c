#include "web_search.h"

#include "json_allocator.h"
#include "panic.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

static const char *SCHEMA_JSON =
    "{\"name\": \"web_search\","
    "\"description\": \"Search the web using Brave Search API and use the results to inform responses. Provides up-to-date information for current events and recent data. Returns search result information formatted as search result blocks, including links as markdown hyperlinks.\","
    "\"parameters\": {\"type\": \"object\",\"properties\": {"
    "\"query\": {\"type\": \"string\",\"description\": \"The search query to use\",\"minLength\": 2},"
    "\"count\": {\"type\": \"integer\",\"description\": \"Number of results to return (1-20)\",\"minimum\": 1,\"maximum\": 20,\"default\": 10},"
    "\"offset\": {\"type\": \"integer\",\"description\": \"Result offset for pagination\",\"minimum\": 0,\"default\": 0},"
    "\"allowed_domains\": {\"type\": \"array\",\"items\": {\"type\": \"string\"},\"description\": \"Only include search results from these domains\"},"
    "\"blocked_domains\": {\"type\": \"array\",\"items\": {\"type\": \"string\"},\"description\": \"Never include search results from these domains\"}"
    "},\"required\": [\"query\"]}}\n";

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
        fprintf(stderr, "web-search-brave: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "web-search-brave: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *query = yyjson_obj_get(root, "query");
    if (query == NULL || !yyjson_is_str(query)) {
        fprintf(stderr, "web-search-brave: missing or invalid query\n");
        talloc_free(ctx);
        return 1;
    }

    const char *query_str = yyjson_get_str(query);
    int32_t count = 10;
    int32_t offset = 0;

    yyjson_val *count_val = yyjson_obj_get(root, "count");
    if (count_val != NULL && yyjson_is_int(count_val)) {
        count = (int32_t)yyjson_get_int(count_val);
    }

    yyjson_val *offset_val = yyjson_obj_get(root, "offset");
    if (offset_val != NULL && yyjson_is_int(offset_val)) {
        offset = (int32_t)yyjson_get_int(offset_val);
    }

    yyjson_val *allowed_domains = yyjson_obj_get(root, "allowed_domains");
    yyjson_val *blocked_domains = yyjson_obj_get(root, "blocked_domains");

    web_search_params_t params = {
        .query = query_str,
        .count = count,
        .offset = offset,
        .allowed_domains = allowed_domains,
        .blocked_domains = blocked_domains
    };

    web_search_execute(ctx, &params);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
