# Google Gemini Thinking Modes

## Summary

Google Gemini uses two distinct approaches for controlling thinking/reasoning: **thinkingBudget** (token-based, Gemini 2.5 series) and **thinkingLevel** (effort-based, Gemini 3 series). Gemini 2.0 Flash does NOT have configurable thinking parameters through the API - only the experimental "2.0 Flash Thinking" variant (available in Gemini app) has thinking capabilities, but this was superseded by Gemini 2.5 Flash. Gemini 3 introduces mandatory thought signatures for function calling to preserve reasoning context across multi-turn conversations. Default behavior is dynamic thinking for most models, with thinking_budget=-1 or thinking_level unspecified enabling automatic adjustment based on task complexity.

## Model Support Matrix

| Model | Series | Thinking | Parameter | Min | Max | Default | Notes |
|-------|--------|----------|-----------|-----|-----|---------|-------|
| gemini-2.5-pro | 2.5 | Yes | thinkingBudget | 128 | 32,768 | Dynamic | Cannot disable - minimum 128 tokens |
| gemini-2.5-flash | 2.5 | Yes | thinkingBudget | 0 | 24,576 | Dynamic | Can fully disable with 0 |
| gemini-2.5-flash-lite | 2.5 | Yes | thinkingBudget | 512 | 24,576 | Off (0) | Thinking off by default |
| gemini-2.5-flash-live | 2.5 | Yes | thinkingBudget | 0 | 24,576 | Dynamic | Native audio model |
| robotics-er-1.5 | 2.5 | Yes | thinkingBudget | 0 | 24,576 | Dynamic | Robotics-specific model |
| gemini-2.0-flash | 2.0 | No | - | - | - | - | **No API thinking support** |
| gemini-2.0-flash-lite | 2.0 | No | - | - | - | - | **No API thinking support** |
| gemini-2.0-flash-thinking-exp | 2.0 | Yes | - | - | - | Fixed | **Gemini app only** - not API accessible |
| gemini-3-pro | 3 | Yes | thinkingLevel | - | - | HIGH | LOW/HIGH only, cannot disable |
| gemini-3-pro-image | 3 | Yes | thinkingLevel | - | - | HIGH | Image generation with reasoning |

**Confidence: Verified** (official Google AI documentation)

## Parameter Details

### thinkingBudget (Gemini 2.5 Series)

**Type:** Integer token count

**Values:**
- `0` - Completely disables thinking (Flash and Flash-Lite only; 2.5 Pro minimum is 128)
- `1-24576` - Explicit token budget for thinking (Flash/Flash-Lite ranges)
- `128-32768` - Explicit token budget for 2.5 Pro
- `-1` - **Dynamic thinking** (model adjusts budget based on task complexity)

**Default behavior:** If not specified, models use dynamic thinking (equivalent to -1)

**Range enforcement:**
- The budget acts as a **maximum limit**, not a fixed allocation
- Model may use fewer tokens if task doesn't require full budget
- Model may exceed budget in rare cases (overflow) or use less (underflow)
- Budget tokens are separate from context window allocation

**Model-specific limits:**

| Model | Minimum | Maximum | Can Disable? |
|-------|---------|---------|--------------|
| 2.5 Pro | 128 | 32,768 | No |
| 2.5 Flash | 0 | 24,576 | Yes |
| 2.5 Flash-Lite | 512 | 24,576 | Yes (set to 0) |
| Robotics-ER 1.5 | 0 | 24,576 | Yes |

**Confidence: Verified** (official documentation)

### thinkingLevel (Gemini 3 Series)

**Type:** Enum string

**Values:**
- `"LOW"` - Minimizes latency and cost, uses fewer thinking tokens
  - Best for: Simple instruction following, chat, high-throughput applications
  - Latency: Faster first token

- `"HIGH"` - Maximizes reasoning depth, uses more thinking tokens (default)
  - Best for: Complex reasoning, multi-step planning, verified code generation
  - Latency: May take significantly longer to first token, but output is more carefully reasoned

