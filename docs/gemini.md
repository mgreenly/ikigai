# Google Gemini

**API**: `https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent`

ikigai uses the `v1beta` API which includes early features that may have breaking changes. Google also offers a stable `v1` API.

## Models

### Gemini 3 (Level-based)

These models use thinking level strings.

| Model | Reasoning |
|-------|-----------|
| `gemini-3-flash-preview` | Yes |
| `gemini-3-pro-preview` | Yes |
| `gemini-3.1-pro-preview` | Yes |

Thinking cannot be fully disabled for Gemini 3 models — `min` maps to the minimum supported level.

#### Reasoning Levels

| Level | gemini-3-flash-preview | gemini-3-pro-preview | gemini-3.1-pro-preview |
|-------|------------------------|----------------------|------------------------|
| `min` | `"minimal"` | `"low"` | `"low"` |
| `low` | `"low"` | `"low"` | `"low"` |
| `med` | `"medium"` | `"high"` | `"medium"` |
| `high` | `"high"` | `"high"` | `"high"` |

`gemini-3-pro-preview` maps `med` to `"high"` because the model does not support a `"medium"` level.

### Gemini 2.5 (Budget-based)

These models use token budgets for thinking.

| Model | Reasoning | Min Budget | Max Budget |
|-------|-----------|------------|------------|
| `gemini-2.5-pro` | Yes | 128 | 32,768 |
| `gemini-2.5-flash` | Yes | 0 | 24,576 |
| `gemini-2.5-flash-lite` | Yes | 512 | 24,576 |

Only `gemini-2.5-flash` can fully disable thinking (min budget is 0). The other models always use some thinking budget.

#### Reasoning Levels

| Level | gemini-2.5-pro | gemini-2.5-flash | gemini-2.5-flash-lite |
|-------|----------------|------------------|----------------------|
| `min` | 128 tokens | 0 tokens | 512 tokens |
| `low` | 8,192 tokens | 8,192 tokens | 8,192 tokens |
| `med` | 16,384 tokens | 16,384 tokens | 16,384 tokens |
| `high` | 32,768 tokens | 24,576 tokens | 24,576 tokens |

## Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Provider default | Not sent in request |
| includeThoughts | `true` | Always enabled |

[Back to providers](providers.md)
