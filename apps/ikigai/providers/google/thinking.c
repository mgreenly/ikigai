/**
 * @file thinking.c
 * @brief Google thinking budget/level implementation
 */

#include "apps/ikigai/providers/google/thinking.h"
#include "apps/ikigai/providers/google/error.h"
#include "shared/panic.h"
#include <string.h>


#include "shared/poison.h"
/**
 * Model-specific thinking budget limits for Gemini 2.5
 */
typedef struct {
    const char *model_pattern;
    int32_t min_budget;
    int32_t max_budget;
} ik_google_budget_t;

/**
 * Budget table for known Gemini 2.5 models
 */
static const ik_google_budget_t BUDGET_TABLE[] = {
    {"gemini-2.5-pro", 128, 32768},
    {"gemini-2.5-flash-lite", 512, 24576},
    {"gemini-2.5-flash", 0, 24576},
    {NULL, 0, 0} // Sentinel
};


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

ik_gemini_series_t ik_google_model_series(const char *model)
{
    if (model == NULL) {
        return IK_GEMINI_OTHER;
    }

    // Check for Gemini 3.x
    if (strstr(model, "gemini-3") != NULL) {
        return IK_GEMINI_3;
    }

    // Check for Gemini 2.5
    if (strstr(model, "gemini-2.5") != NULL) {
        return IK_GEMINI_2_5;
    }

    // Other Gemini models (1.5, etc.)
    return IK_GEMINI_OTHER;
}

bool ik_google_supports_thinking(const char *model)
{
    if (model == NULL) {
        return false;
    }

    ik_gemini_series_t series = ik_google_model_series(model);
    return (series == IK_GEMINI_2_5 || series == IK_GEMINI_3);
}

bool ik_google_can_disable_thinking(const char *model)
{
    if (model == NULL) {
        return false;
    }

    ik_gemini_series_t series = ik_google_model_series(model);

    // Gemini 3 uses levels, not budgets - cannot "disable" thinking
    if (series == IK_GEMINI_3) {
        return false;
    }

    // Non-thinking models
    if (series != IK_GEMINI_2_5) {
        return false;
    }

    // For Gemini 2.5, check if min_budget is 0 (exact table match required)
    for (size_t i = 0; BUDGET_TABLE[i].model_pattern != NULL; i++) {
        if (strcmp(model, BUDGET_TABLE[i].model_pattern) == 0) {
            return (BUDGET_TABLE[i].min_budget == 0);
        }
    }

    // Unknown model - cannot determine, return false
    return false;
}

int32_t ik_google_thinking_budget(const char *model, ik_thinking_level_t level)
{
    if (model == NULL) {
        return -1;
    }

    // Only Gemini 2.5 models use token budgets
    ik_gemini_series_t series = ik_google_model_series(model);
    if (series != IK_GEMINI_2_5) {
        return -1;
    }

    // Find budget limits for this model (exact match required)
    int32_t min_budget = -1;
    int32_t max_budget = -1;

    for (size_t i = 0; BUDGET_TABLE[i].model_pattern != NULL; i++) {
        if (strcmp(model, BUDGET_TABLE[i].model_pattern) == 0) {
            min_budget = BUDGET_TABLE[i].min_budget;
            max_budget = BUDGET_TABLE[i].max_budget;
            break;
        }
    }

    // Unknown model - return error
    if (min_budget == -1) {
        return -1;
    }

    // Calculate budget based on level
    int32_t range = max_budget - min_budget;

    switch (level) { // LCOV_EXCL_BR_LINE
        case IK_THINKING_MIN:
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

/**
 * Per-model level mapping for Gemini 3 models
 */
typedef struct {
    const char *model_pattern;
    const char *min_str;
    const char *low_str;
    const char *med_str;
    const char *high_str;
} ik_google_level_map_t;

static const ik_google_level_map_t LEVEL_TABLE[] = {
    {"gemini-3-flash-preview",  "minimal", "low", "medium", "high"},
    {"gemini-3-pro-preview",    "low",     "low", "high",   "high"},
    {"gemini-3.1-pro-preview",  "low",     "low", "medium", "high"},
    {NULL, NULL, NULL, NULL, NULL} // Sentinel
};

const char *ik_google_thinking_level_str(const char *model, ik_thinking_level_t level)
{
    // Look up model in level table
    const ik_google_level_map_t *entry = NULL;
    if (model != NULL) {
        for (size_t i = 0; LEVEL_TABLE[i].model_pattern != NULL; i++) {
            if (strcmp(model, LEVEL_TABLE[i].model_pattern) == 0) {
                entry = &LEVEL_TABLE[i];
                break;
            }
        }
    }

    // Fall back to safe defaults for unknown Gemini 3 models
    if (entry == NULL) {
        switch (level) { // LCOV_EXCL_BR_LINE
            case IK_THINKING_MIN:
            case IK_THINKING_LOW:
                return "low";
            case IK_THINKING_MED:
            case IK_THINKING_HIGH:
                return "high";
            default: // LCOV_EXCL_LINE
                PANIC("Invalid thinking level"); // LCOV_EXCL_LINE
        }
    }

    switch (level) { // LCOV_EXCL_BR_LINE
        case IK_THINKING_MIN:  return entry->min_str;
        case IK_THINKING_LOW:   return entry->low_str;
        case IK_THINKING_MED:   return entry->med_str;
        case IK_THINKING_HIGH:  return entry->high_str;
        default: // LCOV_EXCL_LINE
            PANIC("Invalid thinking level"); // LCOV_EXCL_LINE
    }
}

res_t ik_google_validate_thinking(TALLOC_CTX *ctx, const char *model, ik_thinking_level_t level)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    if (model == NULL) {
        return ERR(ctx, INVALID_ARG, "Model cannot be NULL");
    }

    // MIN is always valid for any model
    if (level == IK_THINKING_MIN) {
        // For Gemini 2.5 models that cannot disable thinking, MIN is invalid
        ik_gemini_series_t series = ik_google_model_series(model);
        if (series == IK_GEMINI_2_5 && !ik_google_can_disable_thinking(model)) {
            return ERR(ctx, INVALID_ARG,
                       "Model '%s' cannot disable thinking (minimum budget > 0). Use LOW, MED, or HIGH.",
                       model);
        }
        return OK(NULL);
    }

    // Non-MIN levels require thinking support
    if (!ik_google_supports_thinking(model)) {
        return ERR(ctx, INVALID_ARG,
                   "Model '%s' does not support Google thinking (only Gemini 2.5 and 3.x models support thinking)",
                   model);
    }

    // All thinking-capable models support LOW/MED/HIGH
    return OK(NULL);
}
