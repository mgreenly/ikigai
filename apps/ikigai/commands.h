/**
 * @file commands.h
 * @brief REPL command registry and dispatcher
 *
 * Provides a command registry for handling slash commands (e.g., /clear, /help).
 * Commands are registered with a name, description, and handler function.
 */

#ifndef IK_COMMANDS_H
#define IK_COMMANDS_H

#include "apps/ikigai/db/connection.h"
#include "shared/error.h"

#include <inttypes.h>

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Command handler function signature.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (NULL if no arguments)
 * @return OK on success, ERR on failure
 */
typedef res_t (*ik_cmd_handler_t)(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Command definition structure.
 */
typedef struct {
    const char *name;            // Command name (without leading slash)
    const char *description;     // Human-readable description
    ik_cmd_handler_t handler;     // Handler function
} ik_command_t;

/**
 * Dispatch a command to its handler.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param input Command input (with leading slash, e.g., "/clear")
 * @return OK if command was handled, ERR if unknown command or handler failed
 *
 * Preconditions:
 * - ctx != NULL
 * - repl != NULL
 * - input != NULL
 * - input[0] == '/'
 */
res_t ik_cmd_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input);

/**
 * Get array of all registered commands.
 *
 * @param count Output parameter for number of commands
 * @return Pointer to command array (static, do not free)
 *
 * Preconditions:
 * - count != NULL
 */
const ik_command_t *ik_cmd_get_all(size_t *count);

/**
 * Fork command handler - creates child agent
 *
 * Creates a child agent and switches to it. Without prompt argument,
 * the child inherits the parent's conversation history.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (NULL for this basic version)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Kill command handler - terminates agent
 *
 * Without arguments, terminates the current agent and switches to parent.
 * Root agents cannot be killed.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (NULL for self-kill)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_kill(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Core send logic - reusable by both slash command and internal tool
 *
 * Validates recipient, inserts mail into database, and fires PG NOTIFY
 * to wake up the recipient agent.
 *
 * @param ctx Parent context for talloc allocations
 * @param db_ctx Database context to use (main or worker thread)
 * @param session_id Current session ID
 * @param sender_uuid UUID of the sending agent
 * @param recipient_uuid UUID of the recipient agent (must be running)
 * @param body Message body (must be non-empty)
 * @param error_msg_out Optional pointer to receive error message on failure (allocated on ctx)
 * @return OK on success, ERR on failure
 */
res_t ik_send_core(void *ctx,
                   ik_db_ctx_t *db_ctx,
                   int64_t session_id,
                   const char *sender_uuid,
                   const char *recipient_uuid,
                   const char *body,
                   char **error_msg_out);

/**
 * Send command handler - sends message to another agent
 *
 * Sends a message to another agent's mailbox. Format: /send <uuid> "message"
 * Validates recipient exists and is running before sending.
 * Fires PG NOTIFY to wake up the recipient.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments: "<uuid> \"message\""
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_send(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Reap command handler - removes dead agents from memory
 *
 * Removes dead agents from the agent array and frees their memory.
 * Two modes:
 * - Bulk (no args): Removes all dead agents
 * - Targeted (/reap <uuid>): Removes specified dead agent and all its children
 * Switches to first living agent if current agent is affected.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments: NULL for bulk, "<uuid>" for targeted
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_reap(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Wait command handler - waits for messages from other agents
 *
 * Two modes:
 * - Next message (/wait TIMEOUT): Wait for first message from any agent
 * - Fan-in (/wait TIMEOUT UUID1 UUID2...): Wait for all listed agents to respond
 * Uses PG LISTEN/NOTIFY for reactive wake-up. Runs on worker thread.
 * Puts agent into EXECUTING_TOOL state, spinner runs, escape interrupts.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments: "TIMEOUT [UUID1 UUID2...]"
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_wait(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Agents command handler - displays agent hierarchy tree
 *
 * Displays the agent hierarchy as a tree showing parent-child relationships.
 * The current agent is marked with *, root agents are labeled, and each agent
 * shows its status (running/dead). Includes a summary count at the end.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_agents(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Persist command execution to database
 *
 * Captures command input and output from scrollback, then persists to database.
 * Logs errors but does not fail if database persistence fails.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param input Original command input (with leading slash)
 * @param cmd_name Parsed command name
 * @param args Command arguments (NULL if no arguments)
 * @param lines_before Scrollback line count before command execution
 */
void ik_cmd_persist_to_db(void *ctx,
                          ik_repl_ctx_t *repl,
                          const char *input,
                          const char *cmd_name,
                          const char *args,
                          size_t lines_before);

#endif // IK_COMMANDS_H
