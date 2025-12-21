# Thinking/Reasoning Abstraction

## Overview

ikigai provides a unified `/model NAME/THINKING` command syntax where `THINKING` is one of: `none`, `low`, `med`, `high`. Provider adapters translate these levels to provider-specific parameters.

## Unified Thinking Levels

```c
typedef enum {
    IK_THINKING_NONE,   // Disabled or minimum
    IK_THINKING_LOW,    // ~1/3 of max budget
    IK_THINKING_MED,    // ~2/3 of max budget
    IK_THINKING_HIGH    // Maximum budget
} ik_thinking_level_t;
```

## Provider-Specific Mappings

### Anthropic (Token Budget)

Anthropic uses `thinking.budget_tokens` parameter.

**Model budgets (from research):**

| Model | Min | Max | Notes |
|-------|-----|-----|-------|
| claude-sonnet-4-5 | 1,024 | 64,000 | Standard model |
| claude-opus-4-5 | 1,024 | 64,000 | Most capable |
| claude-haiku-4-5 | 1,024 | 32,000 | Fast/cheap |
| claude-3-7-sonnet | 1,024 | 32,000 | Extended thinking |

**Mapping formula:**

```c
budget = min + (level / 3) * (max - min)

Where level is:
  none → 0
  low  → 1
  med  → 2
  high → 3
```

**Examples (Sonnet 4.5, min=1024, max=64000):**

```
none → 1,024   (minimum)
low  → 22,016  (1024 + 1/3 * 62,976)
med  → 43,008  (1024 + 2/3 * 62,976)
high → 64,000  (maximum)
```

**Wire format:**

```json
{
  "thinking": {
    "type": "enabled",
    "budget_tokens": 43008
  }
}
```

**Implementation:**

```c
typedef struct {
    const char *model_pattern;
    int32_t min_budget;
    int32_t max_budget;
} anthropic_thinking_budget_t;

static const anthropic_thinking_budget_t ANTHROPIC_BUDGETS[] = {
    {"claude-sonnet-4-5", 1024, 64000},
    {"claude-opus-4-5", 1024, 64000},
    {"claude-haiku-4-5", 1024, 32000},
    {"claude-3-7-sonnet", 1024, 32000},
    {NULL, 0, 0}
};

int32_t ik_anthropic_thinking_budget(const char *model,
                                      ik_thinking_level_t level)
{
    // Find model budget
    const anthropic_thinking_budget_t *budget = NULL;
    for (size_t i = 0; ANTHROPIC_BUDGETS[i].model_pattern != NULL; i++) {
        if (strstr(model, ANTHROPIC_BUDGETS[i].model_pattern)) {
            budget = &ANTHROPIC_BUDGETS[i];
            break;
        }
    }

    if (budget == NULL) {
        // Unknown model - use conservative defaults
        budget = &ANTHROPIC_BUDGETS[0];  // Sonnet defaults
    }

    // Calculate budget
    switch (level) {
        case IK_THINKING_NONE:
            return budget->min_budget;
        case IK_THINKING_LOW:
            return budget->min_budget +
                   (budget->max_budget - budget->min_budget) / 3;
        case IK_THINKING_MED:
            return budget->min_budget +
                   2 * (budget->max_budget - budget->min_budget) / 3;
        case IK_THINKING_HIGH:
            return budget->max_budget;
        default:
            return budget->min_budget;
    }
}
```

### Google (Mixed: Budget for 2.5, Level for 3)

Google uses **different parameters for different model series:**
- **Gemini 2.5:** `thinkingBudget` (token count)
- **Gemini 3:** `thinkingLevel` ("LOW" or "HIGH")

**Model budgets:**

| Model | Min | Max | Type |
|-------|-----|-----|------|
| gemini-2.5-pro | 128 | 32,768 | Budget |
| gemini-2.5-flash | 0 | 24,576 | Budget |
| gemini-2.5-flash-lite | 512 | 24,576 | Budget |
| gemini-3-pro | - | - | Level (LOW/HIGH) |

**Mapping (Gemini 2.5 Pro):**

```
none → 128     (minimum, cannot disable)
low  → 11,008  (128 + 1/3 * (32768 - 128))
med  → 21,760  (128 + 2/3 * (32768 - 128))
high → 32,768  (maximum)
```

**Mapping (Gemini 2.5 Flash):**

```
none → 0       (disabled)
low  → 8,192   (1/3 of 24,576)
med  → 16,384  (2/3 of 24,576)
high → 24,576  (maximum)
```

