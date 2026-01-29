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
| `claude-opus-4-5` | Yes | 64,000 |
| `claude-sonnet-4-5` | Yes | 64,000 |
| `claude-haiku-4-5` | Yes | 32,000 |

### Thinking Level Mapping

Anthropic uses token budgets. ikigai divides the range between min (1,024) and max into thirds:

| Level | claude-sonnet-4-5 | claude-haiku-4-5 |
|-------|-------------------|------------------|
| `none` | 1,024 tokens | 1,024 tokens |
| `low` | 22,016 tokens | 11,349 tokens |
| `med` | 43,008 tokens | 21,674 tokens |
| `high` | 64,000 tokens | 32,000 tokens |

**Note**: Anthropic's minimum budget is 1,024 tokens. Setting `none` enables thinking at minimum level, not disabled.

### Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Provider default | Not sent in request |

## OpenAI

OpenAI models use two different APIs depending on model family.

### Responses API Models

**API**: `https://api.openai.com/v1/responses`

These models use the newer Responses API with effort-based reasoning:

| Model Name | Thinking | Notes |
|------------|----------|-------|
| `gpt-5` | Yes | Latest GPT-5 |
| `gpt-5-mini` | Yes | Smaller GPT-5 |
| `gpt-5-nano` | Yes | Smallest GPT-5 |
| `gpt-5-pro` | Yes | Always uses "high" effort |
| `gpt-5.1` | Yes | GPT-5.1 base |
| `gpt-5.1-chat-latest` | Yes | GPT-5.1 chat |
| `gpt-5.1-codex` | Yes | GPT-5.1 code-focused |
| `gpt-5.2` | Yes | GPT-5.2 base |
| `gpt-5.2-chat-latest` | Yes | GPT-5.2 chat |
| `gpt-5.2-codex` | Yes | GPT-5.2 code-focused |
| `gpt-4-o1` | Yes | First-gen reasoning |
| `gpt-4-o1-mini` | Yes | Smaller first-gen reasoning |
| `gpt-4-o1-preview` | Yes | First-gen reasoning preview |
| `gpt-4-o3` | Yes | Second-gen reasoning (faster, cheaper) |
| `gpt-4-o3-mini` | Yes | Smaller second-gen reasoning |

### Chat Completions API Models

**API**: `https://api.openai.com/v1/chat/completions`

Legacy models without reasoning support:

| Model Name | Thinking | Notes |
|------------|----------|-------|
| `gpt-4` | No | GPT-4 base |
| `gpt-4-turbo` | No | GPT-4 Turbo |
| `gpt-4o` | No | GPT-4o |
| `gpt-4o-mini` | No | Smaller GPT-4o |

### Thinking Level Mapping

**GPT-5.x models** (except gpt-5-pro):

| Level | API Value |
|-------|-----------|
| `none` | Omitted (no reasoning config) |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

**O-series models** (gpt-4-o1, gpt-4-o3):

| Level | API Value |
|-------|-----------|
| `none` | `"low"` (cannot disable) |
| `low` | `"low"` |
| `med` | `"medium"` |
| `high` | `"high"` |

**gpt-5-pro**:

| Level | API Value |
|-------|-----------|
| `none` | `"high"` (always high) |
| `low` | `"high"` |
| `med` | `"high"` |
| `high` | `"high"` |

### Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Not sent | Reasoning models do not support temperature |
| strict | `true` | Tool schemas use strict mode |
| tool_choice | `"auto"` | Default tool selection |

## Google

**API**: `https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent`

**Note**: ikigai uses the `v1beta` API which includes early features that may have breaking changes. Google also offers a stable `v1` API.

Google has two thinking mechanisms depending on model series.

### Gemini 3 Models (Level-based)

| Model Name | Thinking | Notes |
|------------|----------|-------|
| `gemini-3-pro-preview` | Yes | Uses thinking levels |
| `gemini-3-flash-preview` | Yes | Uses thinking levels |

### Gemini 2.5 Models (Budget-based)

| Model Name | Thinking | Min Budget | Max Budget |
|------------|----------|------------|------------|
| `gemini-2.5-pro` | Yes | 128 | 32,768 |
| `gemini-2.5-flash` | Yes | 0 | 24,576 |
| `gemini-2.5-flash-lite` | Yes | 512 | 24,576 |

### Thinking Level Mapping

**Gemini 3 Pro** (only `low`, `high` available):

| Level | API Value |
|-------|-----------|
| `none` | `"low"` |
| `low` | `"low"` |
| `med` | `"high"` |
| `high` | `"high"` |

**Gemini 3 Flash** (has `minimal`, `low`, `high`):

| Level | API Value |
|-------|-----------|
| `none` | `"minimal"` |
| `low` | `"low"` |
| `med` | `"low"` |
| `high` | `"high"` |

**Note**: Gemini 3 models do not have a "medium" level. Gemini 3 Pro cannot disable thinking (`none` maps to `"low"`).

**Gemini 2.5 (budget-based)**:

Budget is calculated by dividing the range between min and max into thirds:

| Level | gemini-2.5-pro | gemini-2.5-flash | gemini-2.5-flash-lite |
|-------|----------------|------------------|----------------------|
| `none` | 128 tokens | 0 tokens | 512 tokens |
| `low` | 11,008 tokens | 8,192 tokens | 8,533 tokens |
| `med` | 21,888 tokens | 16,384 tokens | 16,554 tokens |
| `high` | 32,768 tokens | 24,576 tokens | 24,576 tokens |

**Note**: `gemini-2.5-pro` and `gemini-2.5-flash-lite` cannot fully disable thinking (minimum budget > 0).

### Fixed Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Temperature | Provider default | Not sent in request |
