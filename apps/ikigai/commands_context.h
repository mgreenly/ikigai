/**
 * @file commands_context.h
 * @brief /context command handler
 *
 * Renders the full LLM context layout as it would be sent on the next turn,
 * using ANSI box-drawing characters. Adapts to terminal width (minimum 60).
 */

#ifndef IK_COMMANDS_CONTEXT_H
#define IK_COMMANDS_CONTEXT_H

#include "shared/error.h"

typedef struct ik_repl_ctx_t ik_repl_ctx_t;

/**
 * Context command handler
 *
 * Displays the full context layout with box-drawing characters.
 * Groups shown in order: System Prompt, Pinned Documents, Skills,
 * Skill Catalog, Session Summaries, Recent Summary, Tools, Message History.
 *
 * @param ctx  Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return     OK on success, ERR on failure
 */
res_t ik_cmd_context(void *ctx, ik_repl_ctx_t *repl, const char *args);

#endif /* IK_COMMANDS_CONTEXT_H */
