/**
 * @file thinking.h
 * @brief Google thinking budget/level calculation (internal)
 *
 * Converts provider-agnostic thinking levels to Google-specific
 * thinking_budget (Gemini 2.5) or thinking_level (Gemini 3) values.
 */

#ifndef IK_PROVIDERS_GOOGLE_THINKING_H
#define IK_PROVIDERS_GOOGLE_THINKING_H

#include <stdbool.h>
#include <stdint.h>
#include "apps/ikigai/providers/provider.h"

/**
 * Gemini series classification
 */
typedef enum {
    IK_GEMINI_2_5 = 0,  /* Gemini 2.5 models (budget-based) */
    IK_GEMINI_3 = 1,    /* Gemini 3.x models (level-based) */
    IK_GEMINI_OTHER = 2 /* Other Gemini models (no thinking support) */
} ik_gemini_series_t;

/**
 * Determine which Gemini series a model belongs to
 *
 * @param model Model identifier (e.g., "gemini-2.5-pro", "gemini-3-pro")
 * @return      Gemini series classification
 *
 * Classification rules:
 * - Contains "gemini-3" -> IK_GEMINI_3
 * - Contains "gemini-2.5" -> IK_GEMINI_2_5
 * - Otherwise -> IK_GEMINI_OTHER
 * - NULL -> IK_GEMINI_OTHER
 */
ik_gemini_series_t ik_google_model_series(const char *model);

/**
 * Calculate thinking budget for Gemini 2.5 models
 *
 * @param model Model identifier
 * @param level Thinking level (NONE/LOW/MED/HIGH)
 * @return      Thinking budget in tokens, or -1 if not applicable
 *
 * Only applies to Gemini 2.5 models. Returns -1 for Gemini 3 (uses levels).
 *
 * Budget calculation:
 * - NONE: min_budget (0 or model minimum)
 * - LOW:  min_budget + range/3
 * - MED:  min_budget + 2*range/3
 * - HIGH: max_budget
 *
 * Model-specific budgets:
 * - gemini-2.5-pro:        min=128, max=32768 (cannot disable)
 * - gemini-2.5-flash:      min=0, max=24576 (can disable)
 * - gemini-2.5-flash-lite: min=512, max=24576 (cannot fully disable)
 * - Unknown 2.5 models:    min=0, max=24576 (default)
 */
int32_t ik_google_thinking_budget(const char *model, ik_thinking_level_t level);

/**
 * Get thinking level string for Gemini 3 models
 *
 * @param model Model identifier (e.g., "gemini-3-flash-preview")
 * @param level Thinking level
 * @return      Lowercase level string for the Gemini 3 API
 *
 * Only applies to Gemini 3 models. Returns "low" as safe default
 * if model is not found in the level table.
 *
 * Per-model mapping:
 * - gemini-3-flash-preview:  NONE->"minimal", LOW->"low", MED->"medium", HIGH->"high"
 * - gemini-3-pro-preview:    NONE->"low",     LOW->"low", MED->"high",   HIGH->"high"
 * - gemini-3.1-pro-preview:  NONE->"low",     LOW->"low", MED->"medium", HIGH->"high"
 */
const char *ik_google_thinking_level_str(const char *model, ik_thinking_level_t level);

/**
 * Check if model supports thinking mode
 *
 * @param model Model identifier
 * @return      true if model supports thinking, false otherwise
 *
 * Gemini 2.5 and 3.x models support thinking.
 * Other Gemini models do not.
 * NULL returns false.
 */
bool ik_google_supports_thinking(const char *model);

/**
 * Check if thinking can be disabled for model
 *
 * @param model Model identifier
 * @return      true if thinking can be disabled (min_budget=0), false otherwise
 *
 * For Gemini 2.5 models:
 * - gemini-2.5-flash: Can disable (min=0)
 * - gemini-2.5-pro: Cannot disable (min=128)
 * - gemini-2.5-flash-lite: Cannot fully disable (min=512)
 *
 * For Gemini 3 models: Returns false (uses levels, not budgets)
 * For non-thinking models: Returns false
 */
bool ik_google_can_disable_thinking(const char *model);

/**
 * Validate thinking level for model
 *
 * @param ctx   Talloc context for error allocation
 * @param model Model identifier
 * @param level Thinking level to validate
 * @return      OK if valid, ERR with message if invalid
 *
 * Validation rules:
 * - NULL model -> ERR(INVALID_ARG)
 * - Gemini 2.5 models that can disable (min=0): All levels valid
 * - Gemini 2.5 models that cannot disable (min>0): NONE returns ERR, others valid
 * - Gemini 3 models: All levels valid (NONE means don't include thinking config)
 * - Non-thinking models: Only NONE is valid, others return ERR
 */
res_t ik_google_validate_thinking(TALLOC_CTX *ctx, const char *model, ik_thinking_level_t level);

#endif /* IK_PROVIDERS_GOOGLE_THINKING_H */
