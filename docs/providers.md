# AI Providers

ikigai supports three AI providers. Each requires an API key set via environment variable or `credentials.json`.

| Provider | Environment Variable | Sign Up |
|----------|---------------------|---------|
| Anthropic | `ANTHROPIC_API_KEY` | https://console.anthropic.com/settings/keys |
| OpenAI | `OPENAI_API_KEY` | https://platform.openai.com/api-keys |
| Google | `GOOGLE_API_KEY` | https://aistudio.google.com/app/apikey |

## Anthropic

**API**: `https://api.anthropic.com/v1/messages`

| Model Name | Thinking | Max Thinking Tokens |
|------------|----------|---------------------|
| `claude-opus-4-5` | Yes | 32,768 (default) |
| `claude-sonnet-4-5` | Yes | 65,536 |
| `claude-haiku-4-5` | Yes | 32,768 |

### Thinking Level Mapping

Anthropic uses token budgets. ikigai divides the range between min (1,024) and max into thirds, then rounds down to the nearest power of 2:

| Level | claude-sonnet-4-5 | claude-haiku-4-5 |
|-------|-------------------|------------------|
| `none` | 1,024 tokens | 1,024 tokens |
| `low` | 16,384 tokens | 8,192 tokens |
| `med` | 32,768 tokens | 16,384 tokens |
| `high` | 65,536 tokens | 32,768 tokens |

**Note**: Anthropic's minimum budget is 1,024 tokens. Setting `none` enables thinking at minimum level, not disabled.

### Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Provider default | Not sent in request |
| Stream | `true` | Always enabled |

## OpenAI

OpenAI models use two different APIs depending on model family.

### Responses API Models

**API**: `https://api.openai.com/v1/responses`

These models use the newer Responses API with effort-based reasoning:

| Model Name | Thinking | Notes |
|------------|----------|-------|
| `o1` | Yes | First-gen reasoning |
| `o3` | Yes | Second-gen reasoning |
| `o3-mini` | Yes | Smaller second-gen reasoning |
| `o4-mini` | Yes | Latest small reasoning model |
| `o3-pro` | Yes | Pro-tier o3 reasoning |
| `gpt-5` | Yes | GPT-5 base |
| `gpt-5-mini` | Yes | Smaller GPT-5 |
| `gpt-5-nano` | Yes | Smallest GPT-5 |
| `gpt-5-pro` | Yes | Always uses "high" effort |
| `gpt-5.1` | Yes | GPT-5.1 base |
| `gpt-5.1-chat-latest` | Yes | GPT-5.1 chat |
| `gpt-5.1-codex` | Yes | GPT-5.1 code-focused |
| `gpt-5.1-codex-mini` | Yes | Smaller GPT-5.1 code model |
| `gpt-5.2` | Yes | GPT-5.2 base |
| `gpt-5.2-chat-latest` | Yes | GPT-5.2 chat |
| `gpt-5.2-codex` | Yes | GPT-5.2 code-focused |
| `gpt-5.2-pro` | Yes | GPT-5.2 pro tier |

### Chat Completions API Models

**API**: `https://api.openai.com/v1/chat/completions`

Legacy models without reasoning support:

| Model Name | Thinking | Notes |
|------------|----------|-------|
| `gpt-4` | No | GPT-4 base |
| `gpt-4-turbo` | No | GPT-4 Turbo |
| `gpt-4o` | No | GPT-4o |
| `gpt-4o-mini` | No | Smaller GPT-4o |
| `gpt-4.1` | No | GPT-4.1 (1M context) |
| `gpt-4.1-mini` | No | Smaller GPT-4.1 |
| `gpt-4.1-nano` | No | Smallest GPT-4.1 |

### Thinking Level Mapping

**Old o-series** (o1, o3-mini — cannot disable reasoning):

| Level | API Value |
|-------|-----------|
| `none` | `"low"` (minimum supported) |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

**New o-series** (o3, o3-pro, o4-mini):

| Level | API Value |
|-------|-----------|
| `none` | `"none"` |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