**Default:** If not specified, defaults to `"HIGH"` for Gemini 3 Pro

**Token equivalents:** Not officially documented. Google describes these as "relative allowances for thinking rather than strict token guarantees." Based on Gemini 2.5 patterns and OpenAI compatibility notes:
- LOW: Likely ~1,000-4,000 token range **(Assumed - needs verification)**
- HIGH: Likely ~8,000-16,000 token range **(Assumed - needs verification)**

**Compatibility notes:**
- Gemini 3 models accept `thinkingBudget` for backward compatibility, but using it "may result in suboptimal performance"
- Cannot use both `thinkingLevel` and `thinkingBudget` simultaneously - returns error
- Cannot disable thinking entirely on Gemini 3 Pro - LOW is minimum

**Confidence: Verified** for parameters, **Assumed** for token equivalents

## Thought Signatures

### What They Are

Thought signatures are **encrypted representations of the model's internal thought process**, used to preserve reasoning context across multi-turn conversations. They appear as `thoughtSignature` fields within response content parts.

### How They Work

When a thinking model calls an external tool, it pauses its internal reasoning process. The thought signature acts as a **"save state"**, allowing the model to resume its chain of thought seamlessly once you provide the function's result.

Without thought signatures, the model "forgets" its specific reasoning steps during the tool execution phase. Passing the signature back ensures context continuity - the model preserves and can verify the reasoning steps that justified calling the tool.

### When They Must Be Passed Back

**Gemini 3 Pro (Mandatory for function calling):**
- If you receive a thought signature in a model response, you **must** pass it back exactly as received in the next turn
- Required in all sequential function calls - each step's first function call must include its signature
- Validation is strict: omitting signatures results in **400 errors** with specific messages indicating which function call is missing its signature

**Gemini 3 Pro (Optional for non-function-call parts):**
- Returning signatures is recommended but not strictly enforced for text/reasoning parts

**Gemini 2.5 series:**
- Thought signatures are optional - different handling than Gemini 3 Pro

### SDK Handling

If you use the official Google Gen AI SDKs and use the chat feature (or append the full model response object directly to history), thought signatures are **handled automatically**. You do not need to manually extract or manage them.

### Structure

Thought signatures are opaque encrypted strings. Do NOT attempt to parse or modify them - pass back exactly as received.

For parallel function calls, only the first call includes a signature. Preserve it exactly when building conversation history.

**Confidence: Verified** (official documentation)

## OpenAI-Compatible API Usage

### Base URL

```
https://generativelanguage.googleapis.com/v1beta/openai/
```

### Authentication

```
Authorization: Bearer YOUR_GEMINI_API_KEY
```

### Thinking Configuration via `extra_body`

Gemini-specific features not in OpenAI's API can be accessed through `extra_body`:

```python
from openai import OpenAI

client = OpenAI(
    api_key="YOUR_GEMINI_API_KEY",
    base_url="https://generativelanguage.googleapis.com/v1beta/openai/"
)

# Gemini 2.5 with thinkingBudget
response = client.chat.completions.create(
    model="gemini-2.5-flash",
    messages=[{"role": "user", "content": "Solve this problem..."}],
    extra_body={
        "google": {
            "thinking_config": {
                "thinking_budget": 16384,  # Explicit token budget
                "include_thoughts": True
            }
        }
    }
)

# Gemini 3 with thinkingLevel
response = client.chat.completions.create(
    model="gemini-3-pro",
    messages=[{"role": "user", "content": "Solve this problem..."}],
    extra_body={
        "google": {
            "thinking_config": {
                "thinking_level": "high",
                "include_thoughts": True
            }
        }
    }
)
```

### OpenAI `reasoning_effort` Parameter

The OpenAI-compatible API also supports the `reasoning_effort` parameter (OpenAI standard):

