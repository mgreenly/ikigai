/**
 * @file response_error.c
 * @brief Google error response parsing
 */

#include "response.h"

#include "json_allocator.h"
#include "panic.h"
#include "wrapper_json.h"
#include "wrapper_talloc.h"

#include <assert.h>
#include <stdbool.h>

static ik_error_category_t map_http_status_to_category(int http_status)
{
    switch (http_status) {
        case 400:
            return IK_ERR_CAT_INVALID_ARG;
        case 401:
        case 403:
            return IK_ERR_CAT_AUTH;
        case 404:
            return IK_ERR_CAT_NOT_FOUND;
        case 429:
            return IK_ERR_CAT_RATE_LIMIT;
        case 500:
        case 502:
        case 503:
            return IK_ERR_CAT_SERVER;
        case 504:
            return IK_ERR_CAT_TIMEOUT;
        default:
            return IK_ERR_CAT_UNKNOWN;
    }
}

static bool try_extract_error_message(TALLOC_CTX *ctx, const char *json,
                                      size_t json_len, int http_status,
                                      char **out_message)
{
    if (json == NULL || json_len == 0) return false;

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)json, json_len, 0, &allocator, NULL);
    if (doc == NULL) return false;

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj == NULL) {
        yyjson_doc_free(doc);
        return false;
    }

    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
    const char *msg = yyjson_get_str(msg_val);
    if (msg == NULL) {
        yyjson_doc_free(doc);
        return false;
    }

    *out_message = talloc_asprintf_(ctx, "%d: %s", http_status, msg);
    yyjson_doc_free(doc);
    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return true;
}

/**
 * Parse Google error response
 */
res_t ik_google_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                            size_t json_len, ik_error_category_t *out_category,
                            char **out_message)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(out_category != NULL); // LCOV_EXCL_BR_LINE
    assert(out_message != NULL);  // LCOV_EXCL_BR_LINE

    *out_category = map_http_status_to_category(http_status);

    if (try_extract_error_message(ctx, json, json_len, http_status, out_message)) {
        return OK(NULL);
    }

    *out_message = talloc_asprintf_(ctx, "HTTP %d", http_status);
    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(NULL);
}
