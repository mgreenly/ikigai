# Provider Types and Transformations

## Overview

Providers differ in how they structure requests and responses. This document describes how ikigai's unified internal format is transformed to and from each provider's wire format, and how provider-specific features like thinking/reasoning are abstracted.

**Key Principle:** Each provider adapter owns the complete bidirectional transformation between ikigai's internal format and the provider's API format. This happens in a single step with no intermediate representations.

## Architecture Overview

### Provider Vtable Pattern (Async)

All providers implement the same async interface via function pointers (vtable):

```
┌────────────────────────────────────────────────────────────┐
│                     ik_provider_t                          │
├────────────────────────────────────────────────────────────┤
│  name: const char*                                         │
│  vtable: ik_provider_vtable_t*  ──────┐                    │
│  impl_ctx: void*                       │                    │
└────────────────────────────────────────┼────────────────────┘
                                         │
                 ┌───────────────────────┘
                 v
┌────────────────────────────────────────────────────────────┐
│              ik_provider_vtable_t (async)                  │
├────────────────────────────────────────────────────────────┤
│  Event Loop Integration (curl_multi):                      │
│    fdset()      - populate fd_sets for select()            │
│    perform()    - process pending I/O (non-blocking)       │
│    timeout()    - get recommended select() timeout         │
│    info_read()  - process completed transfers              │
│                                                             │
│  Request Initiation (returns immediately):                 │
│    start_request()  - initiate non-streaming request       │
│    start_stream()   - initiate streaming request           │
│                                                             │
│  Cleanup & Cancellation:                                    │
│    cleanup()    - release resources (optional)             │
│    cancel()     - cancel all in-flight requests            │
└────────────────────────────────────────────────────────────┘
                 │
                 │ Implemented by each provider
                 v
┌─────────────────────────────────────────────────────────────┐
│  OpenAI Provider    │  Anthropic Provider  │  Google Provider│
├─────────────────────┼──────────────────────┼─────────────────┤
│ curl_multi handle   │ curl_multi handle    │ curl_multi handle│
│ API key             │ API key              │ API key         │
│ Base URL            │ Base URL             │ Base URL        │
│                     │                      │                 │
│ Transform:          │ Transform:           │ Transform:      │
│ internal→OpenAI     │ internal→Anthropic   │ internal→Google │
│ OpenAI→internal     │ Anthropic→internal   │ Google→internal │
└─────────────────────────────────────────────────────────────┘
```

### Async Data Flow

```
1. REPL calls: provider->vt->start_stream(req, callbacks)
                    │
                    └─→ Returns immediately (non-blocking)
                        Adds request to curl_multi

2. Event loop:      provider->vt->fdset() → populate fd_sets
                    select() on file descriptors
                    provider->vt->perform() → curl_multi_perform()
                    │
                    └─→ Curl write callback triggered as data arrives
                        │
                        └─→ Parse streaming data (SSE/NDJSON)
                            Emit ik_stream_event_t via stream_cb()

3. Completion:      provider->vt->info_read()
                    │
                    └─→ Detect completed transfers
                        Invoke completion_cb() with final result

4. Cancellation:    provider->vt->cancel()  (on Ctrl+C)
                    │
                    └─→ Abort all in-flight requests
                        Next perform() completes quickly with no callbacks
```

## Transformation Flow

```
Internal Format → Provider Adapter → Wire Format (JSON) → HTTP → Provider API
                                                                       ↓
Internal Format ← Provider Adapter ← Wire Format (JSON) ← HTTP ←  Response
```

## Request Transformation

### Anthropic Transformation

#### Request: Internal → Wire

**Internal format:**
- System prompt: array of content blocks
- Messages: array with role (USER/ASSISTANT) and content blocks
- Thinking: level (NONE/LOW/MED/HIGH) + include_summary flag
- Tools: array of tool definitions
- Max output tokens: integer

