/**
 * @file commands_fork_args.c
 * @brief Fork command argument parsing and model override helpers
 */

#include "commands_fork_args.h"

#include "commands.h"
#include "commands_basic.h"
#include "panic.h"
#include "providers/provider.h"

#include <assert.h>
#include <string.h>

/**
 * Parse /fork command arguments for --model flag and prompt
 *
 * Supports both orderings:
 * - /fork --model gpt-5 "prompt"
 * - /fork "prompt" --model gpt-5
 *
 * @param ctx Parent context for talloc allocations
 * @param input Command arguments string
 * @param model Output: model specification (NULL if no --model flag)
 * @param prompt Output: prompt string (NULL if no prompt)
 * @return OK on success, ERR on malformed input
 */
res_t cmd_fork_parse_args(void *ctx, const char *input, char **model, char **prompt)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(model != NULL);    // LCOV_EXCL_BR_LINE
    assert(prompt != NULL);   // LCOV_EXCL_BR_LINE

    *model = NULL;
    *prompt = NULL;

    if (input == NULL || input[0] == '\0') {     // LCOV_EXCL_BR_LINE
        return OK(NULL);  // Empty args is valid (no model, no prompt)
    }

    const char *p = input;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t') {     // LCOV_EXCL_BR_LINE
        p++;
    }

    // Parse tokens in any order
    while (*p != '\0') {     // LCOV_EXCL_BR_LINE
        if (strncmp(p, "--model", 7) == 0 && (p[7] == ' ' || p[7] == '\t')) {
            // Found --model flag
            p += 7;
            // Skip whitespace after --model
            while (*p == ' ' || *p == '\t') {     // LCOV_EXCL_BR_LINE
                p++;
            }
            if (*p == '\0') {
                return ERR(ctx, INVALID_ARG, "--model requires an argument");
            }
            // Extract model spec (until next space or quote)
            const char *model_start = p;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '"') {     // LCOV_EXCL_BR_LINE
                p++;
            }
            size_t model_len = (size_t)(p - model_start);
            if (model_len == 0) {
                return ERR(ctx, INVALID_ARG, "--model requires an argument");
            }
            *model = talloc_strndup(ctx, model_start, model_len);
            if (*model == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
        } else if (*p == '"') {
            // Found quoted prompt
            p++;  // Skip opening quote
            const char *prompt_start = p;
            // Find closing quote
            while (*p != '\0' && *p != '"') {     // LCOV_EXCL_BR_LINE
                p++;
            }
            if (*p != '"') {
                return ERR(ctx, INVALID_ARG, "Unterminated quoted string");
            }
            size_t prompt_len = (size_t)(p - prompt_start);
            *prompt = talloc_strndup(ctx, prompt_start, prompt_len);
            if (*prompt == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
            p++;  // Skip closing quote
        } else {
            // Unknown token - likely unquoted text
            return ERR(ctx, INVALID_ARG, "Error: Prompt must be quoted (usage: /fork \"prompt\") or use --model flag");
        }

        // Skip trailing whitespace
        while (*p == ' ' || *p == '\t') {     // LCOV_EXCL_BR_LINE
            p++;
        }
    }

    return OK(NULL);
}

/**
 * Apply model override to child agent
 *
 * Parses MODEL/THINKING specification and updates child's provider, model,
 * and thinking_level fields. Infers provider from model name.
 *
 * @param child Child agent to configure
 * @param model_spec Model specification (e.g., "gpt-5" or "gpt-5-mini/high")
 * @return OK on success, ERR on invalid model or provider
 */
res_t cmd_fork_apply_override(ik_agent_ctx_t *child, const char *model_spec)
{
    assert(child != NULL);      // LCOV_EXCL_BR_LINE
    assert(model_spec != NULL); // LCOV_EXCL_BR_LINE

    // Parse MODEL/THINKING syntax (reuse existing parser)
    char *model_name = NULL;
    char *thinking_str = NULL;
    res_t parse_res = cmd_model_parse(child, model_spec, &model_name, &thinking_str);
    if (is_err(&parse_res)) {
        return parse_res;
    }

    // Infer provider from model name
    const char *provider = ik_infer_provider(model_name);
    if (provider == NULL) {
        return ERR(child, INVALID_ARG, "Unknown model '%s'", model_name);
    }

    // Set provider and model
    if (child->provider != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(child->provider);     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    child->provider = talloc_strdup(child, provider);
    if (child->provider == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    if (child->model != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(child->model);     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    child->model = talloc_strdup(child, model_name);
    if (child->model == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Parse thinking level if specified, otherwise keep parent's level (already inherited)
    if (thinking_str != NULL) {
        ik_thinking_level_t thinking_level;
        if (strcmp(thinking_str, "none") == 0) {
            thinking_level = IK_THINKING_NONE;
        } else if (strcmp(thinking_str, "low") == 0) {
            thinking_level = IK_THINKING_LOW;
        } else if (strcmp(thinking_str, "med") == 0) {
            thinking_level = IK_THINKING_MED;
        } else if (strcmp(thinking_str, "high") == 0) {
            thinking_level = IK_THINKING_HIGH;
        } else {
            return ERR(child, INVALID_ARG, "Invalid thinking level '%s' (must be: none, low, med, high)", thinking_str);
        }
        child->thinking_level = thinking_level;
    }

    return OK(NULL);
}

/**
 * Copy parent's provider config to child agent
 *
 * Inherits provider, model, and thinking_level from parent to child.
 * Used when no --model override is specified.
 *
 * @param child Child agent to configure
 * @param parent Parent agent to copy from
 * @return OK on success, ERR on memory allocation failure
 */
res_t cmd_fork_inherit_config(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    assert(child != NULL);  // LCOV_EXCL_BR_LINE
    assert(parent != NULL); // LCOV_EXCL_BR_LINE

    // Copy provider
    if (parent->provider != NULL) {
        if (child->provider != NULL) {     // LCOV_EXCL_BR_LINE
            talloc_free(child->provider);     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE
        child->provider = talloc_strdup(child, parent->provider);
        if (child->provider == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
    }

    // Copy model
    if (parent->model != NULL) {
        if (child->model != NULL) {     // LCOV_EXCL_BR_LINE
            talloc_free(child->model);     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE
        child->model = talloc_strdup(child, parent->model);
        if (child->model == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
    }

    // Copy thinking level
    child->thinking_level = parent->thinking_level;

    return OK(NULL);
}
