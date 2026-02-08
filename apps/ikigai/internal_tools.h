/**
 * @file internal_tools.h
 * @brief Internal tool implementations and registration
 */

#ifndef IK_INTERNAL_TOOLS_H
#define IK_INTERNAL_TOOLS_H

#include "apps/ikigai/agent.h"
#include "apps/ikigai/tool_registry.h"

#include <talloc.h>

/**
 * Register all internal tools with the given registry.
 * Called from shared_ctx_init() and ik_cmd_refresh().
 *
 * @param registry Tool registry to populate
 */
void ik_internal_tools_register(ik_tool_registry_t *registry);

/**
 * Internal tool handlers (exposed for testing).
 * @{
 */
char *ik_internal_tool_kill_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);
char *ik_internal_tool_send_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);
void ik_internal_tool_kill_on_complete(struct ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);
/** @} */

#endif  // IK_INTERNAL_TOOLS_H