**Wire (Anthropic JSON):**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "system": "You are helpful",
  "messages": [
    {"role": "user", "content": "Hello"},
    {
      "role": "assistant",
      "content": [
        {"type": "thinking", "text": "Let me think..."},
        {"type": "text", "text": "Hi!"}
      ]
    },
    {"role": "user", "content": "How are you?"}
  ],
  "thinking": {
    "type": "enabled",
    "budget_tokens": 43008
  },
  "tools": [...],
  "max_tokens": 4096
}
```

**Transformation rules:**
- **System prompt:** Array of content blocks → single string (concatenate text blocks)
- **Messages:** Internal role/content → Anthropic role/content (may combine blocks)
- **Thinking:** `IK_THINKING_MED` → `budget_tokens: 43008` (2/3 of 64K max for Sonnet 4.5)
- **Tools:** Internal tool defs → Anthropic tool schema (mostly 1:1)
- **Max tokens:** Direct mapping

#### Response: Wire → Internal

**Wire (Anthropic JSON):**
```json
{
  "id": "msg_123",
  "type": "message",
  "role": "assistant",
  "model": "claude-sonnet-4-5-20250929",
  "content": [
    {
      "type": "thinking",
      "text": "I should provide a helpful response"
    },
    {
      "type": "text",
      "text": "I'm doing well, thank you!"
    }
  ],
  "stop_reason": "end_turn",
  "usage": {
    "input_tokens": 50,
    "output_tokens": 100,
    "thinking_tokens": 20
  }
}
```

**Internal format:**
- Content: array of content blocks with types (THINKING, TEXT, TOOL_CALL, etc.)
- Finish reason: enum (IK_FINISH_STOP, IK_FINISH_LENGTH, IK_FINISH_TOOL_USE)
- Usage: token counts (input, output, thinking, total)
- Model: string
- Provider data: optional provider-specific metadata

**Transformation rules:**
- **Content blocks:** Anthropic content[] → Internal content[] (types map 1:1)
- **Stop reason:** "end_turn" → `IK_FINISH_STOP`, "max_tokens" → `IK_FINISH_LENGTH`, "tool_use" → `IK_FINISH_TOOL_USE`
- **Usage:** Direct mapping of token counts

### OpenAI Transformation

#### Request: Internal → Wire (Responses API)

**Wire (OpenAI Responses API JSON):**
```json
{
  "model": "gpt-5-mini",
  "instructions": "You are helpful",
  "input": "Hello",
  "reasoning": {
    "effort": "medium",
    "summary": "auto"
  },
  "tools": [...],
  "max_output_tokens": 4096
}
```

**Transformation rules:**
- **Responses API simplification:** Multi-turn messages → single `input` string (or use full conversation)
- **Instructions:** System prompt → `instructions` field
- **Reasoning:** `IK_THINKING_MED` → `effort: "medium"`
- For **Chat Completions API** (fallback), use `messages[]` array format instead

#### Response: Wire → Internal

**Wire (OpenAI Responses API JSON):**
```json
{
  "id": "resp_123",
  "object": "response",
  "status": "completed",
  "output": [
    {
      "type": "message",
      "content": [
        {
          "type": "output_text",
          "text": "I'm doing well, thank you!"
        }
      ]
    }
  ],
  "usage": {
    "prompt_tokens": 50,
    "completion_tokens": 120,
    "completion_tokens_details": {
      "reasoning_tokens": 20
    }
  }
}
```

**Transformation rules:**
- **Content:** Extract from `output[].content[]`
- **Status:** "completed" → `IK_FINISH_STOP`, "incomplete" → check `incomplete_details.reason`
- **Usage:** Split `completion_tokens` into `output_tokens` and `thinking_tokens`
  - `output_tokens = completion_tokens - reasoning_tokens`
  - `thinking_tokens = reasoning_tokens`
  - `total_tokens = prompt_tokens + completion_tokens`

### Google Transformation

#### Request: Internal → Wire

**Wire (Google Gemini JSON):**
```json
{
  "model": "gemini-3.0-flash",
  "systemInstruction": {
    "parts": [
      {"text": "You are helpful"}
    ]
  },
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "Hello"}]
    },
    {
      "role": "model",
      "parts": [
        {"text": "Let me think...", "thought": true},
        {"text": "Hi!"}
      ]
    },
    {
      "role": "user",
      "parts": [{"text": "How are you?"}]
    }
  ],
  "generationConfig": {
    "thinkingConfig": {
      "thinkingLevel": "HIGH",
      "includeThoughts": true
    },
    "maxOutputTokens": 4096
  },
  "tools": [...]
}
```

**Transformation rules:**
- **System:** Internal system prompt → `systemInstruction.parts[]`
- **Messages:** Internal messages → `contents[]` with `role: "user"` or `role: "model"`
- **Thinking:** `IK_THINKING_MED` → `thinkingLevel: "HIGH"` for Gemini 3.0 models, `thinkingBudget` for Gemini 2.5 models
- **Thinking blocks:** Mark with `thought: true` flag
- **Roles:** `ASSISTANT` → `"model"`, `USER` → `"user"`

#### Response: Wire → Internal

**Wire (Google JSON):**
```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {"text": "I'm doing well, thank you!"}
        ],
        "role": "model"
      },
      "finishReason": "STOP"
    }
  ],
  "usageMetadata": {
    "promptTokenCount": 50,
    "candidatesTokenCount": 120,
    "thoughtsTokenCount": 20,
    "totalTokenCount": 190
  }
}
```

**Transformation rules:**
- **Content:** Extract from `candidates[0].content.parts[]`
- **Finish reason:** "STOP" → `IK_FINISH_STOP`, "MAX_TOKENS" → `IK_FINISH_LENGTH`
- **Usage:** Map token counts with calculation:
  - `input_tokens = promptTokenCount`
  - `output_tokens = candidatesTokenCount - thoughtsTokenCount`
  - `thinking_tokens = thoughtsTokenCount`
  - `total_tokens = totalTokenCount` (Google includes thinking in total)
- **Thought signatures:** Store in `provider_data` for Gemini 3.0

## Tool Call Transformation

### Anthropic Tool Calls

**Wire format:**
```json
{
  "content": [
    {
      "type": "tool_use",
      "id": "toolu_01A09...",
      "name": "read_file",
      "input": {"path": "/etc/hosts"}
    }
  ]
}
```

**Internal format:**
- Type: IK_CONTENT_TOOL_CALL
- ID: string (from provider)
- Name: string
- Arguments: parsed JSON object

**Tool result submission (next user message):**
```json
{
  "role": "user",
  "content": [
    {
      "type": "tool_result",
      "tool_use_id": "toolu_01A09...",
      "content": "127.0.0.1 localhost\n..."
    }
  ]
}
```

### OpenAI Tool Calls

**Wire format:**
```json
{
  "tool_calls": [
    {
      "id": "call_abc123",
      "type": "function",
      "function": {
        "name": "read_file",
        "arguments": "{\"path\":\"/etc/hosts\"}"
      }
    }
  ]
}
```

**Transformation notes:**
- Parse `arguments` string to JSON object for internal format
- Serialize back to JSON string when building request

**Tool result submission:**
```json
{
  "role": "tool",
  "tool_call_id": "call_abc123",
  "content": "127.0.0.1 localhost\n..."
}
```

### Google Tool Calls

**Wire format:**
```json
{
  "functionCall": {
    "name": "read_file",
    "args": {"path": "/etc/hosts"}
  }
}
```

**Transformation notes:**
- **Generate UUID** for Google (22-char base64url format, same as agent IDs)
- Google doesn't provide IDs, we must track them ourselves

**Tool result submission:**
```json
{
  "role": "function",
  "functionResponse": {
    "id": "Kx2J9FsP3vQmWzN5YbRt",
    "name": "read_file",
    "response": {"content": "127.0.0.1 localhost\n..."}
  }
}
```

## Thinking/Reasoning Abstraction

ikigai provides a unified thinking level abstraction where users specify one of: `min`, `low`, `med`, `high`. Provider adapters translate these levels to provider-specific parameters.

### Unified Thinking Levels

- `min` - Disabled or minimum thinking
- `low` - Approximately 1/3 of maximum budget
- `med` - Approximately 2/3 of maximum budget
- `high` - Maximum budget

### Anthropic (Token Budget)

Anthropic uses `thinking.budget_tokens` parameter.

**Model budgets (from research):**

| Model | Min | Max | Notes |
|-------|-----|-----|-------|
| claude-haiku-4-5 | 1,024 | 32,000 | Fast/cheap |
| claude-sonnet-4-5 | 1,024 | 64,000 | Default model |
| claude-opus-4-5 | 1,024 | 64,000 | Most capable |

**Mapping formula:**

```
budget = min + (level_value / 3) * (max - min)

