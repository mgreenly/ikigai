#ifndef IK_MSG_H
#define IK_MSG_H

#include "error.h"
#include <stdbool.h>
#include <stdint.h>
#include <talloc.h>

/**
 * Canonical message structure
 *
 * Represents a single message in the unified conversation format.
 * Uses 'kind' discriminator that maps directly to DB format and renders
 * differently based on context.
 *
 * Kind values:
 *   - "system": System message (role-based)
 *   - "user": User message (role-based)
 *   - "assistant": Assistant message (role-based)
 *   - "tool_call": Tool call message (has data_json with structured tool call data)
 *   - "tool_result": Tool result message (has data_json with structured result data)
 *
 * Non-conversation kinds (not included in conversation):
 *   - "clear": Clear event (not part of LLM context)
 *   - "mark": Mark event (checkpoint metadata)
 *   - "rewind": Rewind event (navigation metadata)
 */
typedef struct {
    int64_t id;       /* DB row ID (0 if not from DB) */
    char *kind;       /* Message kind discriminator */
    char *content;    /* Message text content or human-readable summary */
    char *data_json;  /* Structured data for tool messages (NULL for text messages) */
} ik_msg_t;

/**
 * Check if a message kind should be included in LLM conversation context
 *
 * @param kind Message kind string (e.g., "user", "assistant", "clear")
 * @return true if kind should be sent to LLM, false otherwise
 *
 * Conversation kinds (returns true):
 *   - "system", "user", "assistant", "tool_call", "tool_result", "tool"
 *
 * Metadata kinds (returns false):
 *   - "clear", "mark", "rewind", "agent_killed"
 *   - NULL or unknown kinds
 */
bool ik_msg_is_conversation_kind(const char *kind);

#endif /* IK_MSG_H */
