/**
 * @file commands_basic.h
 * @brief Basic REPL command handlers
 */

#ifndef IK_COMMANDS_BASIC_H
#define IK_COMMANDS_BASIC_H

#include "error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Clear command handler - clears scrollback, session, and marks
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Help command handler - displays available commands
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_help(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Debug command handler - toggles debug output
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args "on", "off", or NULL to show status
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_debug(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Exit command handler - exits the application
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_exit(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif // IK_COMMANDS_BASIC_H