**Mapping (Gemini 3 Pro):**

```
none → "LOW"   (cannot disable)
low  → "LOW"
med  → "HIGH"  (round up)
high → "HIGH"
```

**Wire format (2.5 series):**

```json
{
  "generationConfig": {
    "thinkingConfig": {
      "thinkingBudget": 21760,
      "includeThoughts": true
    }
  }
}
```

**Wire format (3 series):**

```json
{
  "generationConfig": {
    "thinkingConfig": {
      "thinkingLevel": "HIGH",
      "includeThoughts": true
    }
  }
}
```

**Implementation:**

```c
typedef struct {
    const char *model_pattern;
    int32_t min_budget;
    int32_t max_budget;
    bool uses_level;  // true for Gemini 3
} google_thinking_config_t;

static const google_thinking_config_t GOOGLE_CONFIGS[] = {
    {"gemini-2.5-pro", 128, 32768, false},
    {"gemini-2.5-flash-lite", 512, 24576, false},
    {"gemini-2.5-flash", 0, 24576, false},
    {"gemini-3-pro", 0, 0, true},
    {NULL, 0, 0, false}
};

res_t ik_google_serialize_thinking(yyjson_mut_doc *doc,
                                   yyjson_mut_val *config,
                                   const char *model,
                                   ik_thinking_level_t level)
{
    const google_thinking_config_t *cfg = find_config(model);

    yyjson_mut_val *thinking_cfg = yyjson_mut_obj(doc);

    if (cfg->uses_level) {
        // Gemini 3: Use thinkingLevel
        const char *level_str = (level <= IK_THINKING_LOW) ? "LOW" : "HIGH";
        yyjson_mut_obj_add_str(doc, thinking_cfg, "thinkingLevel", level_str);
    } else {
        // Gemini 2.5: Use thinkingBudget
        int32_t budget;
        switch (level) {
            case IK_THINKING_NONE:
                budget = cfg->min_budget;
                break;
            case IK_THINKING_LOW:
                budget = cfg->min_budget +
                        (cfg->max_budget - cfg->min_budget) / 3;
                break;
            case IK_THINKING_MED:
                budget = cfg->min_budget +
                        2 * (cfg->max_budget - cfg->min_budget) / 3;
                break;
            case IK_THINKING_HIGH:
                budget = cfg->max_budget;
                break;
        }
        yyjson_mut_obj_add_int(doc, thinking_cfg, "thinkingBudget", budget);
    }

    yyjson_mut_obj_add_bool(doc, thinking_cfg, "includeThoughts", true);
    yyjson_mut_obj_add_val(doc, config, "thinkingConfig", thinking_cfg);

    return OK(NULL);
}
```

### OpenAI (Effort Level)

OpenAI uses `reasoning.effort` parameter (Responses API) or `reasoning_effort` (Chat Completions API).

**Supported values:**
- o3, o4-mini, GPT-5 models: `"none"`, `"minimal"`, `"low"`, `"medium"`, `"high"`, `"xhigh"`
- o1, o3-mini models: `"low"`, `"medium"`, `"high"` (no "none")

**Mapping:**

```
none → "none"    (o3/o4-mini only; omit for o1/o3-mini)
low  → "low"
med  → "medium"
high → "high"
```

**Wire format (Responses API):**

```json
{
  "reasoning": {
    "effort": "medium",
    "summary": "auto"
  }
}
```

**Wire format (Chat Completions API):**

```json
{
  "reasoning_effort": "medium"
}
```

**Implementation:**

```c
const char *ik_openai_thinking_effort(const char *model,
                                       ik_thinking_level_t level)
{
    bool supports_none = (strstr(model, "o3") != NULL ||
                         strstr(model, "o4-mini") != NULL ||
                         strstr(model, "gpt-5") != NULL);

    switch (level) {
        case IK_THINKING_NONE:
            return supports_none ? "none" : "medium";  // Fallback to medium
        case IK_THINKING_LOW:
            return "low";
        case IK_THINKING_MED:
            return "medium";
        case IK_THINKING_HIGH:
            return "high";
        default:
            return "medium";
    }
}

res_t ik_openai_serialize_thinking(yyjson_mut_doc *doc,
                                   yyjson_mut_val *root,
                                   const char *model,
                                   ik_thinking_level_t level,
                                   bool use_responses_api)
{
    const char *effort = ik_openai_thinking_effort(model, level);

    if (use_responses_api) {
        // Responses API
        yyjson_mut_val *reasoning = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, reasoning, "effort", effort);
        yyjson_mut_obj_add_str(doc, reasoning, "summary", "auto");
        yyjson_mut_obj_add_val(doc, root, "reasoning", reasoning);
    } else {
        // Chat Completions API
        yyjson_mut_obj_add_str(doc, root, "reasoning_effort", effort);
    }

    return OK(NULL);
}
```

