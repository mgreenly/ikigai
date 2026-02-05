#ifndef IK_DB_REPLAY_H
#define IK_DB_REPLAY_H

#include "shared/error.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/db/connection.h"

#include <stdint.h>
#include <talloc.h>

// Forward declaration
typedef struct ik_logger ik_logger_t;

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
    ik_msg_t **messages;              // Dynamic array of message pointers (unified type)
    size_t count;                     // Number of messages in context
    size_t capacity;                  // Allocated capacity
    ik_replay_mark_stack_t mark_stack; // Stack of checkpoint marks
} ik_replay_context_t;

/**
 * Replay range - defines a subset of messages to query for replay
 *
 * Each range contains enough information to query the exact subset of messages
 * from a specific agent's history.
 *
 * Semantics:
 *   - start_id is exclusive (query messages AFTER this ID)
 *   - end_id is inclusive (query messages up to and including this ID)
 *   - end_id = 0 means "no upper limit" (used for leaf agent)
 */
typedef struct {
    char *agent_uuid;   // Which agent's messages to query
    int64_t start_id;   // Start AFTER this message ID (0 = from beginning)
    int64_t end_id;     // End AT this message ID (0 = no limit, i.e., leaf)
} ik_replay_range_t;

#endif // IK_DB_REPLAY_H
