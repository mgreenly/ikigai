/**
 * @file commands_toolset.h
 * @brief Toolset command handlers for filtering tools visible to LLM
 */

#ifndef IK_COMMANDS_TOOLSET_H
#define IK_COMMANDS_TOOLSET_H

#include "shared/error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Toolset command handler - manage tool filter
 *
 * Without arguments: Lists current toolset filter or shows "No toolset filter active"
 * With arguments: Parses comma/space-separated tool names and replaces current filter
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (NULL to list, tool names to set)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_toolset(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Persist toolset command to database
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Tool names argument
 */
void ik_persist_toolset_command(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * List current toolset filter
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_toolset_list(void *ctx, ik_repl_ctx_t *repl);

/**
 * Set toolset filter from comma/space-separated list
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Tool names (comma/space-separated)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_toolset_set(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif // IK_COMMANDS_TOOLSET_H
