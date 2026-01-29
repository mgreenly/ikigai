# AI Providers

ikigai supports three AI providers. Each requires an API key set via environment variable.

| Provider | Environment Variable |
|----------|---------------------|
| Anthropic | `ANTHROPIC_API_KEY` |
| OpenAI | `OPENAI_API_KEY` |
| Google | `GOOGLE_API_KEY` |

Provider is auto-detected from model name prefix:
- `claude-*` → Anthropic
- `gpt-*`, `o1-*`, `o3-*` → OpenAI
- `gemini-*` → Google

## Supported Models

### Anthropic

| Model | Thinking Support | Max Thinking Tokens |
|-------|------------------|---------------------|
| `claude-opus-4-5` | Yes | 64,000 |
| `claude-sonnet-4-5` | Yes | 64,000 |
| `claude-haiku-4-5` | Yes | 32,000 |

### OpenAI

| Model | Thinking Support | Notes |
|-------|------------------|-------|
| `gpt-5` | Yes | Effort-based |
| `gpt-5-mini` | Yes | Effort-based |
| `gpt-5-nano` | Yes | Effort-based |
| `o1`, `o1-mini`, `o1-preview` | No | Legacy reasoning (not GPT-5 API compatible) |
| `o3`, `o3-mini` | No | Legacy reasoning (not GPT-5 API compatible) |
| `gpt-4`, `gpt-4-turbo`, `gpt-4o`, `gpt-4o-mini` | No | |
| `gpt-3.5-turbo` | No | |

### Google

| Model | Thinking Support | Type | Max Thinking Tokens |
|-------|------------------|------|---------------------|
| `gemini-3-pro-preview` | Yes | Level-based | N/A |
| `gemini-3-flash-preview` | Yes | Level-based | N/A |
| `gemini-2.5-pro` | Yes | Budget-based | 32,768 |
| `gemini-2.5-flash` | Yes | Budget-based | 24,576 |
| `gemini-2.5-flash-lite` | Yes | Budget-based | 24,576 |

## Thinking Levels

ikigai uses four abstract thinking levels that map to provider-specific values:

```c
typedef enum {
    IK_THINKING_NONE = 0,
    IK_THINKING_LOW  = 1,
    IK_THINKING_MED  = 2,
    IK_THINKING_HIGH = 3
} ik_thinking_level_t;
```

### Anthropic Mapping

Anthropic uses token budgets. The formula divides the range between min and max into thirds:

```
range = max_budget - min_budget

NONE: min_budget
LOW:  min_budget + range/3
MED:  min_budget + 2*range/3
HIGH: max_budget
```

| Level | claude-sonnet-4-5 | claude-haiku-4-5 |
|-------|-------------------|------------------|
| NONE | 1,024 | 1,024 |
| LOW | 22,032 | 11,349 |
| MED | 43,040 | 21,674 |
| HIGH | 64,000 | 32,000 |

Note: Anthropic's minimum budget is 1,024 tokens. `NONE` still enables thinking at minimum.

### OpenAI Mapping

OpenAI reasoning models (gpt-5 series) use effort strings:

| Level | API Value |
|-------|-----------|
| NONE | `null` (omit reasoning config) |
| LOW | `"low"` |
| MED | `"medium"` |
| HIGH | `"high"` |

Non-reasoning models (gpt-4, etc.) only support `NONE`.

### Google Mapping

Google has two thinking mechanisms depending on model series.

**Gemini 3 (level-based):**

| Level | API Value |
|-------|-----------|
| NONE | `"minimal"` |
| LOW | `"low"` |
| MED | `"medium"` |
| HIGH | `"high"` |

**Gemini 2.5 (budget-based):**

Uses the same thirds formula as Anthropic:

| Level | gemini-2.5-pro | gemini-2.5-flash | gemini-2.5-flash-lite |
|-------|----------------|------------------|----------------------|
| NONE | 128* | 0 | 0 |
| LOW | 11,008 | 8,192 | 8,192 |
| MED | 21,888 | 16,384 | 16,384 |
| HIGH | 32,768 | 24,576 | 24,576 |

*gemini-2.5-pro cannot fully disable thinking (minimum 128 tokens).

## Model-Specific Constraints

Some models have constraints on thinking levels:

| Model | Constraint |
|-------|------------|
| `gemini-2.5-pro` | Cannot use `NONE` (min budget = 128) |
| `gemini-3-*` | Cannot use `NONE` (always has minimal thinking) |
| OpenAI non-reasoning | Only `NONE` supported |

## Source Files

- `src/providers/provider.c` - Model capability table
- `src/providers/anthropic/thinking.c` - Anthropic budget calculation
- `src/providers/openai/reasoning.c` - OpenAI effort mapping
- `src/providers/google/thinking.c` - Google level/budget mapping
