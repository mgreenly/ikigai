/**
 * @file internal_tools.h
 * @brief Internal tool implementations and registration
 */

#ifndef IK_INTERNAL_TOOLS_H
#define IK_INTERNAL_TOOLS_H

#include "apps/ikigai/tool_registry.h"

/**
 * Register all internal tools with the given registry.
 * Called from shared_ctx_init() and ik_cmd_refresh().
 *
 * @param registry Tool registry to populate
 */
void ik_internal_tools_register(ik_tool_registry_t *registry);

#endif  // IK_INTERNAL_TOOLS_H
