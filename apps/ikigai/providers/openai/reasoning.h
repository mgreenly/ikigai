/**
 * @file reasoning.h
 * @brief OpenAI reasoning effort mapping (internal)
 *
 * Converts provider-agnostic thinking levels to OpenAI-specific
 * reasoning.effort strings for o1/o3 reasoning models.
 */

#ifndef IK_PROVIDERS_OPENAI_REASONING_H
#define IK_PROVIDERS_OPENAI_REASONING_H

#include <stdbool.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * OpenAI model configuration entry
 *
 * Single source of truth for all OpenAI model properties.
 */
typedef struct {
    const char *model;           /* Model name (exact match) */
    bool uses_responses_api;     /* true if Responses API, false if Chat Completions API */
    const char *effort[4];       /* Effort strings indexed by ik_thinking_level_t (NONE/LOW/MED/HIGH) */
} ik_openai_model_entry_t;

/**
 * OpenAI model lookup table
 *
 * Unified table containing all OpenAI models with their API type and effort mappings.
 * A model is a "reasoning model" if it has any non-NULL effort entry.
 *
 * Effort mapping strategies:
 * 1. o-series (o1, o3): Use "low" for both NONE and LOW, "medium" for MED, "high" for HIGH
 * 2. gpt-5-pro: Always "high" (no thinking granularity)
 * 3. gpt-5, gpt-5.1: NULL for NONE (omit param), "low"/"medium"/"high" for LOW/MED/HIGH
 * 4. gpt-5.2, gpt-5.3-codex: Shifted mapping spanning "low"/"medium"/"high"/"xhigh" (xhigh support)
 */
static const ik_openai_model_entry_t OPENAI_MODELS[] = {
    // o-series reasoning models (Responses API)
    {"o1",         true,  {"low", "low", "medium", "high"}},
    {"o1-mini",    true,  {"low", "low", "medium", "high"}},
    {"o1-preview", true,  {"low", "low", "medium", "high"}},
    {"o3",         true,  {"low", "low", "medium", "high"}},
    {"o3-mini",    true,  {"low", "low", "medium", "high"}},

    // GPT-5 base models (Responses API)
    {"gpt-5",      true,  {NULL, "low", "medium", "high"}},
    {"gpt-5-mini", true,  {NULL, "low", "medium", "high"}},
    {"gpt-5-nano", true,  {NULL, "low", "medium", "high"}},
    {"gpt-5-pro",  true,  {"high", "high", "high", "high"}},

    // GPT-5.1 models (Responses API)
    {"gpt-5.1",              true,  {NULL, "low", "medium", "high"}},
    {"gpt-5.1-chat-latest",  true,  {NULL, "low", "medium", "high"}},
    {"gpt-5.1-codex",        true,  {NULL, "low", "medium", "high"}},

    // GPT-5.2 models with xhigh support (Responses API, shifted mapping)
    {"gpt-5.2",              true,  {"low", "medium", "high", "xhigh"}},
    {"gpt-5.2-chat-latest",  true,  {"low", "medium", "high", "xhigh"}},
    {"gpt-5.2-codex",        true,  {"low", "medium", "high", "xhigh"}},

    // GPT-5.3 models with xhigh support (Responses API, shifted mapping)
    {"gpt-5.3-codex",        true,  {"low", "medium", "high", "xhigh"}},

    // Sentinel
    {NULL, false, {NULL, NULL, NULL, NULL}}
};

/**
 * Check if model is a reasoning model
 *
 * @param model Model identifier (e.g., "o1", "o3-mini", "gpt-4o")
 * @return      true if model supports reasoning.effort parameter
 *
 * Reasoning models are identified by prefix:
 * - "o1", "o1-*" (e.g., o1-mini, o1-preview)
 * - "o3", "o3-*" (e.g., o3-mini)
 * - "o4", "o4-*" (future models)
 *
 * Validation: Character after prefix must be '\0', '-', or '_' to avoid
 * false matches (e.g., "o30" is NOT a reasoning model).
 */
bool ik_openai_is_reasoning_model(const char *model);

/**
 * Map thinking level to reasoning effort string (model-aware)
 *
 * @param model Model identifier (e.g., "o1", "gpt-5", "gpt-5-pro")
 * @param level Thinking level (NONE/LOW/MED/HIGH)
 * @return      "low", "medium", "high", or NULL
 *
 * Model-aware mapping:
 * | Our Level | o1, o3 family | gpt-5, gpt-5.1, gpt-5.2 | gpt-5-pro |
 * |-----------|---------------|-------------------------|-----------|
 * | NONE      | "low"         | NULL (omit param)       | "high"    |
 * | LOW       | "low"         | "low"                   | "high"    |
 * | MED       | "medium"      | "medium"                | "high"    |
 * | HIGH      | "high"        | "high"                  | "high"    |
 */
const char *ik_openai_reasoning_effort(const char *model, ik_thinking_level_t level);

/**
 * Determine if model uses Responses API
 *
 * @param model Model identifier
 * @return      true if model uses Responses API (not Chat Completions API)
 *
 * Hardcoded lookup table:
 * - Responses API: o1, o1-mini, o1-preview, o3, o3-mini, gpt-5*, gpt-5.1*, gpt-5.2*
 * - Chat Completions API: gpt-4, gpt-4-turbo, gpt-4o, gpt-4o-mini
 * - Unknown models: Default to Chat Completions API
 */
bool ik_openai_use_responses_api(const char *model);

/**
 * Validate thinking level for model
 *
 * @param ctx   Talloc context for error allocation
 * @param model Model identifier
 * @param level Thinking level to validate
 * @return      OK if valid, ERR with message if invalid
 *
 * Validation rules:
 * - NULL model: ERR(INVALID_ARG)
 * - Reasoning models (o1/o3): All levels valid (NONE/LOW/MED/HIGH)
 * - Non-reasoning models (gpt-*): Only NONE is valid, others return ERR
 */
res_t ik_openai_validate_thinking(TALLOC_CTX *ctx, const char *model, ik_thinking_level_t level);

#endif /* IK_PROVIDERS_OPENAI_REASONING_H */
