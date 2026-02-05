/**
 * @file commands_tool.h
 * @brief Tool management command handlers
 */

#ifndef IK_COMMANDS_TOOL_H
#define IK_COMMANDS_TOOL_H

#include "shared/error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Tool command handler - lists all tools or shows schema for specific tool
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Tool name to show schema, or NULL to list all tools
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_tool(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Refresh command handler - reloads tool registry
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_refresh(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif // IK_COMMANDS_TOOL_H
