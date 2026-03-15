/**
 * @file internal_tools_bg.h
 * @brief Background process internal tool handlers (pstart, pread, pwrite, pkill, ps)
 */

#ifndef IK_INTERNAL_TOOLS_BG_H
#define IK_INTERNAL_TOOLS_BG_H

#include "apps/ikigai/agent.h"
#include "apps/ikigai/tool_registry.h"
#include <talloc.h>

/**
 * Register all background process tools with the given registry.
 * @param registry Tool registry to populate
 */
void ik_bg_tools_register(ik_tool_registry_t *registry);

/* Handler functions (exposed for testing) */
char *ik_bg_pstart_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);
char *ik_bg_pread_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);
char *ik_bg_pwrite_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);
char *ik_bg_pkill_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);
char *ik_bg_ps_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json);

#endif /* IK_INTERNAL_TOOLS_BG_H */
