/**
 * @file reasoning.c
 * @brief OpenAI reasoning effort implementation
 */

#include "reasoning.h"
#include "panic.h"
#include <string.h>
#include <assert.h>

/**
 * Reasoning model prefixes
 */
static const char *REASONING_PREFIXES[] = {
    "o1",
    "o3",
    "o4",
    NULL  // Sentinel
};

/**
 * Check if character is a valid separator after prefix
 */
static bool is_valid_separator(char c)
{
    return c == '\0' || c == '-' || c == '_';
}

bool ik_openai_is_reasoning_model(const char *model)
{
    if (model == NULL || model[0] == '\0') {
        return false;
    }

    // Check each prefix
    for (size_t i = 0; REASONING_PREFIXES[i] != NULL; i++) {
        const char *prefix = REASONING_PREFIXES[i];
        size_t prefix_len = strlen(prefix);

        // Check if model starts with prefix
        if (strncmp(model, prefix, prefix_len) == 0) {
            // Validate separator to avoid false matches (e.g., "o30")
            char next_char = model[prefix_len];
            if (is_valid_separator(next_char)) {
                return true;
            }
        }
    }

    return false;
}

const char *ik_openai_reasoning_effort(ik_thinking_level_t level)
{
    switch (level) {
        case IK_THINKING_NONE:
            return NULL;
        case IK_THINKING_LOW:
            return "low";
        case IK_THINKING_MED:
            return "medium";
        case IK_THINKING_HIGH:
            return "high";
        default:
            return NULL;
    }
}

bool ik_openai_supports_temperature(const char *model)
{
    // Reasoning models do NOT support temperature
    return !ik_openai_is_reasoning_model(model);
}

bool ik_openai_prefer_responses_api(const char *model)
{
    // Reasoning models perform 3% better with Responses API
    return ik_openai_is_reasoning_model(model);
}

res_t ik_openai_validate_thinking(TALLOC_CTX *ctx, const char *model, ik_thinking_level_t level)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    if (model == NULL) {
        return ERR(ctx, INVALID_ARG, "Model cannot be NULL");
    }

    // NONE is always valid for any model
    if (level == IK_THINKING_NONE) {
        return OK(NULL);
    }

    // Non-NONE levels require reasoning support
    if (!ik_openai_is_reasoning_model(model)) {
        return ERR(ctx, INVALID_ARG,
                   "Model %s does not support thinking (only o1/o3 reasoning models support thinking)",
                   model);
    }

    // Reasoning models support all levels
    return OK(NULL);
}
