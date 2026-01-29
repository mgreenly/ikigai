/**
 * @file reasoning.c
 * @brief OpenAI reasoning effort implementation
 */

#include "reasoning.h"
#include "panic.h"
#include <string.h>
#include <assert.h>

/**
 * Reasoning model lookup table
 *
 * All OpenAI models that support reasoning/thinking parameters.
 * - o-series: o1, o1-mini, o1-preview, o3, o3-mini
 * - GPT-5.x: gpt-5, gpt-5-mini, gpt-5-nano, gpt-5-pro, gpt-5.1/5.2 variants
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

    // Check exact match in lookup table
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

    // Identify model family
    bool is_o_series = (strncmp(model, "o1", 2) == 0 || strncmp(model, "o3", 2) == 0);
    bool is_gpt5_pro = (strcmp(model, "gpt-5-pro") == 0);

    // Model-aware effort mapping
    switch (level) {
        case IK_THINKING_NONE:
            if (is_o_series) {
                return "low";  // o1/o3: NONE -> "low"
            } else if (is_gpt5_pro) {
                return "high";  // gpt-5-pro: NONE -> "high"
            } else {
                return NULL;  // gpt-5.x: NONE -> omit parameter
            }

        case IK_THINKING_LOW:
            if (is_gpt5_pro) {
                return "high";  // gpt-5-pro: LOW -> "high"
            } else {
                return "low";  // o1/o3 and gpt-5.x: LOW -> "low"
            }

        case IK_THINKING_MED:
            if (is_gpt5_pro) {
                return "high";  // gpt-5-pro: MED -> "high"
            } else {
                return "medium";  // o1/o3 and gpt-5.x: MED -> "medium"
            }

        case IK_THINKING_HIGH:
            return "high";  // All models: HIGH -> "high"

        default:
            return NULL;
    }
}

bool ik_openai_supports_temperature(const char *model)
{
    // Reasoning models do NOT support temperature
    return !ik_openai_is_reasoning_model(model);
}

/**
 * Models that use Responses API (not Chat Completions API)
 *
 * Hardcoded mapping table - no heuristics. Unknown models default to Chat Completions API.
 */
static const char *RESPONSES_API_MODELS[] = {
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

bool ik_openai_use_responses_api(const char *model)
{
    if (model == NULL || model[0] == '\0') {
        return false;
    }

    // Exact match lookup - no heuristics
    for (size_t i = 0; RESPONSES_API_MODELS[i] != NULL; i++) {
        if (strcmp(model, RESPONSES_API_MODELS[i]) == 0) {
            return true;
        }
    }

    // Unknown models default to Chat Completions API
    return false;
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
