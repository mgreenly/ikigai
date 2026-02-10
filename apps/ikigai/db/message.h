#ifndef IK_DB_MESSAGE_H
#define IK_DB_MESSAGE_H

#include "shared/error.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/replay.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * Insert a message event into the database.
 *
 * Writes a message event to the messages table for the specified session.
 * This is the core database API for persisting conversation events.
 *
 * Event kinds:
 *   - "clear"        : Context reset (session start or /clear command)
 *   - "system"       : System prompt message
 *   - "user"         : User input message
 *   - "assistant"    : LLM response message
 *   - "tool_call"    : Tool invocation request from LLM
 *   - "tool_result"  : Tool execution result
 *   - "mark"         : Checkpoint created by /mark command
 *   - "rewind"       : Rollback operation created by /rewind command
 *   - "agent_killed" : Agent termination event
 *   - "command"      : Slash command output for persistence across restarts
 *   - "fork"         : Fork event recorded in both parent and child histories
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

#endif // IK_DB_MESSAGE_H