## User Feedback

When user sets thinking level, provide feedback about what it means:

### Anthropic

```
> /model claude-sonnet-4-5/med

✓ Switched to Anthropic claude-sonnet-4-5
  Thinking: medium (43,008 tokens)
```

### Google

```
> /model gemini-2.5-pro/high

✓ Switched to Google gemini-2.5-pro
  Thinking: high (32,768 tokens)
```

```
> /model gemini-3-pro/none

✓ Switched to Google gemini-3-pro
  ⚠ This model does not support disabling thinking
  Thinking: LOW level (minimum)
```

### OpenAI

```
> /model o3/med

✓ Switched to OpenAI o3 (Responses API)
  Thinking: medium effort (~50% of output budget)
  Recommended output buffer: 25,000 tokens
```

```
> /model o3-mini/none

✓ Switched to OpenAI o3-mini
  ⚠ This model does not support disabling thinking
  Thinking: medium effort (default)
```

## Thinking Summary Display

Providers may expose thinking content:

### Anthropic
- **Sonnet 3.7:** Full thinking content (may be very long)
- **Sonnet 4.5+:** Summary only

### Google
- **All models:** Summary with `thought: true` flag

### OpenAI
- **o-series:** Summary only (when using `reasoning.summary: "auto"`)
- **Not exposed by default** - count only

**Display strategy:**

```c
void display_thinking(ik_scrollback_t *sb, const char *provider,
                     const char *thinking_text)
{
    if (thinking_text == NULL || strlen(thinking_text) == 0) {
        return;  // No thinking content
    }

    // Add to scrollback with different styling
    ik_scrollback_add_thinking_block(sb, thinking_text);

    // Could be collapsed by default, expanded on click
    // Or shown in separate pane
}
```

## Thought Signatures (Google Gemini 3)

Gemini 3 requires thought signatures for function calling:

**Storage:**

```c
// In provider_data field
yyjson_mut_val *provider_data = yyjson_mut_obj(doc);
yyjson_mut_obj_add_str(doc, provider_data, "thought_signature", signature);
```

**Resubmission:**

```c
// When building next request, include signature
if (prev_message->provider_data) {
    const char *sig = yyjson_obj_get_str(prev_message->provider_data,
                                        "thought_signature");
    if (sig != NULL) {
        // Add to parts in request
        yyjson_mut_val *sig_part = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, sig_part, "thoughtSignature", sig);
        yyjson_mut_arr_append(parts, sig_part);
    }
}
```

## Testing

### Thinking Budget Calculation

```c
START_TEST(test_anthropic_thinking_budget) {
    // Sonnet 4.5: min=1024, max=64000
    ck_assert_int_eq(
        ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_NONE),
        1024
    );

    ck_assert_int_eq(
        ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_LOW),
        22016  // 1024 + 1/3 * 62976
    );

    ck_assert_int_eq(
        ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_MED),
        43008  // 1024 + 2/3 * 62976
    );

    ck_assert_int_eq(
        ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_HIGH),
        64000
    );
}
END_TEST
```

### Provider-Specific Serialization

```c
START_TEST(test_google_gemini_3_thinking_level) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *config = yyjson_mut_obj(doc);

    ik_google_serialize_thinking(doc, config, "gemini-3-pro", IK_THINKING_HIGH);

    // Should use thinkingLevel, not thinkingBudget
    const char *json = yyjson_mut_write(doc, 0, NULL);
    ck_assert_str_contains(json, "\"thinkingLevel\":\"HIGH\"");
    ck_assert_str_not_contains(json, "thinkingBudget");

    free(json);
    yyjson_mut_doc_free(doc);
}
END_TEST
```

## Future Extensions

### Custom Budgets

Allow users to override budget calculation:

```
/model claude-sonnet-4-5/med --thinking-budget=50000
```

Implementation deferred to rel-08+.

### Dynamic Budget Adjustment

Automatically reduce budget for long conversations:

```c
if (context_tokens > 150000) {
    // Reduce thinking budget to leave room
    thinking_budget = min(thinking_budget, 10000);
}
```

Implementation deferred to rel-08+.