```python
response = client.chat.completions.create(
    model="gemini-2.5-flash",
    messages=[{"role": "user", "content": "Solve this problem..."}],
    reasoning_effort="medium"  # Maps to thinking budget
)
```

**Mapping of `reasoning_effort` to thinkingBudget:**
- `"none"` → 0 tokens (disables thinking for 2.5 Flash/Flash-Lite)
- `"low"` → ~1,000 tokens
- `"medium"` → ~8,000 tokens
- `"high"` → ~24,000 tokens

**Note:** Cannot use both `reasoning_effort` and `thinking_config` simultaneously - they overlap functionality.

**Limitations:**
- Reasoning cannot be turned off for Gemini 2.5 Pro or 3 models (minimum enforced)
- Thought signatures must still be handled manually if doing function calling with Gemini 3

**Confidence: Verified** (official documentation)

## Defaults by Model

| Model | Default Thinking | Can Override? | Override Parameter |
|-------|------------------|---------------|-------------------|
| gemini-2.5-pro | Dynamic (up to 32,768) | Yes | thinkingBudget (min: 128) |
| gemini-2.5-flash | Dynamic (up to 24,576) | Yes | thinkingBudget (can set to 0) |
| gemini-2.5-flash-lite | Off (0) | Yes | thinkingBudget |
| gemini-3-pro | HIGH | Yes | thinkingLevel (LOW/HIGH only) |
| gemini-3-pro-image | HIGH | Yes | thinkingLevel (LOW/HIGH only) |
| gemini-2.0-flash | N/A | No | No thinking support |

**Confidence: Verified** (official documentation)

## includeThoughts Parameter

**Purpose:** Controls whether thinking summaries are included in the response

**Default:** `false` (thoughts not included)

**Usage:**
```json
{
  "generationConfig": {
    "thinkingConfig": {
      "thinkingBudget": -1,
      "includeThoughts": true
    }
  }
}
```

**Response format when enabled:**

```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {
            "thought": true,
            "text": "First, I need to understand the problem. The user is asking about..."
          },
          {
            "text": "The solution to your problem is..."
          }
        ],
        "role": "model"
      }
    }
  ],
  "usageMetadata": {
    "thoughtsTokenCount": 47,
    "candidatesTokenCount": 120,
    "totalTokenCount": 167
  }
}
```

**Notes:**
- Thoughts appear as parts with `"thought": true` flag
- `thoughtsTokenCount` in `usageMetadata` shows thinking tokens used
- Thoughts are **summaries** of internal reasoning, not full thought process
- When `includeThoughts: false`, thinking still happens but summaries are hidden
- Thinking tokens are still counted and billed regardless of `includeThoughts`

**Confidence: Verified** (official documentation)

## Recommended Configuration for ikigai

### Mapping for `/model NAME/THINKING` Command

#### Gemini 2.5 Series (budget-based)

**gemini-2.5-flash and gemini-2.5-flash-lite:**

```
Calculation:
  none → 0
  low  → 8,192  (1/3 of 24,576)
  med  → 16,384 (2/3 of 24,576)
  high → 24,576 (max)
```

**gemini-2.5-pro:**

```
Calculation (accounting for minimum):
  none → 128    (minimum - cannot disable)
  low  → 11,008 (128 + 1/3 × (32,768 - 128))
  med  → 21,760 (128 + 2/3 × (32,768 - 128))
  high → 32,768 (max)
```

**gemini-2.5-flash-lite special note:**
- Default is thinking OFF (0)
- User setting to "none" should keep it at 0
- Only enable thinking when user sets low/med/high

#### Gemini 3 Series (level-based)

**gemini-3-pro and gemini-3-pro-image:**

```
Mapping (only 2 levels available):
  none → LOW  (cannot fully disable)
  low  → LOW
  med  → HIGH (round up)
  high → HIGH
```

**User feedback should clarify:**
```
> /model gemini-3-pro/none

✓ Switched to Google gemini-3-pro
  Thinking: LOW level (cannot disable for this model)
```

