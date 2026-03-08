/**
 * @file commands_fork_helpers.h
 * @brief Fork command utility helpers
 */

#ifndef IK_COMMANDS_FORK_HELPERS_H
#define IK_COMMANDS_FORK_HELPERS_H

#include "apps/ikigai/agent.h"
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * Copy parent's loaded_skills to child with load_position reset to 0.
 * Used for no-prompt fork where child inherits parent's working knowledge.
 */
void ik_commands_fork_copy_loaded_skills(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent);

/**
 * Copy parent's skillset_catalog to child with load_position reset to 0.
 * Used for no-prompt fork where child inherits parent's skill catalog.
 */
void ik_commands_fork_copy_skillset_catalog(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent);

/**
 * Helper to convert thinking level enum to string
 */
const char *ik_commands_thinking_level_to_string(ik_thinking_level_t level);

/**
 * Helper to build fork feedback message
 */
char *ik_commands_build_fork_feedback(TALLOC_CTX *ctx, const ik_agent_ctx_t *child, bool is_override);

/**
 * Helper to insert fork events into database
 */
res_t ik_commands_insert_fork_events(TALLOC_CTX *ctx,
                                     ik_repl_ctx_t *repl,
                                     ik_agent_ctx_t *parent,
                                     ik_agent_ctx_t *child,
                                     int64_t fork_message_id);

#endif // IK_COMMANDS_FORK_HELPERS_H
