# OpenAI

OpenAI models use two different APIs depending on model family.

## Responses API Models

**API**: `https://api.openai.com/v1/responses`

These models use the Responses API with effort-based reasoning.

### o-series

| Model | Reasoning | Notes |
|-------|-----------|-------|
| `o1` | Yes | Cannot fully disable reasoning (minimum `"low"`) |
| `o3` | Yes | Can disable reasoning |
| `o3-mini` | Yes | Cannot fully disable reasoning (minimum `"low"`) |
| `o3-pro` | Yes | Can disable reasoning |
| `o4-mini` | Yes | Can disable reasoning |

#### Reasoning Levels

| Level | o1 | o3-mini | o3, o3-pro, o4-mini |
|-------|----|---------|---------------------|
| `none` | `"low"` | `"low"` | `"none"` |
| `low` | `"low"` | `"low"` | `"low"` |
| `med` | `"medium"` | `"medium"` | `"medium"` |
| `high` | `"high"` | `"high"` | `"high"` |

### GPT-5

| Model | Reasoning | Notes |
|-------|-----------|-------|
| `gpt-5` | Yes | Cannot fully disable reasoning (minimum `"minimal"`) |
| `gpt-5-mini` | Yes | Cannot fully disable reasoning (minimum `"minimal"`) |
| `gpt-5-nano` | Yes | Cannot fully disable reasoning (minimum `"minimal"`) |
| `gpt-5-pro` | Yes | Always uses `"high"` effort regardless of level |

#### Reasoning Levels

| Level | gpt-5, gpt-5-mini, gpt-5-nano | gpt-5-pro |
|-------|-------------------------------|-----------|
| `none` | `"minimal"` | `"high"` |
| `low` | `"low"` | `"high"` |
| `med` | `"medium"` | `"high"` |
| `high` | `"high"` | `"high"` |

### GPT-5.1

| Model | Reasoning | Notes |
|-------|-----------|-------|
| `gpt-5.1` | Yes | Can disable reasoning |
| `gpt-5.1-chat-latest` | Yes | Fixed `"medium"` — adaptive reasoning, not configurable |
| `gpt-5.1-codex` | Yes | Can disable reasoning |
| `gpt-5.1-codex-mini` | Yes | Can disable reasoning |

#### Reasoning Levels

| Level | gpt-5.1, gpt-5.1-codex, gpt-5.1-codex-mini | gpt-5.1-chat-latest |
|-------|----------------------------------------------|---------------------|
| `none` | `"none"` | `"medium"` |
| `low` | `"low"` | `"medium"` |
| `med` | `"medium"` | `"medium"` |
| `high` | `"high"` | `"medium"` |

### GPT-5.2

| Model | Reasoning | Notes |
|-------|-----------|-------|
| `gpt-5.2` | Yes | Supports `"xhigh"` effort |
| `gpt-5.2-chat-latest` | Yes | Fixed `"medium"` — adaptive reasoning, not configurable |
| `gpt-5.2-codex` | Yes | Supports `"xhigh"` effort |
| `gpt-5.2-pro` | Yes | Minimum `"medium"`, supports `"xhigh"` |

#### Reasoning Levels

| Level | gpt-5.2, gpt-5.2-codex | gpt-5.2-pro | gpt-5.2-chat-latest |
|-------|------------------------|-------------|---------------------|
| `none` | `"none"` | `"medium"` | `"medium"` |
| `low` | `"low"` | `"medium"` | `"medium"` |
| `med` | `"medium"` | `"high"` | `"medium"` |
| `high` | `"xhigh"` | `"xhigh"` | `"medium"` |

## Chat Completions API Models

**API**: `https://api.openai.com/v1/chat/completions`

These models do not support reasoning.

| Model | Reasoning |
|-------|-----------|
| `gpt-4` | No |
| `gpt-4-turbo` | No |
| `gpt-4o` | No |
| `gpt-4o-mini` | No |
| `o1-mini` | No |
| `o1-preview` | No |

## Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Not sent | Reasoning models do not support temperature |
| strict | `true` | Tool schemas use strict mode |
| parallel_tool_calls | `false` | Tools executed sequentially |

[Back to providers](providers.md)
