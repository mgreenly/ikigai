/**
 * @file commands_fork_helpers.h
 * @brief Fork command utility helpers
 */

#ifndef COMMANDS_FORK_HELPERS_H
#define COMMANDS_FORK_HELPERS_H

#include "apps/ikigai/agent.h"
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"

#include <stdbool.h>
#include <stdint.h>

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

#endif // COMMANDS_FORK_HELPERS_H
