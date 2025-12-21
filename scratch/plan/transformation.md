# Request/Response Transformation

## Overview

Each provider adapter is responsible for transforming ikigai's internal request format to the provider's wire format (JSON), and parsing the provider's response back to internal format.

**Principle:** Transformation is single-step and happens entirely in the provider adapter. No intermediate representations.

## Transformation Flow

```
Internal Format → Provider Adapter → Wire Format (JSON) → HTTP → Provider API
                                                                       ↓
Internal Format ← Provider Adapter ← Wire Format (JSON) ← HTTP ←  Response
```

## Anthropic Transformation

### Request: Internal → Wire

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
    "budget_tokens": 43000
  },
  "tools": [...],
  "max_tokens": 4096
}
```

**Transformation rules:**
- **System prompt:** Array of content blocks → single string (concatenate text blocks)
- **Messages:** Internal role/content → Anthropic role/content (may combine blocks)
- **Thinking:** `IK_THINKING_MED` → `budget_tokens: 43000` (2/3 of 64K max for Sonnet 4.5)
- **Tools:** Internal tool defs → Anthropic tool schema (mostly 1:1)
- **Max tokens:** Direct mapping

### Response: Wire → Internal

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

## OpenAI Transformation

### Request: Internal → Wire (Responses API)

**Wire (OpenAI Responses API JSON):**
```json
{
  "model": "o3",
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

### Response: Wire → Internal

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

## Google Transformation

### Request: Internal → Wire

**Wire (Google Gemini JSON):**
```json
{
  "model": "gemini-2.5-pro",
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
      "thinkingBudget": 21760,
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
- **Thinking:** `IK_THINKING_MED` → `thinkingBudget: 21760` (model-specific calculation, 2/3 of 32,768 for Gemini 2.5 Pro)
- **Thinking blocks:** Mark with `thought: true` flag
- **Roles:** `ASSISTANT` → `"model"`, `USER` → `"user"`

### Response: Wire → Internal

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
- **Thought signatures:** Store in `provider_data` for Gemini 3

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

## Thinking Budget Calculation

Providers use different thinking parameter types. The formulas below show how internal thinking levels map to provider-specific values.

### Anthropic (Token Budget)

**Model-specific budgets (from research):**
- Sonnet 4.5: min = 1024, max = 64000
- Opus 4.5: min = 1024, max = 64000
- Haiku 4.5: min = 1024, max = 32000
- Sonnet 3.7: min = 1024, max = 32000

**Calculation formula:**
```
budget = min + fraction × (max - min)

where fraction is:
  IK_THINKING_NONE: 0           → min
  IK_THINKING_LOW:  1/3         → min + (max - min)/3
  IK_THINKING_MED:  2/3         → min + 2(max - min)/3
  IK_THINKING_HIGH: 1           → max
```

**Example (Sonnet 4.5):**
- NONE: 1024
- LOW: 1024 + (64000 - 1024)/3 ≈ 21,992
- MED: 1024 + 2(64000 - 1024)/3 ≈ 43,000
- HIGH: 64,000

### Google (Token Budget for 2.5, Level for 3)

**Gemini 2.5 Pro:** max = 32,768, min = 128
```
NONE: 128
LOW:  128 + (32768 - 128)/3 ≈ 11,008
MED:  128 + 2(32768 - 128)/3 ≈ 21,760
HIGH: 32,768
```

**Gemini 2.5 Flash:** max = 24,576, min = 0
```
NONE: 0
LOW:  24576/3 ≈ 8,192
MED:  2 × 24576/3 ≈ 16,384
HIGH: 24,576
```

**Gemini 3 Pro:** Uses effort levels instead of budgets
```
IK_THINKING_NONE → "LOW"
IK_THINKING_LOW  → "LOW"
IK_THINKING_MED  → "HIGH"
IK_THINKING_HIGH → "HIGH"
```

### OpenAI (Effort Level)

**Direct mapping:**
```
IK_THINKING_NONE → "none"    (o3/o4-mini only)
IK_THINKING_LOW  → "low"
IK_THINKING_MED  → "medium"
IK_THINKING_HIGH → "high"
```

See [thinking-abstraction.md](thinking-abstraction.md) for detailed thinking parameter mapping.
