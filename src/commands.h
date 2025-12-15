/**
 * @file commands.h
 * @brief REPL command registry and dispatcher
 *
 * Provides a command registry for handling slash commands (e.g., /clear, /help).
 * Commands are registered with a name, description, and handler function.
 */

#ifndef IK_COMMANDS_H
#define IK_COMMANDS_H

#include "error.h"

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
res_t cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args);

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
res_t cmd_kill(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Send command handler - sends mail to another agent
 *
 * Sends a mail message to another agent's mailbox. Format: /send <uuid> "message"
 * Validates recipient exists and is running before sending.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments: "<uuid> \"message\""
 * @return OK on success, ERR on failure
 */
res_t cmd_send(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Check mail command handler - lists inbox contents
 *
 * Displays the current agent's inbox with unread markers, message previews,
 * and relative timestamps. Unread messages are shown first.
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t cmd_check_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif // IK_COMMANDS_H
