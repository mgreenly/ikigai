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

**Internal:**
```c
ik_request_t req = {
    .system_prompt = [{.type = TEXT, .text = "You are helpful"}],
    .messages = [
        {.role = USER, .content = [{.type = TEXT, .text = "Hello"}]},
        {.role = ASSISTANT, .content = [
            {.type = THINKING, .text = "Let me think..."},
            {.type = TEXT, .text = "Hi!"}
        ]},
        {.role = USER, .content = [{.type = TEXT, .text = "How are you?"}]}
    ],
    .thinking = {.level = IK_THINKING_MED, .include_summary = true},
    .tools = [...],
    .max_output_tokens = 4096
};
```

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
    "budget_tokens": 43000  // Calculated from IK_THINKING_MED
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

**Internal:**
```c
ik_response_t resp = {
    .content = [
        {.type = IK_CONTENT_THINKING, .data.thinking.text = "I should provide..."},
        {.type = IK_CONTENT_TEXT, .data.text.text = "I'm doing well..."}
    ],
    .content_count = 2,
    .finish_reason = IK_FINISH_STOP,
    .usage = {
        .input_tokens = 50,
        .output_tokens = 100,
        .thinking_tokens = 20,
        .total_tokens = 170
    },
    .model = "claude-sonnet-4-5-20250929",
    .provider_data = NULL
};
```

**Transformation rules:**
- **Content blocks:** Anthropic content[] → Internal content[] (types map 1:1)
- **Stop reason:** "end_turn" → `IK_FINISH_STOP`, "max_tokens" → `IK_FINISH_LENGTH`, "tool_use" → `IK_FINISH_TOOL_USE`
- **Usage:** Direct mapping of token counts

## OpenAI Transformation

### Request: Internal → Wire (Responses API)

**Internal:** (same as above)

**Wire (OpenAI Responses API JSON):**
```json
{
  "model": "o3",
  "instructions": "You are helpful",
  "input": "Hello",  // Simplified for Responses API
  "reasoning": {
    "effort": "medium",
    "summary": "auto"
  },
  "tools": [...],
  "max_output_tokens": 4096
}
```

**Notes:**
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

**Internal:**
```c
ik_response_t resp = {
    .content = [
        {.type = IK_CONTENT_TEXT, .data.text.text = "I'm doing well..."}
    ],
    .content_count = 1,
    .finish_reason = IK_FINISH_STOP,
    .usage = {
        .input_tokens = 50,
        .output_tokens = 100,  // completion_tokens - reasoning_tokens
        .thinking_tokens = 20,
        .total_tokens = 170
    },
    .model = "o3",
    .provider_data = NULL
};
```

**Transformation rules:**
- **Content:** Extract from `output[].content[]`
- **Status:** "completed" → `IK_FINISH_STOP`, "incomplete" → check `incomplete_details.reason`
- **Usage:** Split `completion_tokens` into `output_tokens` and `thinking_tokens`

## Google Transformation

### Request: Internal → Wire

**Internal:** (same as above)

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
      "thinkingBudget": 21760,  // 2/3 of 32,768 for Gemini 2.5 Pro
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
- **Thinking:** `IK_THINKING_MED` → `thinkingBudget: 21760` (model-specific calculation)
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

**Internal:**
```c
ik_response_t resp = {
    .content = [
        {.type = IK_CONTENT_TEXT, .data.text.text = "I'm doing well..."}
    ],
    .content_count = 1,
    .finish_reason = IK_FINISH_STOP,
    .usage = {
        .input_tokens = 50,
        .output_tokens = 100,  // candidatesTokenCount - thoughtsTokenCount
        .thinking_tokens = 20,
        .total_tokens = 190  // Google includes thinking in total
    },
    .model = "gemini-2.5-pro",
    .provider_data = NULL  // Or store thought signatures if present
};
```

**Transformation rules:**
- **Content:** Extract from `candidates[0].content.parts[]`
- **Finish reason:** "STOP" → `IK_FINISH_STOP`, "MAX_TOKENS" → `IK_FINISH_LENGTH`
- **Usage:** Map token counts (Google includes thinking in total)
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

**Internal:**
```c
{
  .type = IK_CONTENT_TOOL_CALL,
  .data.tool_call = {
    .id = "toolu_01A09...",
    .name = "read_file",
    .arguments = {path: "/etc/hosts"}  // Parsed JSON object
  }
}
```

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
        "arguments": "{\"path\":\"/etc/hosts\"}"  // JSON string!
      }
    }
  ]
}
```

**Internal:** (same as Anthropic)

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

**Internal:**
```c
{
  .type = IK_CONTENT_TOOL_CALL,
  .data.tool_call = {
    .id = "Kx2J9FsP3vQmWzN5YbRt",  // Generated by us!
    .name = "read_file",
    .arguments = {path: "/etc/hosts"}
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
    "id": "Kx2J9FsP3vQmWzN5YbRt",  // Our UUID
    "name": "read_file",
    "response": {"content": "127.0.0.1 localhost\n..."}
  }
}
```

## Thinking Budget Calculation

Providers use different thinking parameter types:

### Anthropic (Token Budget)

```c
// Model-specific budgets (from research)
typedef struct {
    int32_t min_budget;
    int32_t max_budget;
} ik_thinking_budget_t;

