# Thinking/Reasoning Abstraction

## Overview

ikigai provides a unified `/model NAME/THINKING` command syntax where `THINKING` is one of: `none`, `low`, `med`, `high`. Provider adapters translate these levels to provider-specific parameters.

## Unified Thinking Levels

- `none` - Disabled or minimum thinking
- `low` - Approximately 1/3 of maximum budget
- `med` - Approximately 2/3 of maximum budget
- `high` - Maximum budget

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

```
budget = min + (level_value / 3) * (max - min)

Where level_value is:
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

**Configuration table:**

| Model Pattern | Min Budget | Max Budget |
|---------------|------------|------------|
| claude-sonnet-4-5 | 1024 | 64000 |
| claude-opus-4-5 | 1024 | 64000 |
| claude-haiku-4-5 | 1024 | 32000 |
| claude-3-7-sonnet | 1024 | 32000 |

**Implementation logic:**

1. Find matching model pattern from configuration table
2. If no match, use Sonnet defaults (1024-64000)
3. Calculate budget using formula above
4. Return minimum for `none`, maximum for `high`

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

**Configuration table:**

| Model Pattern | Min Budget | Max Budget | Uses Level |
|---------------|------------|------------|------------|
| gemini-2.5-pro | 128 | 32768 | false |
| gemini-2.5-flash-lite | 512 | 24576 | false |
| gemini-2.5-flash | 0 | 24576 | false |
| gemini-3-pro | 0 | 0 | true |

**Implementation logic:**

1. Find matching model pattern from configuration table
2. If `uses_level` is true (Gemini 3):
   - Map `none`/`low` → "LOW"
   - Map `med`/`high` → "HIGH"
   - Add to `thinkingLevel` field
3. If `uses_level` is false (Gemini 2.5):
   - Calculate budget using standard formula
   - Add to `thinkingBudget` field
4. Always set `includeThoughts: true`

### OpenAI (Effort Level)

OpenAI uses `reasoning.effort` parameter (Responses API) or `reasoning_effort` (Chat Completions API).

**Supported values:**
- o3, o4-mini, GPT-5 models: `"none"`, `"minimal"`, `"low"`, `"medium"`, `"high"`, `"xhigh"`
- o1, o3-mini models: `"low"`, `"medium"`, `"high"` (no "none")

**Mapping:**

```
none → "none"    (o3/o4-mini only; fallback to "medium" for o1/o3-mini)
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

**Model capabilities:**

| Model Pattern | Supports None | Default Fallback |
|---------------|---------------|------------------|
| o3 | true | medium |
| o4-mini | true | medium |
| gpt-5 | true | medium |
| o1 | false | medium |
| o3-mini | false | medium |

**Implementation logic:**

1. Check if model supports "none" effort (o3, o4-mini, gpt-5)
2. Map level to effort string:
   - `none` → "none" if supported, else "medium"
   - `low` → "low"
   - `med` → "medium"
   - `high` → "high"
3. For Responses API: add `reasoning` object with `effort` and `summary: "auto"`
4. For Chat Completions API: add `reasoning_effort` string directly

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

1. If thinking text is null or empty, skip display
2. Add thinking content to scrollback with special styling
3. May be collapsed by default, expanded on click
4. Or shown in separate pane

## Thought Signatures (Google Gemini 3)

Gemini 3 requires thought signatures for function calling:

**Storage in provider_data:**

```json
{
  "thought_signature": "signature_value_here"
}
```

**Resubmission logic:**

1. When building next request, check previous message's `provider_data`
2. Extract `thought_signature` if present
3. Add signature part to request:

```json
{
  "parts": [
    {
      "thoughtSignature": "signature_value_here"
    }
  ]
}
```

## Testing

### Thinking Budget Calculation

**Test: Anthropic Sonnet 4.5 budgets**

| Level | Expected Budget | Calculation |
|-------|----------------|-------------|
| none | 1,024 | minimum |
| low | 22,016 | 1024 + 1/3 * 62976 |
| med | 43,008 | 1024 + 2/3 * 62976 |
| high | 64,000 | maximum |

**Test: Google Gemini 3 Pro uses level not budget**

- Should generate `"thinkingLevel":"HIGH"` for high setting
- Should NOT generate `thinkingBudget` field
- Should include `"includeThoughts":true`

## Future Extensions

### Custom Budgets

Allow users to override budget calculation:

```
/model claude-sonnet-4-5/med --thinking-budget=50000
```

Implementation deferred to rel-08+.

### Dynamic Budget Adjustment

Automatically reduce budget for long conversations:

**Logic:**
- If context tokens > 150,000
- Reduce thinking budget to min(current_budget, 10,000)
- Prevents exceeding total token limits

Implementation deferred to rel-08+.
