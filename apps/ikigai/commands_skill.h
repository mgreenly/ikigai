/**
 * @file commands_skill.h
 * @brief Skill management slash command handlers (/load, /unload, /skills)
 */

#ifndef IK_COMMANDS_SKILL_H
#define IK_COMMANDS_SKILL_H

#include "shared/error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;
typedef struct ik_agent_ctx ik_agent_ctx_t;

/**
 * Load command handler - loads a skill into the agent context
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Skill name and optional positional args (e.g., "database" or "tmpl arg1 arg2")
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_load(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Unload command handler - removes a loaded skill from the agent context
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Skill name to unload
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_unload(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Skills command handler - lists currently loaded skills
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Command arguments (unused)
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_skills(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Skillset command handler - loads a skillset (preloads skills, populates catalog)
 *
 * @param ctx Parent context for talloc allocations
 * @param repl REPL context
 * @param args Skillset name
 * @return OK on success, ERR on failure
 */
res_t ik_cmd_skillset(void *ctx, ik_repl_ctx_t *repl, const char *args);

/**
 * Add or replace a skill entry in agent->loaded_skills[].
 * Safe to call from on_complete hooks (main thread).
 *
 * @param agent      Agent context
 * @param skill_name Skill name (e.g., "database")
 * @param content    Fully resolved skill content
 */
void ik_skill_store_loaded(ik_agent_ctx_t *agent, const char *skill_name,
                           const char *content);

/**
 * Add a catalog entry to agent->skillset_catalog[].
 * Safe to call from on_complete hooks (main thread).
 *
 * @param agent       Agent context
 * @param skill_name  Skill name (e.g., "style")
 * @param description Short description for system prompt
 */
void ik_skillset_store_catalog_entry(ik_agent_ctx_t *agent, const char *skill_name,
                                     const char *description);

/**
 * Load a single skill by name into the agent (no positional args).
 *
 * Reads the skill file, applies template processing, stores in loaded_skills[],
 * and persists a skill_load DB event. Used internally by /skillset.
 *
 * @param ctx    Talloc parent context
 * @param repl   REPL context
 * @param agent  Agent to load the skill into
 * @param skill_name Skill name (e.g., "database")
 * @return true on success, false if skill file not found
 */
bool ik_skill_load_by_name(void *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                           const char *skill_name);

#endif // IK_COMMANDS_SKILL_H
