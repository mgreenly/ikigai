/**
 * @file error.c
 * @brief OpenAI error handling implementation
 */

#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/**
 * Map HTTP status code to error category (default mapping)
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
        case 502:
        case 503:
            return IK_ERR_CAT_SERVER;
        default:
            return IK_ERR_CAT_UNKNOWN;
    }
}

/**
 * Check if error code or type indicates content filter
 */
static bool is_content_filter(const char *str)
{
    if (str == NULL) {
        return false;
    }
    return strstr(str, "content_filter") != NULL;
}

res_t ik_openai_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body,
                              ik_error_category_t *out_category)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(body != NULL);  // LCOV_EXCL_BR_LINE
    assert(out_category != NULL);  // LCOV_EXCL_BR_LINE

    // Default mapping from HTTP status
    *out_category = status_to_category(status);

    // Parse JSON body to extract error details
    yyjson_doc *doc = yyjson_read(body, strlen(body), 0);
    if (doc == NULL) {
        return ERR(ctx, PARSE, "Failed to parse OpenAI error response");
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL) { // LCOV_EXCL_LINE - defensive: yyjson always sets root if parse succeeds
        yyjson_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "OpenAI error response has no root"); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    // Extract error object
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL && yyjson_is_obj(error_obj)) {
        // Extract error.code and error.type
        yyjson_val *code_val = yyjson_obj_get(error_obj, "code");
        yyjson_val *type_val = yyjson_obj_get(error_obj, "type");

        const char *code = yyjson_is_str(code_val) ? yyjson_get_str(code_val) : NULL;
        const char *type = yyjson_is_str(type_val) ? yyjson_get_str(type_val) : NULL;

        // Check for content filter
        if (is_content_filter(code) || is_content_filter(type)) {
            *out_category = IK_ERR_CAT_CONTENT_FILTER;
        }
        // Refine category based on error code (takes precedence over status)
        else if (code != NULL) {
            if (strcmp(code, "invalid_api_key") == 0 || strcmp(code, "invalid_org") == 0) {
                *out_category = IK_ERR_CAT_AUTH;
            } else if (strcmp(code, "rate_limit_exceeded") == 0 || strcmp(code, "quota_exceeded") == 0) {
                *out_category = IK_ERR_CAT_RATE_LIMIT;
            } else if (strcmp(code, "model_not_found") == 0) {
                *out_category = IK_ERR_CAT_NOT_FOUND;
            }
        }
    }

    yyjson_doc_free(doc);
    return OK(NULL);
}

/**
 * Parse duration string in format "6m0s" or "30s"
 *
 * @param duration Duration string
 * @return         Seconds, or -1 if parse fails
 */
static int32_t parse_duration(const char *duration)
{
    if (duration == NULL) { // LCOV_EXCL_LINE - defensive: called only after extracting from header
        return -1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    int32_t total_seconds = 0;
    const char *p = duration;

    while (*p != '\0') {
        // Parse number
        char *endptr;
        long value = strtol(p, &endptr, 10);

        if (endptr == p) {
            // No digits found
            return -1;
        }

        // Check unit
        char unit = *endptr;
        if (unit == 'm') {
            total_seconds += (int32_t)value * 60;
        } else if (unit == 's') {
            total_seconds += (int32_t)value;
        } else if (unit == 'h') {
            total_seconds += (int32_t)value * 3600;
        } else {
            // Unknown unit
            return -1;
        }

        // Move past unit
        p = endptr + 1;
    }

    return total_seconds;
}

int32_t ik_openai_get_retry_after(const char **headers)
{
    if (headers == NULL) {
        return -1;
    }

    int32_t reset_requests = -1;
    int32_t reset_tokens = -1;

    // Scan headers for x-ratelimit-reset-* headers
    for (size_t i = 0; headers[i] != NULL; i++) {
        const char *header = headers[i];

        // Check for x-ratelimit-reset-requests (case-insensitive)
        if (strncasecmp(header, "x-ratelimit-reset-requests:", 27) == 0) {
            const char *value = header + 27;
            // Skip whitespace
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            reset_requests = parse_duration(value);
        }
        // Check for x-ratelimit-reset-tokens (case-insensitive)
        else if (strncasecmp(header, "x-ratelimit-reset-tokens:", 25) == 0) {
            const char *value = header + 25;
            // Skip whitespace
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            reset_tokens = parse_duration(value);
        }
    }

    // Return minimum of both (prefer requests over tokens)
    if (reset_requests >= 0 && reset_tokens >= 0) {
        return reset_requests < reset_tokens ? reset_requests : reset_tokens;
    } else if (reset_requests >= 0) {
        return reset_requests;
    } else if (reset_tokens >= 0) {
        return reset_tokens;
    } else {
        return -1;
    }
}
