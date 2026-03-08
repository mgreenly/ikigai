/**
 * @file commands_skill.h
 * @brief Skill management slash command handlers (/load, /unload, /skills)
 */

#ifndef IK_COMMANDS_SKILL_H
#define IK_COMMANDS_SKILL_H

#include "shared/error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

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

#endif // IK_COMMANDS_SKILL_H
