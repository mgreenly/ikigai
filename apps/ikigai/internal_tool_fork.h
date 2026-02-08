/**
 * @file internal_tool_fork.h
 * @brief Fork tool handler declaration
 */

#ifndef IK_INTERNAL_TOOL_FORK_H
#define IK_INTERNAL_TOOL_FORK_H

#include "apps/ikigai/agent.h"
#include "apps/ikigai/repl.h"

#include <talloc.h>

/**
 * Fork tool handler - creates child agent with prompt
 * @param ctx Talloc context for result allocation
 * @param agent Agent executing the fork
 * @param args_json JSON arguments {"name": "...", "prompt": "..."}
 * @return JSON result string (wrapped success or failure)
 */
char *ik_fork_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);

/**
 * Fork on_complete hook - steals child to repl and adds to agents array
 * @param repl REPL context
 * @param agent Agent that executed the fork
 */
void ik_fork_on_complete(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

#endif
