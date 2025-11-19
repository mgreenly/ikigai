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

#endif // IK_COMMANDS_H
