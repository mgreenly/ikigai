/**
 * @file error.c
 * @brief Anthropic error handling implementation
 */

#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <ctype.h>
#include <string.h>

/**
 * Map HTTP status code to error category
 */
static ik_error_category_t status_to_category(int32_t status)
{
    switch (status) {
        case 401:
        case 403:
            return IK_ERR_CAT_AUTH;
        case 429:
            return IK_ERR_CAT_RATE_LIMIT;
        case 400:
            return IK_ERR_CAT_INVALID_ARG;
        case 404:
            return IK_ERR_CAT_NOT_FOUND;
        case 500:
        case 529:
            return IK_ERR_CAT_SERVER;
        default:
            return IK_ERR_CAT_UNKNOWN;
    }
}

res_t ik_anthropic_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body,
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
        return ERR(ctx, PARSE, "Failed to parse Anthropic error response");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL) { // LCOV_EXCL_LINE - defensive: yyjson always sets root if parse succeeds
        yyjson_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Anthropic error response has no root"); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    // Extract error.type and error.message (optional - just for validation)
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        // Validate that error object has the expected fields
        yyjson_val *type_val = yyjson_obj_get(error_obj, "type");
        yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");

        // Fields exist but we don't need to do anything with them
        // The category is determined by HTTP status, not error.type
        (void)type_val;
        (void)msg_val;
    }

    yyjson_doc_free(doc);
    return OK(NULL);
}

int32_t ik_anthropic_get_retry_after(const char **headers)
{
    if (headers == NULL) {
        return -1;
    }

    // Scan headers for "retry-after: N" (case-insensitive)
    for (size_t i = 0; headers[i] != NULL; i++) {
        const char *header = headers[i];

        // Check if header starts with "retry-after:" (case-insensitive)
        if (strncasecmp(header, "retry-after:", 12) == 0) {
            // Skip "retry-after:" prefix and whitespace
            const char *value = header + 12;
            while (*value == ' ' || *value == '\t') {
                value++;
            }

            // Parse integer value
            char *endptr;
            long parsed = strtol(value, &endptr, 10);

            // Validate: must have parsed some digits and result is positive
            if (endptr != value && parsed > 0) {
                return (int32_t)parsed;
            }
        }
    }

    return -1;
}
