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
 * Model command handler - switches LLM model
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Model name
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * System command handler - sets system message
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args System message text (NULL to clear)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_system(void *ctx, ik_repl_ctx_t *repl, const char *args);

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
 * Parse MODEL/THINKING syntax
 *
 * Parses input like "claude-sonnet-4-5/med" into model and thinking components.
 * If no "/" is present, thinking is set to NULL (use current level).
 *
 * @param ctx      Parent context for talloc allocations
 * @param input    Input string to parse
 * @param model    Output: model name (talloc'd)
 * @param thinking Output: thinking level string or NULL (talloc'd if not NULL)
 * @return         OK on success, ERR on malformed input
 */
res_t cmd_model_parse(void *ctx, const char *input, char **model, char **thinking);

#endif // IK_COMMANDS_BASIC_H
