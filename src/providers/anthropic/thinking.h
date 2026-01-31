/**
 * @file thinking.h
 * @brief Anthropic thinking budget calculation (internal)
 *
 * Converts provider-agnostic thinking levels to Anthropic-specific
 * budget_tokens values based on model capabilities.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_THINKING_H
#define IK_PROVIDERS_ANTHROPIC_THINKING_H

#include <stdbool.h>
#include <stdint.h>
#include "providers/provider.h"

/**
 * Calculate thinking budget tokens for model and level
 *
 * @param model Model identifier (e.g., "claude-sonnet-4-5")
 * @param level Thinking level (NONE/LOW/MED/HIGH)
 * @return      Thinking budget in tokens (power of 2), or -1 if unsupported
 *
 * Budget calculation (LOW/MED rounded down to nearest power of 2):
 * - NONE: min_budget (1024)
 * - LOW:  floor_power_of_2(min_budget + range/3)
 * - MED:  floor_power_of_2(min_budget + 2*range/3)
 * - HIGH: max_budget
 *
 * Model-specific max budgets (powers of 2):
 * - claude-sonnet-4-5: 65536 (2^16)
 * - claude-haiku-4-5:  32768 (2^15)
 * - Unknown Claude:    32768 (default)
 * - Non-Claude:        -1 (unsupported)
 */
int32_t ik_anthropic_thinking_budget(const char *model, ik_thinking_level_t level);

/**
 * Check if model supports extended thinking
 *
 * @param model Model identifier
 * @return      true if model supports thinking, false otherwise
 *
 * All Claude models support thinking. Non-Claude models return false.
 */
bool ik_anthropic_supports_thinking(const char *model);

#endif /* IK_PROVIDERS_ANTHROPIC_THINKING_H */
