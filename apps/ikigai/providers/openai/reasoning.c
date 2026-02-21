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
 * Reasoning model lookup table
 *
 * All OpenAI models that support reasoning/thinking parameters.
 * - o-series: o1, o3, o3-mini, o4-mini, o3-pro
 * - GPT-5.x: gpt-5, gpt-5-mini, gpt-5-nano, gpt-5-pro, gpt-5.1/5.2 variants
 */
static const char *REASONING_MODELS[] = {
    "o1",
    "o3",
    "o3-mini",
    "o4-mini",
    "o3-pro",
    "gpt-5",
    "gpt-5-mini",
    "gpt-5-nano",
    "gpt-5-pro",
    "gpt-5.1",
    "gpt-5.1-chat-latest",
    "gpt-5.1-codex",
    "gpt-5.1-codex-mini",
    "gpt-5.2",
    "gpt-5.2-chat-latest",
    "gpt-5.2-codex",
    "gpt-5.2-pro",
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

typedef struct {
    const char *model;
    const char *min;
    const char *low;
    const char *med;
    const char *high;
} ik_effort_map_t;

static const ik_effort_map_t EFFORT_MAP[] = {
    /* Old o-series: cannot disable reasoning, max "high" */
    { "o1",                  "low",     "low",  "medium", "high"  },
    { "o3-mini",             "low",     "low",  "medium", "high"  },
    /* New o-series: can disable, max "high" */
    { "o3",                  "none",    "low",  "medium", "high"  },
    { "o3-pro",              "none",    "low",  "medium", "high"  },
    { "o4-mini",             "none",    "low",  "medium", "high"  },
    /* gpt-5 base: min "minimal" (API rejects "none"), max "high" */
    { "gpt-5",               "minimal", "low",  "medium", "high"  },
    { "gpt-5-mini",          "minimal", "low",  "medium", "high"  },
    { "gpt-5-nano",          "minimal", "low",  "medium", "high"  },
    /* gpt-5-pro: always high effort regardless of level */
    { "gpt-5-pro",           "high",    "high", "high",   "high"  },
    /* gpt-5.1: min "none", max "high" (no minimal, no xhigh) */
    { "gpt-5.1",             "none",    "low",  "medium", "high"  },
    /* gpt-5.1-chat-latest: fixed "medium" — adaptive reasoning, API rejects all other values */
    { "gpt-5.1-chat-latest", "medium",  "medium", "medium", "medium" },
    { "gpt-5.1-codex",       "none",    "low",  "medium", "high"  },
    { "gpt-5.1-codex-mini",  "none",    "low",  "medium", "high"  },
    /* gpt-5.2: min "none", max "xhigh" */
    { "gpt-5.2",             "none",    "low",  "medium", "xhigh" },
    /* gpt-5.2-chat-latest: fixed "medium" — adaptive reasoning, API rejects all other values */
    { "gpt-5.2-chat-latest", "medium",  "medium", "medium", "medium" },
    { "gpt-5.2-codex",       "none",    "low",  "medium", "xhigh" },
    { "gpt-5.2-pro",         "medium",  "medium", "high",  "xhigh" },
    { NULL, NULL, NULL, NULL, NULL }
};

const char *ik_openai_reasoning_effort(const char *model, ik_thinking_level_t level)
{
    if (model == NULL) {
        return NULL;
    }

    for (size_t i = 0; EFFORT_MAP[i].model != NULL; i++) {
        if (strcmp(model, EFFORT_MAP[i].model) == 0) {
            switch (level) {
                case IK_THINKING_MIN: return EFFORT_MAP[i].min;
                case IK_THINKING_LOW:  return EFFORT_MAP[i].low;
                case IK_THINKING_MED:  return EFFORT_MAP[i].med;
                case IK_THINKING_HIGH: return EFFORT_MAP[i].high;
                default:               return NULL;
            }
        }
    }

    return NULL;
}

/**
 * Models that use Responses API (not Chat Completions API)
 *
 * Hardcoded mapping table - no heuristics. Unknown models default to Chat Completions API.
 */
static const char *RESPONSES_API_MODELS[] = {
    "o1",
    "o3",
    "o3-mini",
    "o4-mini",
    "o3-pro",
    "gpt-5",
    "gpt-5-mini",
    "gpt-5-nano",
    "gpt-5-pro",
    "gpt-5.1",
    "gpt-5.1-chat-latest",
    "gpt-5.1-codex",
    "gpt-5.1-codex-mini",
    "gpt-5.2",
    "gpt-5.2-chat-latest",
    "gpt-5.2-codex",
    "gpt-5.2-pro",
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