Where level_value is:
  min  → 0
  low  → 1
  med  → 2
  high → 3
```

**Examples (Sonnet 4.5, min=1024, max=64000):**

```
min  → 1,024   (minimum)
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
| claude-haiku-4-5 | 1024 | 32000 |
| claude-sonnet-4-5 | 1024 | 64000 |
| claude-opus-4-5 | 1024 | 64000 |

**Implementation logic:**

1. Find matching model pattern from configuration table
2. If no match, use Sonnet defaults (1024-64000)
3. Calculate budget using formula above
4. Return minimum for `min`, maximum for `high`

### Google (Mixed: Budget for 2.5, Level for 3)

Google uses **different parameters for different model series:**
- **Gemini 2.5:** `thinkingBudget` (token count)
- **Gemini 3:** `thinkingLevel` ("LOW" or "HIGH")

**Model budgets:**

| Model | Min | Max | Type |
|-------|-----|-----|------|
| gemini-2.5-flash-lite | 512 | 24,576 | Budget |
| gemini-3.0-flash | - | - | Level (LOW/HIGH) |
| gemini-3.0-pro | - | - | Level (LOW/HIGH) |

**Mapping (Gemini 2.5 Flash Lite):**

```
min  → 512     (minimum, cannot disable)
low  → 8,533   (512 + 1/3 * (24576 - 512))
med  → 16,554  (512 + 2/3 * (24576 - 512))
high → 24,576  (maximum)
```

