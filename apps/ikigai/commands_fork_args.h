/**
 * @file commands_fork_args.h
 * @brief Fork command argument parsing and model override helpers
 */

#ifndef COMMANDS_FORK_ARGS_H
#define COMMANDS_FORK_ARGS_H

#include "apps/ikigai/agent.h"
#include "shared/error.h"

/**
 * Parse /fork command arguments for --model flag and prompt
 *
 * @param ctx Parent context for talloc allocations
 * @param input Command arguments string
 * @param model Output: model specification (NULL if no --model flag)
 * @param prompt Output: prompt string (NULL if no prompt)
 * @return OK on success, ERR on malformed input
 */
res_t ik_commands_fork_parse_args(void *ctx, const char *input, char **model, char **prompt);

/**
 * Apply model override to child agent
 *
 * @param child Child agent to configure
 * @param model_spec Model specification (e.g., "gpt-5" or "gpt-5-mini/high")
 * @return OK on success, ERR on invalid model or provider
 */
res_t ik_commands_fork_apply_override(ik_agent_ctx_t *child, const char *model_spec);

/**
 * Copy parent's provider config to child agent
 *
 * @param child Child agent to configure
 * @param parent Parent agent to copy from
 * @return OK on success, ERR on memory allocation failure
 */
res_t ik_commands_fork_inherit_config(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent);

#endif // COMMANDS_FORK_ARGS_H
