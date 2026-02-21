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
 * @return      "low", "medium", "high", "minimal", "xhigh", "none", or NULL
 *
 * Model-aware mapping:
 * | Level | o1, o3-mini | o3, o3-pro, o4-mini | gpt-5/mini/nano | gpt-5-pro | gpt-5.1.x | gpt-5.2.x |
 * |-------|-------------|---------------------|-----------------|-----------|-----------|-----------|
 * | NONE  | "low"       | "none"              | "minimal"       | "high"    | "none"    | "none"    |
 * | LOW   | "low"       | "low"               | "low"           | "high"    | "low"     | "low"     |
 * | MED   | "medium"    | "medium"            | "medium"        | "high"    | "medium"  | "medium"  |
 * | HIGH  | "high"      | "high"              | "high"          | "high"    | "high"    | "xhigh"   |
 */
const char *ik_openai_reasoning_effort(const char *model, ik_thinking_level_t level);

/**
 * Determine if model uses Responses API
 *
 * @param model Model identifier
 * @return      true if model uses Responses API (not Chat Completions API)
 *
 * Hardcoded lookup table:
 * - Responses API: o1, o3, o3-mini, o4-mini, o3-pro, gpt-5*, gpt-5.1*, gpt-5.2*
 * - Chat Completions API: gpt-4, gpt-4-turbo, gpt-4o, gpt-4o-mini, gpt-4.1*
 * - Unknown models: Default to Chat Completions API
 */
bool ik_openai_use_responses_api(const char *model);

#endif /* IK_PROVIDERS_OPENAI_REASONING_H */