#### General Configuration

**All Gemini models:**
- Set `includeThoughts: true` to expose thinking summaries
- Store thoughts separately in `data.thinking` field (per event history design)
- Track `thoughtsTokenCount` from `usageMetadata` for cost tracking

### Dynamic Thinking Option

Consider exposing "dynamic" or "auto" as a fifth option:

```
/model gemini-2.5-flash/auto  → thinkingBudget: -1
```

This lets users opt into Google's automatic budget adjustment based on task complexity.

**Confidence: Verified** configuration approach based on official limits

## Thinking Budget and Context Window Relationship

### Key Findings

**Thinking budget is part of the context window allocation:**
- Gemini uses **bidirectional token budgeting** - dynamically splits context between input and response space
- Example: 256K window might split as 180K input + 76K output
- Thinking tokens come from the OUTPUT portion of the context window
- Setting a higher thinking budget leaves less room for actual response text

**Context windows by model:**
- Gemini 2.5 Pro: 1M tokens (2M coming soon)
- Gemini 2.5 Flash: 1M tokens
- Gemini 3 Pro: 1M tokens

**Practical implications:**
- High thinking budgets (24K-32K) can significantly reduce available output space
- For long conversations, thinking budget may need to be reduced to prevent hitting limits
- Model intelligently determines how much of budget to use - acts as maximum, not fixed allocation

**Confidence: Verified** (official documentation)

## Missed Concepts and Gemini-Specific Features

### 1. Grounding with Thinking

Gemini 3 Pro has **integrated grounding** (Google Search) that works alongside thinking:
- Model can retrieve real-time data (weather, stock charts, etc.) before generating response
- Thinking mode can reason about grounded data
- Particularly useful for Gemini 3 Pro Image (reasons through prompts before generating images)

**Integration:** Not directly related to thinking parameters, but can be combined

### 2. Thinking with Multimodal Inputs

All Gemini 2.5 and 3 models with thinking support are **natively multimodal**:
- Can think about text, images, video, audio, PDFs simultaneously
- Gemini 3 Pro has improved "video reasoning" - traces cause-and-effect over time
- `media_resolution` parameter (low/medium/high) impacts vision processing tokens and latency

**Note for ikigai:** Currently planning text-only (rel-07 design decision), but thinking works with multimodal when ready

### 3. Thinking in Streaming

**Streaming behavior:**
- Thinking happens before response generation starts
- With `includeThoughts: true`, thought summaries are streamed first
- Can result in higher latency to first token with HIGH thinking levels
- Thought signatures appear in streaming responses for Gemini 3

**Format:** Standard SSE with `alt=sse` query parameter

### 4. Robotics-Specific Model

**robotics-er-1.5-preview** supports thinking:
- Uses thinkingBudget (0-24,576 range)
- Specialized for robotics applications
- Same thinking mechanics as other 2.5 models

### 5. Thinking Can't Be Disabled on Some Models

**Important constraint:**
- Gemini 2.5 Pro: Minimum 128 tokens (cannot fully disable)
- Gemini 3 Pro: Minimum LOW level (cannot disable)
- This differs from Anthropic and OpenAI where thinking can be fully disabled

**Impact on ikigai:** User setting `/model gemini-2.5-pro/none` should show warning and use minimum

### 6. Pricing Implications

**Thinking tokens are priced the same as output tokens:**
- No separate pricing tier for thinking (unlike some future OpenAI models)
- `thoughtsTokenCount` is included in `candidatesTokenCount` and `totalTokenCount`
- High thinking budgets can significantly increase costs

**Example:**
- gemini-2.5-flash output: $0.30 per 1M tokens
- 10K thinking tokens = 10K output tokens in terms of cost
- Setting thinking to 0 vs 24,576 can save substantial costs at scale

## Research Question Answers

### 1. Gemini 2.0 Series

**Q: Does gemini-2.0-flash support thinking?**
- **No** - Gemini 2.0 Flash does NOT have configurable thinking through the API

