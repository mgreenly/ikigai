/**
 * @file response_utils.c
 * @brief Google response utility functions
 */

#include "response_utils.h"

#include "response.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Generate random 22-character base64url tool call ID
 */
char *ik_google_generate_tool_id(TALLOC_CTX *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    static const char ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    static bool seeded = false;

    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    char *id = talloc_zero_array(ctx, char, 23); // 22 chars + null
    if (id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    for (int i = 0; i < 22; i++) {
        id[i] = ALPHABET[rand() % 64];
    }
    id[22] = '\0';

    return id;
}

/**
 * Map Google finishReason to internal finish reason
 */
ik_finish_reason_t ik_google_map_finish_reason(const char *finish_reason)
{
    if (finish_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(finish_reason, "STOP") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(finish_reason, "MAX_TOKENS") == 0) {
        return IK_FINISH_LENGTH;
    } else if (strcmp(finish_reason, "SAFETY") == 0 ||
               strcmp(finish_reason, "BLOCKLIST") == 0 ||
               strcmp(finish_reason, "PROHIBITED_CONTENT") == 0 ||
               strcmp(finish_reason, "IMAGE_SAFETY") == 0 ||
               strcmp(finish_reason, "IMAGE_PROHIBITED_CONTENT") == 0 ||
               strcmp(finish_reason, "RECITATION") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    } else if (strcmp(finish_reason, "MALFORMED_FUNCTION_CALL") == 0 ||
               strcmp(finish_reason, "UNEXPECTED_TOOL_CALL") == 0) {
        return IK_FINISH_ERROR;
    }

    return IK_FINISH_UNKNOWN;
}

/**
 * Extract thought signature from response (Gemini 3 only)
 */
char *ik_google_extract_thought_signature_from_response(TALLOC_CTX *ctx, yyjson_val *root)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Try to find thoughtSignature in various possible locations
    // Location varies by API version and model
    yyjson_val *sig = yyjson_obj_get(root, "thoughtSignature");
    if (sig == NULL) {
        // Try in candidates[0]
        yyjson_val *candidates = yyjson_obj_get(root, "candidates");
        if (candidates != NULL && yyjson_is_arr(candidates)) {
            yyjson_val *first = yyjson_arr_get_first(candidates);
            if (first != NULL) {
                sig = yyjson_obj_get(first, "thoughtSignature");
            }
        }
    }

    if (sig == NULL || !yyjson_is_str(sig)) {
        return NULL; // No signature found
    }

    const char *sig_str = yyjson_get_str(sig);
    if (sig_str == NULL || sig_str[0] == '\0') {  // LCOV_EXCL_BR_LINE
        return NULL;
    }

    // Build provider_data JSON: {"thought_signature": "value"}
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) {                  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);       // LCOV_EXCL_LINE
        PANIC("Out of memory");         // LCOV_EXCL_LINE
    }

    yyjson_mut_val *key = yyjson_mut_strcpy(doc, "thought_signature");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *val = yyjson_mut_strcpy(doc, sig_str);              // LCOV_EXCL_BR_LINE
    if (key == NULL || val == NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);       // LCOV_EXCL_LINE
        PANIC("Out of memory");         // LCOV_EXCL_LINE
    }

    yyjson_mut_obj_add(obj, key, val);
    yyjson_mut_doc_set_root(doc, obj);

    // Serialize to string
    size_t json_len;
    char *json = yyjson_mut_write(doc, YYJSON_WRITE_NOFLAG, &json_len);
    yyjson_mut_doc_free(doc);

    if (json == NULL) { // LCOV_EXCL_BR_LINE
        return NULL;    // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(ctx, json);
    free(json); // yyjson uses malloc

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}
