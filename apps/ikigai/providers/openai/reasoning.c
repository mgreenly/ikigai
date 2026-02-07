/**
 * @file reasoning.c
 * @brief OpenAI reasoning effort implementation
 */

#include "apps/ikigai/providers/openai/reasoning.h"
#include "shared/panic.h"
#include <string.h>
#include <assert.h>


#include "shared/poison.h"
/**
 * Find model entry in unified table
 *
 * @param model Model identifier
 * @return      Pointer to entry, or NULL if not found
 */
static const ik_openai_model_entry_t *find_model_entry(const char *model)
{
    if (model == NULL || model[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; OPENAI_MODELS[i].model != NULL; i++) {
        if (strcmp(model, OPENAI_MODELS[i].model) == 0) {
            return &OPENAI_MODELS[i];
        }
    }

    return NULL;
}

bool ik_openai_is_reasoning_model(const char *model)
{
    const ik_openai_model_entry_t *entry = find_model_entry(model);
    if (entry == NULL) {
        return false;
    }

    // A model is a reasoning model if it has any non-NULL effort entry
    for (size_t i = 0; i < 4; i++) {
        if (entry->effort[i] != NULL) {
            return true;
        }
    }

    return false;
}

const char *ik_openai_reasoning_effort(const char *model, ik_thinking_level_t level)
{
    const ik_openai_model_entry_t *entry = find_model_entry(model);
    if (entry == NULL) {
        return NULL;
    }

    // Validate level range
    if (level < 0 || level > 3) {
        return NULL;
    }

    // Pure table lookup
    return entry->effort[level];
}

bool ik_openai_use_responses_api(const char *model)
{
    const ik_openai_model_entry_t *entry = find_model_entry(model);
    if (entry == NULL) {
        // Unknown models default to Chat Completions API
        return false;
    }

    // Pure table lookup
    return entry->uses_responses_api;
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