**gpt-5, gpt-5-mini, gpt-5-nano** (minimum is "minimal", API rejects "none"):

| Level | API Value |
|-------|-----------|
| `none` | `"minimal"` (minimum supported) |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

**gpt-5-pro** (always high effort):

| Level | API Value |
|-------|-----------|
| `none` | `"high"` |
| `low` | `"high"` |
| `med` | `"high"` |
| `high` | `"high"` |

**gpt-5.1, gpt-5.1-codex, gpt-5.1-codex-mini**:

| Level | API Value |
|-------|-----------|
| `none` | `"none"` |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

**gpt-5.1-chat-latest** (fixed medium effort — adaptive reasoning, not configurable):

| Level | API Value |
|-------|-----------|
| `none` | `"medium"` |
| `low`  | `"medium"` |
| `med`  | `"medium"` |
| `high` | `"medium"` |

**gpt-5.2, gpt-5.2-codex, gpt-5.2-pro**:

| Level | API Value |
|-------|-----------|
| `none` | `"none"` |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"xhigh"` (maximum supported) |

**gpt-5.2-chat-latest** (fixed medium effort — adaptive reasoning, not configurable):

| Level | API Value |
|-------|-----------|
| `none` | `"medium"` |
| `low`  | `"medium"` |
| `med`  | `"medium"` |
| `high` | `"medium"` |

### Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Not sent | Reasoning models do not support temperature |
| strict | `true` | Tool schemas use strict mode |
| parallel_tool_calls | `false` | Tools executed sequentially |

## Google

**API**: `https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent`

**Note**: ikigai uses the `v1beta` API which includes early features that may have breaking changes. Google also offers a stable `v1` API.

Google has two thinking mechanisms depending on model series.

### Gemini 3 Models (Level-based)

| Model Name | Thinking | Notes |
|------------|----------|-------|
| `gemini-3-flash-preview` | Yes | Uses thinking levels |
| `gemini-3-pro-preview` | Yes | Uses thinking levels |
| `gemini-3.1-pro-preview` | Yes | Uses thinking levels |

### Gemini 2.5 Models (Budget-based)

| Model Name | Thinking | Min Budget | Max Budget |
|------------|----------|------------|------------|
| `gemini-2.5-pro` | Yes | 128 | 32,768 |
| `gemini-2.5-flash` | Yes | 0 | 24,576 |
| `gemini-2.5-flash-lite` | Yes | 512 | 24,576 |

### Thinking Level Mapping

**Gemini 3 models** (per-model mapping, lowercase strings):

| Level | gemini-3-flash-preview | gemini-3-pro-preview | gemini-3.1-pro-preview |
|-------|------------------------|----------------------|------------------------|
| `none` | `"minimal"` | `"low"` | `"low"` |
| `low` | `"low"` | `"low"` | `"low"` |
| `med` | `"medium"` | `"high"` | `"medium"` |
| `high` | `"high"` | `"high"` | `"high"` |

**Note**: Thinking config is always sent for Gemini 3 models (NONE maps to the minimum supported level). The `med`→`"high"` mapping for `gemini-3-pro-preview` is a silent best-fit (medium not supported by that model).

**Gemini 2.5 (budget-based)**:

Budget is calculated by dividing the range between min and max into thirds, then rounded down to the nearest power of 2:

| Level | gemini-2.5-pro | gemini-2.5-flash | gemini-2.5-flash-lite |
|-------|----------------|------------------|----------------------|
| `none` | 128 tokens | 0 tokens | 512 tokens |
| `low` | 8,192 tokens | 8,192 tokens | 8,192 tokens |
| `med` | 16,384 tokens | 16,384 tokens | 16,384 tokens |
| `high` | 32,768 tokens | 24,576 tokens | 24,576 tokens |

**Note**: `gemini-2.5-pro` and `gemini-2.5-flash-lite` cannot fully disable thinking (minimum budget > 0).

### Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Provider default | Not sent in request |
| includeThoughts | `true` | Always enabled |
