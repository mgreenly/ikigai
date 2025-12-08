#ifndef IK_MSG_H
#define IK_MSG_H

#include "error.h"
#include "db/replay.h"
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
    char *kind;       /* Message kind discriminator */
    char *content;    /* Message text content or human-readable summary */
    char *data_json;  /* Structured data for tool messages (NULL for text messages) */
} ik_msg_t;

/**
 * Create canonical message from database message
 *
 * Converts database format (ik_message_t) to canonical in-memory format (ik_msg_t).
 * The DB and canonical formats use the same 'kind' discriminator, so this is
 * primarily a copy/validation operation.
 *
 * Loading rules:
 *   - system/user/assistant: Copy kind and content, set data_json to NULL
 *   - tool_call/tool_result: Copy kind, content, and data_json
 *   - clear/mark/rewind: Return OK(NULL) - not conversation messages
 *
 * @param parent  Talloc context parent (or NULL)
 * @param db_msg  Database message to convert (must not be NULL)
 * @return        OK(msg) for conversation kinds, OK(NULL) for non-conversation kinds, ERR(...) on failure
 */
res_t ik_msg_from_db(void *parent, const ik_message_t *db_msg);

#endif /* IK_MSG_H */