**Mapping (Gemini 3.0 Flash / 3.0 Pro):**

```
min  → "LOW"   (cannot disable)
low  → "LOW"
med  → "HIGH"  (round up)
high → "HIGH"
```

**Wire format (2.5 series):**

```json
{
  "generationConfig": {
    "thinkingConfig": {
      "thinkingBudget": 21888,
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
| gemini-2.5-flash-lite | 512 | 24576 | false |
| gemini-3.0-flash | 0 | 0 | true |
| gemini-3.0-pro | 0 | 0 | true |

**Implementation logic:**

1. Find matching model pattern from configuration table
2. If `uses_level` is true (Gemini 3):
   - Map `min`/`low` → "LOW"
   - Map `med`/`high` → "HIGH"
   - Add to `thinkingLevel` field
3. If `uses_level` is false (Gemini 2.5):
   - Calculate budget using standard formula
   - Add to `thinkingBudget` field
4. Always set `includeThoughts: true`

### OpenAI (Effort Level)

OpenAI uses `reasoning.effort` parameter (Responses API) or `reasoning_effort` (Chat Completions API).

**Supported values:**
- gpt-5, gpt-5-mini, gpt-5-nano: `"none"`, `"minimal"`, `"low"`, `"medium"`, `"high"`, `"xhigh"`

**Mapping:**

```
min  → "none"
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
| gpt-5-nano | true | medium |
| gpt-5-mini | true | medium |
| gpt-5 | true | medium |

**Implementation logic:**

1. All GPT-5 models support "none" effort
2. Map level to effort string:
   - `min` → "none"
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
  Thinking: med (budget: 32K)
```

### Google

```
> /model gemini-2.5-flash-lite/high

✓ Switched to Google gemini-2.5-flash-lite
  Thinking: high (budget: 24K)
```

```
> /model gemini-3.0-flash/min

✓ Switched to Google gemini-3.0-flash
  Thinking: min (level: minimal)
```

### OpenAI

```
> /model gpt-5-mini/med

✓ Switched to OpenAI gpt-5-mini
  Thinking: med (effort: medium)
```

```
> /model gpt-5/high

✓ Switched to OpenAI gpt-5
  Thinking: high (effort: high)
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

## Thought Signatures (Google Gemini 3.0)

Gemini 3.0 requires thought signatures for function calling:

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
| min | 1,024 | minimum |
| low | 22,016 | 1024 + 1/3 * 62976 |
| med | 43,008 | 1024 + 2/3 * 62976 |
| high | 64,000 | maximum |

**Test: Google Gemini 3.0 Flash/Pro uses level not budget**

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
