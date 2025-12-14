#ifndef IK_DB_MESSAGE_H
#define IK_DB_MESSAGE_H

#include "../error.h"
#include "connection.h"
#include "replay.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Insert a message event into the database.
 *
 * Writes a message event to the messages table for the specified session.
 * This is the core database API for persisting conversation events.
 *
 * Event kinds:
 *   - "clear"     : Context reset (session start or /clear command)
 *   - "system"    : System prompt message
 *   - "user"      : User input message
 *   - "assistant" : LLM response message
 *   - "mark"      : Checkpoint created by /mark command
 *   - "rewind"    : Rollback operation created by /rewind command
 *
 * @param db          Database connection context (must not be NULL)
 * @param session_id  Session ID (must be positive, references sessions.id)
 * @param agent_uuid  Agent UUID (may be NULL for backward compatibility)
 * @param kind        Event kind string (must be one of the valid kinds above)
 * @param content     Message content (may be NULL for clear events, empty string allowed)
 * @param data_json   JSONB data as JSON string (may be NULL)
 * @return            OK on success, ERR on failure (invalid params or database error)
 */
res_t ik_db_message_insert(ik_db_ctx_t *db,
                            int64_t session_id,
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json);

/**
 * Validate that a kind string is one of the allowed event kinds.
 *
 * This is primarily used for parameter validation before database insertion.
 * Exposed for testing purposes.
 *
 * @param kind  The kind string to validate (may be NULL)
 * @return      true if kind is valid, false otherwise
 */
bool ik_db_message_is_valid_kind(const char *kind);

/**
 * Create a canonical tool result message.
 *
 * Allocates an ik_message_t struct representing a tool execution result
 * with kind="tool_result". The data_json field is populated with a JSON object
 * containing tool_call_id, name, output, and success fields.
 *
 * Memory ownership:
 * - Returned message is allocated on parent context
 * - All string fields (kind, content, data_json) are children of the message
 * - Single talloc_free(message) or talloc_free(parent) releases all memory
 *
 * @param parent         Talloc context for allocation (can be NULL for root)
 * @param tool_call_id   Unique tool call ID (e.g., "call_abc123")
 * @param name           Tool name (e.g., "glob", "file_read")
 * @param output         Tool output string (can be empty string)
 * @param success        Whether the tool executed successfully (true/false)
 * @param content        Human-readable summary for the message (e.g., "3 files found")
 * @return               Allocated ik_message_t struct (owned by parent), or NULL on OOM
 */
ik_message_t *ik_msg_create_tool_result(void *parent,
                                         const char *tool_call_id,
                                         const char *name,
                                         const char *output,
                                         bool success,
                                         const char *content);

#endif // IK_DB_MESSAGE_H
