/**
 * @file reasoning.c
 * @brief OpenAI reasoning effort implementation
 */

#include "reasoning.h"
#include "panic.h"
#include <string.h>
#include <assert.h>

/**
 * Lookup table for reasoning models
 *
 * Reasoning models support reasoning.effort parameter:
 * - o1, o1-mini, o1-preview, o3, o3-mini
 * - gpt-5, gpt-5-mini, gpt-5-nano, gpt-5-pro
 * - gpt-5.1, gpt-5.1-chat-latest, gpt-5.1-codex
 * - gpt-5.2, gpt-5.2-chat-latest, gpt-5.2-codex
 */
static const char *REASONING_MODELS[] = {
    "o1",
    "o1-mini",
    "o1-preview",
    "o3",
    "o3-mini",
    "gpt-5",
    "gpt-5-mini",
    "gpt-5-nano",
    "gpt-5-pro",
    "gpt-5.1",
    "gpt-5.1-chat-latest",
    "gpt-5.1-codex",
    "gpt-5.2",
    "gpt-5.2-chat-latest",
    "gpt-5.2-codex",
    NULL  // Sentinel
};

bool ik_openai_is_reasoning_model(const char *model)
{
    if (model == NULL || model[0] == '\0') {
        return false;
    }

    // Check each reasoning model in lookup table
    for (size_t i = 0; REASONING_MODELS[i] != NULL; i++) {
        if (strcmp(model, REASONING_MODELS[i]) == 0) {
            return true;
        }
    }

    return false;
}

const char *ik_openai_reasoning_effort(const char *model, ik_thinking_level_t level)
{
    if (model == NULL) {
        return NULL;
    }

    // Determine model family
    bool is_o1_o3 = (strcmp(model, "o1") == 0 ||
                     strcmp(model, "o1-mini") == 0 ||
                     strcmp(model, "o1-preview") == 0 ||
                     strcmp(model, "o3") == 0 ||
                     strcmp(model, "o3-mini") == 0);

    bool is_gpt5_pro = (strcmp(model, "gpt-5-pro") == 0);

    bool is_gpt5_family = (strncmp(model, "gpt-5", 5) == 0 && !is_gpt5_pro);

    // Apply model-specific mapping
    if (is_o1_o3) {
        // o1/o3 family: NONE->low, LOW->low, MED->medium, HIGH->high
        switch (level) {
            case IK_THINKING_NONE:
                return "low";
            case IK_THINKING_LOW:
                return "low";
            case IK_THINKING_MED:
                return "medium";
            case IK_THINKING_HIGH:
                return "high";
            default:
                return NULL;
        }
    } else if (is_gpt5_pro) {
        // gpt-5-pro: all levels -> "high"
        switch (level) {
            case IK_THINKING_NONE:
            case IK_THINKING_LOW:
            case IK_THINKING_MED:
            case IK_THINKING_HIGH:
                return "high";
            default:
                return NULL;
        }
    } else if (is_gpt5_family) {
        // gpt-5 family: NONE->NULL, LOW->low, MED->medium, HIGH->high
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

    // Unknown model or non-reasoning model
    return NULL;
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
