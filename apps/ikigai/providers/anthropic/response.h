/**
 * @file response.h
 * @brief Anthropic response parsing (internal)
 *
 * Transforms Anthropic JSON response to internal ik_response_t format.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_RESPONSE_H
#define IK_PROVIDERS_ANTHROPIC_RESPONSE_H

#include <stddef.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Map Anthropic stop_reason to internal finish reason
 *
 * @param stop_reason Anthropic stop_reason string (may be NULL)
 * @return            Internal finish reason enum
 *
 * Mapping:
 * - "end_turn" -> IK_FINISH_STOP
 * - "max_tokens" -> IK_FINISH_LENGTH
 * - "tool_use" -> IK_FINISH_TOOL_USE
 * - "stop_sequence" -> IK_FINISH_STOP
 * - "refusal" -> IK_FINISH_CONTENT_FILTER
 * - NULL or unknown -> IK_FINISH_UNKNOWN
 */
ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason);


#endif /* IK_PROVIDERS_ANTHROPIC_RESPONSE_H */
