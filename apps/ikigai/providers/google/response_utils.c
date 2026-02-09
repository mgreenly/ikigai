/**
 * @file response_utils.c
 * @brief Google response utility functions
 */

#include "apps/ikigai/providers/google/response_utils.h"

#include "apps/ikigai/providers/google/response.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "shared/poison.h"
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