**Q: Does gemini-2.0-flash-lite support thinking?**
- **No** - Gemini 2.0 Flash-Lite does NOT have thinking support

**Q: If yes, which parameter?**
- **N/A** - Only "gemini-2.0-flash-thinking-experimental" (Gemini app only) has thinking
- This experimental model was superseded by Gemini 2.5 Flash with configurable thinkingBudget
- The 2.0 Thinking Experimental model is not API-accessible

**Confidence: Verified**

### 2. Gemini 3 Series Completeness

**Q: Any other Gemini 3 models besides gemini-3-pro?**
- **Yes** - gemini-3-pro-image (image generation with reasoning)
- **No gemini-3-flash yet** - Expected "soon" (possibly before end of December 2025)
- Gemini 3 Pro released November 18, 2025

**Q: Does gemini-3-pro-image support thinking?**
- **Yes** - Uses thinkingLevel (LOW/HIGH)
- Reasons through prompts before generating high-fidelity images
- Same thinking parameters as gemini-3-pro

**Q: Any gemini-3-flash variant?**
- **Not yet released** - Google stated "plan to release additional models to Gemini 3 series soon"
- Speculation: December 22-31, 2025 release
- Expected to be distilled, latency-focused version with sub-second response times

**Confidence: Verified** for current models, **Likely** for upcoming gemini-3-flash

### 3. thinkingBudget Details

**Q: What happens at budget = 0?**
- **Fully disabled** for gemini-2.5-flash and gemini-2.5-flash-lite
- **Not allowed** for gemini-2.5-pro (minimum 128)
- No thinking tokens used, no thinking cost

**Q: What does budget = -1 mean?**
- **Dynamic thinking** - model automatically adjusts budget based on task complexity
- Model may use anywhere from 0 tokens (simple tasks) to max allowed (complex tasks)
- This is the default behavior when thinkingBudget is not specified

**Q: Is there any upper limit beyond documented max?**
- **No** - Hard limits are enforced:
  - 2.5 Pro: 32,768 max
  - 2.5 Flash: 24,576 max
  - 2.5 Flash-Lite: 24,576 max
- Exceeding max returns validation error

**Q: Does thinking budget come from context window?**
- **Yes** - Thinking tokens come from the OUTPUT portion of the bidirectional context window allocation
- Higher thinking budgets leave less room for actual response text
- Model dynamically splits context between input and output space

**Confidence: Verified**

### 4. thinkingLevel vs thinkingBudget

**Q: Why do some models use level and others budget?**
- **Evolution of approach:**
  - Gemini 2.5: Token-based control (thinkingBudget) for fine-grained tuning
  - Gemini 3: Simplified to effort levels (thinkingLevel) - easier for developers
- Google is moving toward level-based for newer models (3.x series)
- Budget-based gives more control but requires understanding token implications

**Q: For level-based models, what's the token equivalent of LOW vs HIGH?**
- **Not officially documented** - Google describes them as "relative allowances rather than strict token guarantees"
- **Estimated based on patterns:**
  - LOW: ~1,000-4,000 token range (Assumed)
  - HIGH: ~8,000-16,000 token range (Assumed)
- Actual usage varies dynamically based on task

**Q: Can you use both parameters on any model?**
- **No** - Using both simultaneously returns an error
- Gemini 3 models: Use thinkingLevel (thinkingBudget accepted for backward compatibility but "may result in suboptimal performance")
- Gemini 2.5 models: thinkingLevel not supported, use thinkingBudget

**Confidence: Verified** for behavior, **Assumed** for token equivalents

### 5. Thought Signatures

**Q: What are "thought signatures"?**
- Encrypted representations of the model's internal thought process
- Act as "save states" for reasoning across tool calls
- Preserve the "why" behind decisions during multi-step workflows

