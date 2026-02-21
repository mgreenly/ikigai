/**
 * @file thinking.c
 * @brief Anthropic thinking budget implementation
 */

#include "apps/ikigai/providers/anthropic/thinking.h"
#include "apps/ikigai/providers/anthropic/error.h"
#include "shared/panic.h"
#include <string.h>


#include "shared/poison.h"
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
 * All values are powers of 2
 * Note: 4.6 models use adaptive thinking, not budget-based
 */
static const ik_anthropic_budget_t BUDGET_TABLE[] = {
    {"claude-sonnet-4-5", 1024, 65536},
    {"claude-haiku-4-5",  1024, 32768},
    {"claude-opus-4-5",   1024, 65536},
    {NULL, 0, 0} // Sentinel
};

/**
 * Default budget for unknown Claude models
 * All values are powers of 2
 */
static const int32_t DEFAULT_MIN_BUDGET = 1024;
static const int32_t DEFAULT_MAX_BUDGET = 32768;

/**
 * Round down to nearest power of 2
 */
static int32_t floor_power_of_2(int32_t n)
{
    if (n <= 0) {
        return 0;
    }
    // Set all bits below the highest set bit
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    // (n >> 1) + 1 gives us the highest power of 2 <= original n
    return (n >> 1) + 1;
}

bool ik_anthropic_supports_thinking(const char *model)
{
    if (model == NULL) {
        return false;
    }

    // All Claude models support thinking
    return strncmp(model, "claude-", 7) == 0;
}

static const char *ADAPTIVE_MODELS[] = {
    "claude-opus-4-6",
    "claude-sonnet-4-6",
    NULL
};

bool ik_anthropic_is_adaptive_model(const char *model)
{
    if (model == NULL) return false;
    for (size_t i = 0; ADAPTIVE_MODELS[i] != NULL; i++) {
        if (strncmp(model, ADAPTIVE_MODELS[i],
                    strlen(ADAPTIVE_MODELS[i])) == 0)
            return true;
    }
    return false;
}

const char *ik_anthropic_thinking_effort(const char *model, ik_thinking_level_t level)
{
    if (model == NULL) {
        return NULL;
    }

    // Only adaptive models return effort strings
    if (!ik_anthropic_is_adaptive_model(model)) {
        return NULL;
    }

    // Map thinking level to effort string
    switch (level) { // LCOV_EXCL_BR_LINE
        case IK_THINKING_NONE:
            return NULL; // Omit thinking parameter
        case IK_THINKING_LOW:
            return "low";
        case IK_THINKING_MED:
            return "medium";
        case IK_THINKING_HIGH:
            return "high";
        default: // LCOV_EXCL_LINE
            PANIC("Invalid thinking level"); // LCOV_EXCL_LINE
    }
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
            return floor_power_of_2(min_budget + range / 3);
        case IK_THINKING_MED:
            return floor_power_of_2(min_budget + (2 * range) / 3);
        case IK_THINKING_HIGH:
            return max_budget;
        default: // LCOV_EXCL_LINE
            PANIC("Invalid thinking level"); // LCOV_EXCL_LINE
    }
}
