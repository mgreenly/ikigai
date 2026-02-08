/**
 * @file internal_tool_wait.h
 * @brief Wait tool handler declaration
 */

#ifndef IK_INTERNAL_TOOL_WAIT_H
#define IK_INTERNAL_TOOL_WAIT_H

#include "apps/ikigai/agent.h"

#include <talloc.h>

/**
 * Wait tool handler - waits for messages from other agents
 * @param ctx Talloc context for result allocation
 * @param agent Agent executing the wait
 * @param args_json JSON arguments {"timeout": N, "from_agents": ["uuid", ...] (optional)}
 * @return JSON result string (wrapped success or failure)
 */
char *ik_wait_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);

#endif
