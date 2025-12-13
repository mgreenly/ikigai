#ifndef IK_DB_REPLAY_H
#define IK_DB_REPLAY_H

#include "../error.h"
#include "connection.h"
#include <stdint.h>
#include <talloc.h>

/**
 * Message structure - represents a single event from the database
 */
struct ik_message {
    int64_t id;        // Message ID from database
    char *kind;        // Event kind (clear, system, user, assistant, mark, rewind)
    char *content;     // Message content
    char *data_json;   // JSONB data as string
};
typedef struct ik_message ik_message_t;

/**
 * Mark entry - checkpoint information for conversation rollback
 */
typedef struct {
    int64_t message_id;  // ID of the mark event
    char *label;         // User label or NULL
    size_t context_idx;  // Position in context array when mark was created
} ik_replay_mark_t;

/**
 * Mark stack - dynamic array of checkpoint marks
 */
typedef struct {
    ik_replay_mark_t *marks;  // Dynamic array of marks
    size_t count;             // Number of marks
    size_t capacity;          // Allocated capacity
} ik_replay_mark_stack_t;

/**
 * Context array - dynamic array of messages representing conversation state
 */
typedef struct {
    ik_message_t **messages;          // Dynamic array of message pointers
    size_t count;                     // Number of messages in context
    size_t capacity;                  // Allocated capacity
    ik_replay_mark_stack_t mark_stack; // Stack of checkpoint marks
} ik_replay_context_t;

/**
 * Load messages from database and replay to build context
 *
 * Queries the messages table for the specified session, ordered by created_at,
 * and processes events to build the current conversation context.
 *
 * Event processing:
 *   - "clear": Empty context (set count = 0)
 *   - "system"/"user"/"assistant": Append to context array
 *   - "mark"/"rewind": Skip for now (Task 7b will handle these)
 *
 * Memory management:
 *   - All structures allocated under ctx parameter
 *   - Uses geometric growth (capacity *= 2) for dynamic array
 *   - Initial capacity: 16 messages
 *   - Single talloc_free(ctx) releases everything
 *
 * @param ctx         Talloc context for allocations (must not be NULL)
 * @param db_ctx      Database connection context (must not be NULL)
 * @param session_id  Session ID to load messages for (must be positive)
 * @return            OK with replay_context on success, ERR on failure
 */
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id);

#endif // IK_DB_REPLAY_H
