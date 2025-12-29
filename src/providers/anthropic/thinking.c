/**
 * @file thinking.c
 * @brief Anthropic thinking budget implementation
 */

#include "thinking.h"
#include "error.h"
#include "panic.h"
#include <string.h>

/**
 * Model-specific thinking budget limits
 */
typedef struct {
    const char *model_pattern;
    int32_t min_budget;
    int32_t max_budget;
} ik_anthropic_budget_t;

/**
 * Budget table for known Claude models
 */
static const ik_anthropic_budget_t BUDGET_TABLE[] = {
    {"claude-sonnet-4-5", 1024, 64000},
    {"claude-haiku-4-5",  1024, 32000},
    {NULL, 0, 0} // Sentinel
};

/**
 * Default budget for unknown Claude models
 */
static const int32_t DEFAULT_MIN_BUDGET = 1024;
static const int32_t DEFAULT_MAX_BUDGET = 32000;

bool ik_anthropic_supports_thinking(const char *model)
{
    if (model == NULL) {
        return false;
    }

    // All Claude models support thinking
    return strncmp(model, "claude-", 7) == 0;
}

int32_t ik_anthropic_thinking_budget(const char *model, ik_thinking_level_t level)
{
    if (model == NULL) {
        return -1;
    }

    // Non-Claude models don't support Anthropic thinking
    if (!ik_anthropic_supports_thinking(model)) {
        return -1;
    }

    // Find budget limits for this model
    int32_t min_budget = DEFAULT_MIN_BUDGET;
    int32_t max_budget = DEFAULT_MAX_BUDGET;

    for (size_t i = 0; BUDGET_TABLE[i].model_pattern != NULL; i++) {
        if (strncmp(model, BUDGET_TABLE[i].model_pattern,
                    strlen(BUDGET_TABLE[i].model_pattern)) == 0) {
            min_budget = BUDGET_TABLE[i].min_budget;
            max_budget = BUDGET_TABLE[i].max_budget;
            break;
        }
    }

    // Calculate budget based on level
    int32_t range = max_budget - min_budget;

    switch (level) { // LCOV_EXCL_BR_LINE
        case IK_THINKING_NONE:
            return min_budget;
        case IK_THINKING_LOW:
            return min_budget + range / 3;
        case IK_THINKING_MED:
            return min_budget + (2 * range) / 3;
        case IK_THINKING_HIGH:
            return max_budget;
        default: // LCOV_EXCL_LINE
            PANIC("Invalid thinking level"); // LCOV_EXCL_LINE
    }
}

res_t ik_anthropic_validate_thinking(TALLOC_CTX *ctx, const char *model, ik_thinking_level_t level)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    if (model == NULL) {
        return ERR(ctx, INVALID_ARG, "Model cannot be NULL");
    }

    // NONE is always valid for any model
    if (level == IK_THINKING_NONE) {
        return OK(NULL);
    }

    // Non-NONE levels require thinking support
    if (!ik_anthropic_supports_thinking(model)) {
        return ERR(ctx, INVALID_ARG,
                   "Model '%s' does not support Anthropic thinking (only Claude models support thinking)",
                   model);
    }

    // All Claude models support all thinking levels
    return OK(NULL);
}
