# Diagnosis: Anthropic Thinking Mode Returns 400

## Problem

When user sets `/model claude-sonnet-4-5/med`, requests to Anthropic API return HTTP 400.
`/model claude-sonnet-4-5/none` works fine.

## Root Cause Identified

The Anthropic API requires: `budget_tokens < max_tokens`

We were sending:
- `max_tokens: 4096` (default)
- `budget_tokens: 43008` (calculated for med level)

This violates the constraint and causes 400.

## Fix Applied

In `src/providers/anthropic/request.c`, added logic to ensure `max_tokens > budget_tokens`:

```c
// Ensure max_tokens > thinking budget when thinking is enabled
if (req->thinking.level != IK_THINKING_NONE) {
    int32_t budget = ik_anthropic_thinking_budget(req->model, req->thinking.level);
    if (budget > 0 && max_tokens <= budget) {
        max_tokens = budget + 4096;
    }
}
```

## User's Concern

User indicated the budget calculation itself might be broken - specifically the min/max lookup for calculating the 2/3 value.

### Current Budget Table (src/providers/anthropic/thinking.c)

```c
static const ik_anthropic_budget_t BUDGET_TABLE[] = {
    {"claude-sonnet-4-5", 1024, 64000},
    {"claude-haiku-4-5",  1024, 32000},
    {NULL, 0, 0} // Sentinel
};
```

### Budget Calculation Logic

```c
int32_t ik_anthropic_thinking_budget(const char *model, ik_thinking_level_t level)
{
    // Find budget limits for this model (prefix match via strncmp)
    int32_t min_budget = DEFAULT_MIN_BUDGET;  // 1024
    int32_t max_budget = DEFAULT_MAX_BUDGET;  // 32000

    for (size_t i = 0; BUDGET_TABLE[i].model_pattern != NULL; i++) {
        if (strncmp(model, BUDGET_TABLE[i].model_pattern,
                    strlen(BUDGET_TABLE[i].model_pattern)) == 0) {
            min_budget = BUDGET_TABLE[i].min_budget;
            max_budget = BUDGET_TABLE[i].max_budget;
            break;
        }
    }

    int32_t range = max_budget - min_budget;

    switch (level) {
        case IK_THINKING_NONE: return min_budget;
        case IK_THINKING_LOW:  return min_budget + range / 3;
        case IK_THINKING_MED:  return min_budget + (2 * range) / 3;
        case IK_THINKING_HIGH: return max_budget;
    }
}
```

### What UI Shows

`/model claude-sonnet-4-5/med` shows "Thinking: medium (43008 tokens)"

Calculation: 1024 + (2 * 62976) / 3 = 1024 + 41984 = 43008

## Questions to Investigate

1. Is the model name being passed to `ik_anthropic_thinking_budget()` exactly "claude-sonnet-4-5"?
2. Is the strncmp prefix match working correctly?
3. Are the min/max values in BUDGET_TABLE correct for current Anthropic API limits?
4. Should the default fallback (32000 max) be used instead of the table values?

## Files Changed in This Session

### Token Display Feature (completed)
- `src/agent.h` - Added response_input_tokens, response_output_tokens, response_thinking_tokens
- `src/agent.c` - Initialize new fields
- `src/repl_callbacks.c` - Store tokens, display subdued token line via `display_token_usage()`
- `src/repl_event_handlers.c` - Persist all token counts to data_json
- `src/event_render.c` - Render token line from data_json on replay via `render_token_usage()`
- Various test files updated to use new field names

### Thinking Budget Fix (partial)
- `src/providers/anthropic/request.c` - Added max_tokens adjustment when thinking enabled

## Test Status

`make check` passes after all changes.

## Next Steps

1. Verify the budget calculation is correct
2. Test with live API after fix
3. Consider if BUDGET_TABLE values need updating