static const ik_thinking_budget_t ANTHROPIC_BUDGETS[] = {
    // Sonnet 4.5
    {"claude-sonnet-4-5", .min = 1024, .max = 64000},
    // Opus 4.5
    {"claude-opus-4-5", .min = 1024, .max = 64000},
    // Haiku 4.5
    {"claude-haiku-4-5", .min = 1024, .max = 32000},
    // Sonnet 3.7
    {"claude-3-7-sonnet", .min = 1024, .max = 32000}
};

int32_t ik_anthropic_calculate_thinking_budget(const char *model,
                                                ik_thinking_level_t level)
{
    ik_thinking_budget_t budget = get_budget_for_model(model);

    switch (level) {
        case IK_THINKING_NONE:
            return budget.min;
        case IK_THINKING_LOW:
            return budget.min + (budget.max - budget.min) / 3;
        case IK_THINKING_MED:
            return budget.min + 2 * (budget.max - budget.min) / 3;
        case IK_THINKING_HIGH:
            return budget.max;
    }
}
```

### Google (Token Budget for 2.5, Level for 3)

```c
int32_t ik_google_calculate_thinking_budget(const char *model,
                                             ik_thinking_level_t level)
{
    if (strstr(model, "gemini-2.5-pro")) {
        // Max: 32,768, Min: 128
        switch (level) {
            case IK_THINKING_NONE: return 128;
            case IK_THINKING_LOW: return 128 + (32768 - 128) / 3;     // ~11,008
            case IK_THINKING_MED: return 128 + 2 * (32768 - 128) / 3; // ~21,760
            case IK_THINKING_HIGH: return 32768;
        }
    }
    else if (strstr(model, "gemini-2.5-flash")) {
        // Max: 24,576, Min: 0
        switch (level) {
            case IK_THINKING_NONE: return 0;
            case IK_THINKING_LOW: return 24576 / 3;     // ~8,192
            case IK_THINKING_MED: return 2 * 24576 / 3; // ~16,384
            case IK_THINKING_HIGH: return 24576;
        }
    }
    else if (strstr(model, "gemini-3-pro")) {
        // Gemini 3 uses effort levels, not budgets
        // Return symbolic value, actual serialization uses "LOW"/"HIGH"
        return (level <= IK_THINKING_LOW) ? 0 : 1;  // LOW or HIGH
    }

    return -1;  // Unknown model
}
```

### OpenAI (Effort Level)

```c
const char *ik_openai_thinking_effort(ik_thinking_level_t level)
{
    switch (level) {
        case IK_THINKING_NONE: return "none";    // o3/o4-mini only
        case IK_THINKING_LOW: return "low";
        case IK_THINKING_MED: return "medium";
        case IK_THINKING_HIGH: return "high";
    }
}
```

## Serialization Implementation

### Anthropic Serializer

```c
res_t ik_anthropic_serialize_request(TALLOC_CTX *ctx,
                                     ik_request_t *req,
                                     char **out_json)
{
    // Create mutable JSON document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    // Model
    yyjson_mut_obj_add_str(doc, root, "model", req->model);

    // System prompt
    if (req->system_prompt_count > 0) {
        // Concatenate all text blocks
        char *system = concat_text_blocks(ctx, req->system_prompt,
                                         req->system_prompt_count);
        yyjson_mut_obj_add_str(doc, root, "system", system);
    }

    // Messages
    yyjson_mut_val *messages = yyjson_mut_arr(doc);
    for (size_t i = 0; i < req->message_count; i++) {
        yyjson_mut_val *msg = serialize_message(doc, &req->messages[i]);
        yyjson_mut_arr_append(messages, msg);
    }
    yyjson_mut_obj_add_val(doc, root, "messages", messages);

    // Thinking config
    if (req->thinking.level != IK_THINKING_NONE) {
        yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, thinking_obj, "type", "enabled");

        int32_t budget = ik_anthropic_calculate_thinking_budget(req->model,
                                                                req->thinking.level);
        yyjson_mut_obj_add_int(doc, thinking_obj, "budget_tokens", budget);

        yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj);
    }

    // Tools
    if (req->tool_count > 0) {
        yyjson_mut_val *tools = serialize_tools(doc, req->tools, req->tool_count);
        yyjson_mut_obj_add_val(doc, root, "tools", tools);
    }

    // Max tokens
    if (req->max_output_tokens > 0) {
        yyjson_mut_obj_add_int(doc, root, "max_tokens", req->max_output_tokens);
    }

    // Serialize to string
    char *json = yyjson_mut_write(doc, 0, NULL);
    *out_json = talloc_strdup(ctx, json);
    free(json);

    yyjson_mut_doc_free(doc);
    return OK(NULL);
}
```

See [thinking-abstraction.md](thinking-abstraction.md) for detailed thinking parameter mapping.
