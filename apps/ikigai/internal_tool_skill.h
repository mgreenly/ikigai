/**
 * @file internal_tool_skill.h
 * @brief Skill management internal tool handlers (load_skill, unload_skill,
 *        load_skillset, list_skills)
 */

#ifndef IK_INTERNAL_TOOL_SKILL_H
#define IK_INTERNAL_TOOL_SKILL_H

#include "apps/ikigai/agent.h"
#include "apps/ikigai/repl.h"

#include <talloc.h>

/**
 * load_skill tool handler (worker thread).
 * Reads skill file, applies positional + template substitution,
 * stores resolved content in agent->tool_deferred_data.
 */
char *ik_internal_tool_load_skill_handler(TALLOC_CTX *ctx,
                                          ik_agent_ctx_t *agent,
                                          const char *args_json);

/**
 * load_skill on_complete hook (main thread).
 * Stores skill in agent->loaded_skills[], invalidates token cache,
 * persists skill_load DB event.
 */
void ik_internal_tool_load_skill_on_complete(ik_repl_ctx_t *repl,
                                             ik_agent_ctx_t *agent);

/**
 * unload_skill tool handler (worker thread).
 * Validates skill is loaded, stores skill name in agent->tool_deferred_data.
 */
char *ik_internal_tool_unload_skill_handler(TALLOC_CTX *ctx,
                                            ik_agent_ctx_t *agent,
                                            const char *args_json);

/**
 * unload_skill on_complete hook (main thread).
 * Removes skill from agent->loaded_skills[], invalidates token cache,
 * persists skill_unload DB event.
 */
void ik_internal_tool_unload_skill_on_complete(ik_repl_ctx_t *repl,
                                               ik_agent_ctx_t *agent);

/**
 * load_skillset tool handler (worker thread).
 * Reads skillset JSON, loads all preload skill files,
 * stores resolved data in agent->tool_deferred_data.
 */
char *ik_internal_tool_load_skillset_handler(TALLOC_CTX *ctx,
                                             ik_agent_ctx_t *agent,
                                             const char *args_json);

/**
 * load_skillset on_complete hook (main thread).
 * Adds preloaded skills and catalog entries to agent state,
 * invalidates token cache, persists skillset DB event.
 */
void ik_internal_tool_load_skillset_on_complete(ik_repl_ctx_t *repl,
                                                ik_agent_ctx_t *agent);

/**
 * list_skills tool handler (worker thread).
 * Read-only — returns JSON of currently loaded skills and catalog entries.
 * No on_complete hook needed.
 */
char *ik_internal_tool_list_skills_handler(TALLOC_CTX *ctx,
                                           ik_agent_ctx_t *agent,
                                           const char *args_json);

/**
 * Register all four skill management tools with the given registry.
 * Called from ik_internal_tools_register().
 *
 * @param registry Tool registry to populate
 */
void ik_skill_tools_register(ik_tool_registry_t *registry);

#endif  // IK_INTERNAL_TOOL_SKILL_H
