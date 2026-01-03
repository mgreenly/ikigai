#include "providers/provider.h"

#include "panic.h"

#include <string.h>

/**
 * Model capability lookup table
 *
 * Maps model prefixes to their thinking capabilities and budgets.
 * Used for validation and user feedback.
 */
static const ik_model_capability_t MODEL_CAPABILITIES[] = {
    // Anthropic models (token budget)
    {"claude-haiku-4-5", "anthropic", true, 32000},
    {"claude-sonnet-4-5", "anthropic", true, 64000},
    {"claude-opus-4-5", "anthropic", true, 64000},

    // OpenAI thinking models (effort-based, budget = 0)
    {"gpt-5", "openai", true, 0},
    {"gpt-5-mini", "openai", true, 0},
    {"gpt-5-nano", "openai", true, 0},

    // Google models (mixed: level-based for 3.x, budget for 2.5)
    {"gemini-3.0-flash", "google", true, 0},       // Level-based (LOW/HIGH)
    {"gemini-3.0-pro", "google", true, 0},         // Level-based (LOW/HIGH)
    {"gemini-2.5-pro", "google", true, 32768},     // Budget-based
    {"gemini-2.5-flash", "google", true, 24576},   // Budget-based
    {"gemini-2.5-flash-lite", "google", true, 24576},  // Budget-based

    // Legacy non-thinking OpenAI models
    {"gpt-4", "openai", false, 0},
    {"gpt-4-turbo", "openai", false, 0},
    {"gpt-4o", "openai", false, 0},
    {"gpt-4o-mini", "openai", false, 0},
    {"gpt-3.5-turbo", "openai", false, 0},

    // O-series models (legacy reasoning models, not GPT-5 compatible)
    {"o1", "openai", false, 0},
    {"o1-mini", "openai", false, 0},
    {"o1-preview", "openai", false, 0},
    {"o3", "openai", false, 0},
    {"o3-mini", "openai", false, 0},
};

static const size_t MODEL_CAPABILITIES_COUNT = sizeof(MODEL_CAPABILITIES) / sizeof(MODEL_CAPABILITIES[0]);

const char *ik_infer_provider(const char *model_name)
{
    if (model_name == NULL) {
        return NULL;
    }

    // OpenAI models: gpt-*, o1, o1-*, o3, o3-*
    if (strncmp(model_name, "gpt-", 4) == 0) {
        return "openai";
    }
    if (strcmp(model_name, "o1") == 0 || strncmp(model_name, "o1-", 3) == 0) {
        return "openai";
    }
    if (strcmp(model_name, "o3") == 0 || strncmp(model_name, "o3-", 3) == 0) {
        return "openai";
    }

    // Anthropic models: claude-*
    if (strncmp(model_name, "claude-", 7) == 0) {
        return "anthropic";
    }

    // Google models: gemini-*
    if (strncmp(model_name, "gemini-", 7) == 0) {
        return "google";
    }

    // Unknown model
    return NULL;
}

res_t ik_model_supports_thinking(const char *model, bool *supports)
{
    if (model == NULL || supports == NULL) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        res_t r = ERR(tmp_ctx, INVALID_ARG, "model and supports must not be NULL");
        return r;
    }

    // Search for exact match in capability table
    for (size_t i = 0; i < MODEL_CAPABILITIES_COUNT; i++) {
        if (strcmp(model, MODEL_CAPABILITIES[i].prefix) == 0) {
            *supports = MODEL_CAPABILITIES[i].supports_thinking;
            return OK(NULL);
        }
    }

    // Unknown model - assume no thinking support
    *supports = false;
    return OK(NULL);
}

res_t ik_model_get_thinking_budget(const char *model, int32_t *budget)
{
    if (model == NULL || budget == NULL) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        res_t r = ERR(tmp_ctx, INVALID_ARG, "model and budget must not be NULL");
        return r;
    }

    // Search for exact match in capability table
    for (size_t i = 0; i < MODEL_CAPABILITIES_COUNT; i++) {
        if (strcmp(model, MODEL_CAPABILITIES[i].prefix) == 0) {
            *budget = MODEL_CAPABILITIES[i].max_thinking_tokens;
            return OK(NULL);
        }
    }

    // Unknown model - return 0
    *budget = 0;
    return OK(NULL);
}