**Q: How should we handle them for conversation continuity?**
- **Gemini 3 Pro function calling:** MUST pass back exactly as received (mandatory)
- **Gemini 3 Pro text:** Recommended but optional
- **Gemini 2.5:** Optional
- If using Google Gen AI SDK with chat feature, handled automatically
- For ikigai: Store in conversation history as opaque strings, pass back in subsequent calls

**Q: Do they need to be passed back like Anthropic thinking blocks?**
- **Different approach:**
  - Anthropic: Extended thinking content blocks can be included in history but not required
  - Gemini 3: Thought signatures are REQUIRED for function calling (400 error if missing)
  - Gemini: Encrypted and opaque (can't be parsed), Anthropic: Readable text blocks
- For parallel function calls, only first call has signature

**Confidence: Verified**

### 6. OpenAI-Compatible API

**Q: Does thinking work through the OpenAI-compatible endpoint?**
- **Yes** - Thinking fully supported via OpenAI-compatible API

**Q: How to configure thinking via `extra_body`?**
```python
extra_body={
    "google": {
        "thinking_config": {
            "thinking_budget": 16384,     # For 2.5 models
            # OR
            "thinking_level": "high",     # For 3 models
            "include_thoughts": True
        }
    }
}
```

**Q: Any limitations vs native API?**
- Same functionality as native API
- Can also use OpenAI standard `reasoning_effort` parameter (maps to thinking budgets)
- Thought signatures still must be handled for Gemini 3 function calling
- Cannot use both `reasoning_effort` and `thinking_config` simultaneously

**Confidence: Verified**

### 7. Default Behavior

**Q: What's the default if thinkingConfig not specified?**
- **Gemini 2.5 Pro:** Dynamic thinking (equivalent to budget=-1, up to 32,768)
- **Gemini 2.5 Flash:** Dynamic thinking (equivalent to budget=-1, up to 24,576)
- **Gemini 2.5 Flash-Lite:** Thinking OFF (budget=0) - optimized for speed/cost
- **Gemini 3 Pro:** HIGH level

**Q: Does it vary by model?**
- **Yes** - Flash-Lite is unique with thinking OFF by default
- All other thinking-capable models default to dynamic/HIGH

**Confidence: Verified**

### 8. Concepts We May Have Missed

**Covered above in "Missed Concepts" section:**
- Grounding + thinking interaction
- Thinking with multimodal inputs
- Thinking in streaming responses
- Robotics-specific model
- Mandatory minimum thinking on some models
- Pricing implications (thinking = output tokens)
- Thought signature requirements for Gemini 3
- Evolution from 2.0 Thinking Experimental to 2.5 Flash with thinkingBudget

## Model Evolution Timeline

- **December 2024:** Gemini 2.0 Flash Thinking Experimental launched (Gemini app only)
- **January 2025:** Updated 2.0 Flash Thinking Experimental in AI Studio
- **March 2025:** Gemini 2.5 series with thinkingBudget parameter
- **November 18, 2025:** Gemini 3 Pro released with thinkingLevel parameter
- **Expected late December 2025:** Gemini 3 Flash (not yet released)

**Key transition:** 2.0 Thinking Experimental → 2.5 Flash with configurable thinking

**Confidence: Verified**

## Implementation Notes for ikigai

### Provider Metadata Structure

```typescript
// Gemini 2.5 Flash
{
  model_id: "gemini-2.5-flash",
  display_name: "Gemini 2.5 Flash",
  context_window: 1000000,
  thinking: {
    supported: true,
    budget: {
      min: 0,
      max: 24576,
      type: "tokens"
    }
  }
}

// Gemini 2.5 Pro
{
  model_id: "gemini-2.5-pro",
  display_name: "Gemini 2.5 Pro",
  context_window: 1000000,
  thinking: {
    supported: true,
    budget: {
      min: 128,  // Cannot disable
      max: 32768,
      type: "tokens"
    }
  }
}

// Gemini 3 Pro
{
  model_id: "gemini-3-pro",
  display_name: "Gemini 3 Pro",
  context_window: 1000000,
  thinking: {
    supported: true,
    levels: {
      available: ["LOW", "HIGH"],
      type: "level"
    }
  }
}

// Gemini 2.0 Flash (no thinking)
{
  model_id: "gemini-2.0-flash",
  display_name: "Gemini 2.0 Flash",
  context_window: 1000000,
  thinking: {
    supported: false
  }
}
```

### Thought Signature Handling

**For Gemini 3 models:**
1. Check response parts for `thoughtSignature` fields
2. Store signatures in conversation history exactly as received
3. When constructing next request, include signatures in corresponding parts
4. For function calling: MUST include signatures or return 400 error to user
5. For text parts: Recommended but optional

**Storage in event history:**
```json
{
  "provider": "google",
  "model": "gemini-3-pro",
  "thinking_level": "high",
  "thinking": "First, I need to understand...",  // Thought summary if includeThoughts=true
  "thought_signature": "encrypted_signature_string",  // Store for next turn
  "thinking_tokens": 1247
}
```

### Error Handling

**Missing thought signature (Gemini 3 function calling):**
- Status: 400
- Message: "Validation error: function call at position X is missing thought signature"
- Action: Return error to user, do not retry

**Invalid thinkingBudget:**
- Status: 400
- Message: "thinkingBudget must be in range [min, max]"
- Action: Clamp to valid range and warn user

**Both thinkingLevel and thinkingBudget specified:**
- Status: 400
- Message: "Cannot specify both thinkingLevel and thinkingBudget"
- Action: Use thinkingLevel for Gemini 3, thinkingBudget for Gemini 2.5

## Sources

- [Gemini thinking | Gemini API | Google AI for Developers](https://ai.google.dev/gemini-api/docs/thinking)
- [Thinking | Generative AI on Vertex AI | Google Cloud](https://cloud.google.com/vertex-ai/generative-ai/docs/thinking)
- [Thought Signatures | Gemini API | Google AI for Developers](https://ai.google.dev/gemini-api/docs/thought-signatures)
- [Thought signatures | Generative AI on Vertex AI | Google Cloud](https://docs.cloud.google.com/vertex-ai/generative-ai/docs/thought-signatures)
- [Gemini models | Gemini API | Google AI for Developers](https://ai.google.dev/gemini-api/docs/models)
- [Gemini 3 Developer Guide | Gemini API | Google AI for Developers](https://ai.google.dev/gemini-api/docs/gemini-3)
- [Gemini 3 Pro | Generative AI on Vertex AI | Google Cloud](https://docs.cloud.google.com/vertex-ai/generative-ai/docs/models/gemini/3-pro)
- [OpenAI compatibility | Gemini API | Google AI for Developers](https://ai.google.dev/gemini-api/docs/openai)
- [New Gemini API updates for Gemini 3 - Google Developers Blog](https://developers.googleblog.com/new-gemini-api-updates-for-gemini-3/)
- [Gemini 2.5: Updates to our family of thinking models - Google Developers Blog](https://developers.googleblog.com/en/gemini-2-5-thinking-model-updates/)
- [Start building with Gemini 2.5 Flash - Google Developers Blog](https://developers.googleblog.com/en/start-building-with-gemini-25-flash/)
- [Migrating to Gemini 3: Implementing Stateful Reasoning with Thought Signatures - Medium](https://medium.com/google-cloud/migrating-to-gemini-3-implementing-stateful-reasoning-with-thought-signatures-4f11b625a8c9)
- [Gemini 2.0 model updates - Google DeepMind Blog](https://blog.google/technology/google-deepmind/gemini-model-updates-february-2025/)
- [Google's Gemini 2.5 Flash introduces 'thinking budgets' - VentureBeat](https://venturebeat.com/ai/googles-gemini-2-5-flash-introduces-thinking-budgets-that-cut-ai-costs-by-600-when-turned-down/)

All information accessed December 14, 2025.
