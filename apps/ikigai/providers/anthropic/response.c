/**
 * @file response.c
 * @brief Anthropic response parsing implementation
 */

#include "apps/ikigai/providers/anthropic/response.h"

#include <string.h>

#include "shared/poison.h"
/* ================================================================
 * Public Functions
 * ================================================================ */

ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason)
{
    if (stop_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(stop_reason, "end_turn") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(stop_reason, "max_tokens") == 0) {
        return IK_FINISH_LENGTH;
    } else if (strcmp(stop_reason, "tool_use") == 0) {
        return IK_FINISH_TOOL_USE;
    } else if (strcmp(stop_reason, "stop_sequence") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(stop_reason, "refusal") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    }

    return IK_FINISH_UNKNOWN;
}
