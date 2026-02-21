# Anthropic

**API**: `https://api.anthropic.com/v1/messages`

## Models

### Claude 4.6 (Adaptive)

These models use effort-based reasoning. ikigai sends an effort string to the API.

| Model | Reasoning |
|-------|-----------|
| `claude-opus-4-6` | Yes |
| `claude-sonnet-4-6` | Yes |

#### Reasoning Levels

| Level | Effort Sent |
|-------|-------------|
| `none` | Thinking disabled (parameter omitted) |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

### Claude 4.5 (Budget-based)

These models use token budgets for reasoning. ikigai sends a token count to the API.

| Model | Reasoning | Min Budget | Max Budget |
|-------|-----------|------------|------------|
| `claude-opus-4-5` | Yes | 1,024 | 65,536 |
| `claude-sonnet-4-5` | Yes | 1,024 | 65,536 |
| `claude-haiku-4-5` | Yes | 1,024 | 32,768 |

Anthropic's minimum budget is 1,024 tokens. Setting `none` sends the minimum — it does not disable thinking.

#### Reasoning Levels

| Level | claude-opus-4-5 | claude-sonnet-4-5 | claude-haiku-4-5 |
|-------|-----------------|-------------------|------------------|
| `none` | 1,024 tokens | 1,024 tokens | 1,024 tokens |
| `low` | 16,384 tokens | 16,384 tokens | 8,192 tokens |
| `med` | 32,768 tokens | 32,768 tokens | 16,384 tokens |
| `high` | 65,536 tokens | 65,536 tokens | 32,768 tokens |

## Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Provider default | Not sent in request |
| Stream | `true` | Always enabled |

[Back to providers](providers.md)
