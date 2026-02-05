/**
 * @file error.c
 * @brief Google error handling implementation
 */

#include "apps/ikigai/providers/google/error.h"

#include "shared/panic.h"
#include "shared/wrapper.h"

#include "vendor/yyjson/yyjson.h"
#include <string.h>


#include "shared/poison.h"
/**
 * Map HTTP status code to error category
 */
static ik_error_category_t status_to_category(int32_t status)
{
    switch (status) {
        case 403:
            return IK_ERR_CAT_AUTH;
        case 429:
            return IK_ERR_CAT_RATE_LIMIT;
        case 400:
            return IK_ERR_CAT_INVALID_ARG;
        case 404:
            return IK_ERR_CAT_NOT_FOUND;
        case 500:
        case 503:
            return IK_ERR_CAT_SERVER;
        case 504:
            return IK_ERR_CAT_TIMEOUT;
        default:
            return IK_ERR_CAT_UNKNOWN;
    }
}

res_t ik_google_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body,
                             ik_error_category_t *out_category)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(body != NULL); // LCOV_EXCL_BR_LINE
    assert(out_category != NULL); // LCOV_EXCL_BR_LINE

    // Map status to category
    *out_category = status_to_category(status);

    // Parse JSON body to extract error details
    yyjson_doc *doc = yyjson_read(body, strlen(body), 0);
    if (doc == NULL) {
        return ERR(ctx, PARSE, "Failed to parse Google error response");
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL) {
        yyjson_doc_free(doc);
        return ERR(ctx, PARSE, "Google error response has no root");
    }

    // Extract error.status and error.message (optional - just for validation)
    yyjson_val *error_obj = yyjson_obj_get_(root, "error");
    if (error_obj != NULL) {
        // Validate that error object has the expected fields
        yyjson_val *status_val = yyjson_obj_get_(error_obj, "status");
        yyjson_val *msg_val = yyjson_obj_get_(error_obj, "message");

        // Fields exist but we don't need to do anything with them
        // The category is determined by HTTP status, not error.status
        (void)status_val;
        (void)msg_val;
    }

    yyjson_doc_free(doc);
    return OK(NULL);
}

int32_t ik_google_get_retry_after(const char *body)
{
    if (body == NULL) {
        return -1;
    }

    // Parse JSON body
    yyjson_doc *doc = yyjson_read(body, strlen(body), 0);
    if (doc == NULL) {
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL) {
        yyjson_doc_free(doc);
        return -1;
    }

    // Extract retryDelay field (top-level)
    yyjson_val *retry_delay = yyjson_obj_get_(root, "retryDelay");
    if (retry_delay == NULL || !yyjson_is_str(retry_delay)) {
        yyjson_doc_free(doc);
        return -1;
    }

    // Parse delay string (format: "60s")
    const char *delay_str = yyjson_get_str_(retry_delay);
    if (delay_str == NULL) {
        yyjson_doc_free(doc);
        return -1;
    }

    // Extract integer part (skip trailing 's')
    char *endptr;
    long parsed = strtol(delay_str, &endptr, 10);

    // Validate: must have parsed some digits and result is positive
    if (endptr == delay_str || parsed <= 0) {
        yyjson_doc_free(doc);
        return -1;
    }

    yyjson_doc_free(doc);
    return (int32_t)parsed;
}
