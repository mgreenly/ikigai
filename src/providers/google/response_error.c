/**
 * @file response_error.c
 * @brief Google error response parsing
 */

#include "response.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>

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

    // Map HTTP status to category
    switch (http_status) {
        case 400:
            *out_category = IK_ERR_CAT_INVALID_ARG;
            break;
        case 401:
        case 403:
            *out_category = IK_ERR_CAT_AUTH;
            break;
        case 404:
            *out_category = IK_ERR_CAT_NOT_FOUND;
            break;
        case 429:
            *out_category = IK_ERR_CAT_RATE_LIMIT;
            break;
        case 500:
        case 502:
        case 503:
            *out_category = IK_ERR_CAT_SERVER;
            break;
        case 504:
            *out_category = IK_ERR_CAT_TIMEOUT;
            break;
        default:
            *out_category = IK_ERR_CAT_UNKNOWN;
            break;
    }

    // Try to extract error message from JSON
    if (json != NULL && json_len > 0) {
        yyjson_alc allocator = ik_make_talloc_allocator(ctx);
        // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
        yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)json, json_len, 0, &allocator, NULL);
        if (doc != NULL) {
            yyjson_val *root = yyjson_doc_get_root(doc);
            if (yyjson_is_obj(root)) {
                yyjson_val *error_obj = yyjson_obj_get(root, "error");
                if (error_obj != NULL) {
                    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
                    const char *msg = yyjson_get_str(msg_val);
                    if (msg != NULL) {
                        *out_message = talloc_asprintf(ctx, "%d: %s", http_status, msg);
                        yyjson_doc_free(doc);
                        if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                        return OK(NULL);
                    }
                }
            }
            yyjson_doc_free(doc);
        }
    }

    // Fallback to generic message
    *out_message = talloc_asprintf(ctx, "HTTP %d", http_status);
    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(NULL);
}
