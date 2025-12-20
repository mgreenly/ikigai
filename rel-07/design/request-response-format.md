# Internal Request/Response Format

## Overview

ikigai uses a **superset internal format** that can represent all features from all supported providers. Each provider adapter translates between this internal format and the provider's wire format.

**Design Principle:** The internal format is the union of all provider capabilities. Adapters strip/combine/reformat as needed for each provider.

## Request Format

### Core Structure

```c
typedef struct ik_request {
    // System prompt (separate from messages)
    ik_content_block_t *system_prompt;    // Array of content blocks
    size_t system_prompt_count;

    // Messages
    ik_message_t *messages;
    size_t message_count;

    // Model configuration
    char *model;                          // Provider-specific model ID
    ik_thinking_config_t thinking;        // Thinking/reasoning config

    // Tool definitions
    ik_tool_def_t *tools;
    size_t tool_count;
    ik_tool_choice_t tool_choice;         // AUTO, NONE, REQUIRED, SPECIFIC

    // Generation parameters
    int32_t max_output_tokens;            // -1 = use provider default
    // temperature, top_p, etc. deferred to future release

    // Provider-specific metadata (opaque)
    yyjson_mut_val *provider_data;        // Optional extras
} ik_request_t;
```

### Content Blocks

Messages and system prompts are composed of content blocks:

```c
typedef enum {
    IK_CONTENT_TEXT,           // Plain text
    IK_CONTENT_IMAGE,          // Image (future: base64 + media_type)
    IK_CONTENT_TOOL_CALL,      // Tool call request
    IK_CONTENT_TOOL_RESULT,    // Tool call result
    IK_CONTENT_THINKING        // Thinking/reasoning (in history)
} ik_content_type_t;

typedef struct {
    ik_content_type_t type;

    union {
        struct {
            char *text;
        } text;

        struct {
            char *id;          // Tool call ID (provider-generated or UUID for Google)
            char *name;        // Function name
            yyjson_val *arguments;  // Parsed JSON object (not string)
        } tool_call;

        struct {
            char *tool_call_id;  // Matches id from tool_call
            char *content;       // Result as string (JSON or plain text)
            bool is_error;       // Whether this is an error result
        } tool_result;

        struct {
            char *text;        // Thinking/reasoning text summary
        } thinking;

        // Image support deferred to rel-08
    } data;
} ik_content_block_t;
```

### Messages

```c
typedef enum {
    IK_ROLE_USER,
    IK_ROLE_ASSISTANT,
    IK_ROLE_TOOL        // For tool results (some providers)
} ik_role_t;

typedef struct {
    ik_role_t role;
    ik_content_block_t *content;  // Array of content blocks
    size_t content_count;

    // Metadata (for messages loaded from database)
    char *provider;               // Which provider generated this
    char *model;                  // Which model was used
    ik_thinking_level_t thinking; // Thinking level used
    yyjson_val *provider_data;    // Opaque provider-specific data
} ik_message_t;
```

### Thinking Configuration

```c
typedef enum {
    IK_THINKING_NONE,   // Disabled or minimum
    IK_THINKING_LOW,    // ~1/3 of max budget
    IK_THINKING_MED,    // ~2/3 of max budget
    IK_THINKING_HIGH    // Maximum budget
} ik_thinking_level_t;

typedef struct {
    ik_thinking_level_t level;
    bool include_summary;      // Request thinking summary in response
} ik_thinking_config_t;
```

### Tool Definitions

```c
typedef struct {
    char *name;
    char *description;
    yyjson_val *parameters;    // JSON Schema
    bool strict;               // OpenAI strict mode (default: true)
} ik_tool_def_t;

typedef enum {
    IK_TOOL_CHOICE_AUTO,       // Model decides
    IK_TOOL_CHOICE_NONE,       // No tools allowed
    IK_TOOL_CHOICE_REQUIRED,   // Must use at least one tool (deferred)
    IK_TOOL_CHOICE_SPECIFIC    // Must use named tool (deferred)
} ik_tool_choice_t;
```

## Response Format

### Core Structure

```c
typedef struct ik_response {
    // Content blocks in response
    ik_content_block_t *content;
    size_t content_count;

    // Finish reason
    ik_finish_reason_t finish_reason;

    // Token usage
    ik_usage_t usage;

    // Model identification
    char *model;               // Actual model used (may differ from request)

    // Provider-specific metadata
    yyjson_val *provider_data; // Opaque extras (e.g., thought signatures)
} ik_response_t;
```

### Finish Reasons

```c
typedef enum {
    IK_FINISH_STOP,            // Natural completion
    IK_FINISH_LENGTH,          // Hit max_output_tokens
    IK_FINISH_TOOL_USE,        // Stopped to call tools
    IK_FINISH_CONTENT_FILTER,  // Content policy violation
    IK_FINISH_ERROR,           // Error occurred
    IK_FINISH_UNKNOWN          // Unmapped finish reason
} ik_finish_reason_t;
```

### Token Usage

```c
typedef struct {
    int32_t input_tokens;      // Prompt tokens
    int32_t output_tokens;     // Generated tokens (excluding thinking)
    int32_t thinking_tokens;   // Reasoning/thinking tokens (if separate)
    int32_t cached_tokens;     // Cached input tokens (if reported)
    int32_t total_tokens;      // Sum of above
} ik_usage_t;
```

## Provider Mapping Examples

### Anthropic Request

**Internal:**
```c
ik_request_t req = {
    .system_prompt = [{"type": TEXT, "text": "You are helpful"}],
    .messages = [{"role": USER, "content": [{"type": TEXT, "text": "Hello"}]}],
    .thinking = {.level = IK_THINKING_MED, .include_summary = true},
    .max_output_tokens = 4096
};
```

**Anthropic Wire Format:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "system": "You are helpful",
  "messages": [
    {"role": "user", "content": "Hello"}
  ],
  "thinking": {
    "type": "enabled",
    "budget_tokens": 43000
  },
  "max_tokens": 4096
}
```

**Transformation:**
- System prompt: Array → single string (concatenate blocks)
- Thinking: IK_THINKING_MED → `budget_tokens: 43000` (2/3 of 64K max)
- Messages: Internal format → Anthropic format

### OpenAI Request

**Internal:** (same as above)

**OpenAI Wire Format (Responses API):**
```json
{
  "model": "o3",
  "instructions": "You are helpful",
  "input": "Hello",
  "reasoning": {
    "effort": "medium",
    "summary": "auto"
  },
  "max_output_tokens": 4096
}
```

**Transformation:**
- System prompt → `instructions` field
- Messages → `input` field (Responses API is simpler)
- Thinking: IK_THINKING_MED → `effort: "medium"`

### Google Request

**Internal:** (same as above)

**Google Wire Format:**
```json
{
  "model": "gemini-2.5-pro",
  "systemInstruction": {
    "parts": [{"text": "You are helpful"}]
  },
  "contents": [
    {"role": "user", "parts": [{"text": "Hello"}]}
  ],
  "generationConfig": {
    "thinkingConfig": {
      "thinkingBudget": 21760,
      "includeThoughts": true
    },
    "maxOutputTokens": 4096
  }
}
```

**Transformation:**
- System prompt → `systemInstruction.parts[]`
- Thinking: IK_THINKING_MED → `thinkingBudget: 21760` (2/3 of 32,768 max for 2.5-pro)
- Messages → `contents[]` with role/parts structure

## Tool Call Representation

### Internal Format (same for all providers)

```c
ik_content_block_t tool_call = {
    .type = IK_CONTENT_TOOL_CALL,
    .data.tool_call = {
        .id = "toolu_01A09...",      // Or UUID for Google
        .name = "read_file",
        .arguments = {               // Parsed JSON object
            "path": "/etc/hosts"
        }
    }
};

ik_content_block_t tool_result = {
    .type = IK_CONTENT_TOOL_RESULT,
    .data.tool_result = {
        .tool_call_id = "toolu_01A09...",
        .content = "127.0.0.1 localhost\n...",
        .is_error = false
    }
};
```

### Provider Wire Formats

**Anthropic:**
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

// Result submission (next user message):
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

**OpenAI:**
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

// Result submission:
{
  "role": "tool",
  "tool_call_id": "call_abc123",
  "content": "127.0.0.1 localhost\n..."
}
```

**Google:**
```json
{
  "functionCall": {
    "name": "read_file",
    "args": {"path": "/etc/hosts"}
  }
}

// Result submission (we generate UUID):
{
  "role": "function",
  "functionResponse": {
    "id": "550e8400-e29b-41d4-a716-446655440000",  // Generated by us
    "name": "read_file",
    "response": {"content": "127.0.0.1 localhost\n..."}
  }
}
```

## Thinking Content Representation

### Internal Format

```c
ik_content_block_t thinking = {
    .type = IK_CONTENT_THINKING,
    .data.thinking = {
        .text = "Let me analyze this step by step..."
    }
};
```

**Notes:**
- Thinking content is only present if provider exposes it
- Anthropic: Full text (3.7) or summary (4.x)
- Google: Summary with `thought: true` flag
- OpenAI: Summary only (Responses API with `summary: "auto"`)

### Provider Data (Opaque)

Provider-specific metadata stored in `provider_data` field:

**Google Thought Signatures:**
```json
{
  "provider_data": {
    "thought_signature": "encrypted_signature_string_for_gemini_3"
  }
}
```

**Anthropic Thinking Metadata:**
```json
{
  "provider_data": {
    "thinking_type": "summarized",  // or "full"
    "thinking_signature": "encrypted_verification_data"
  }
}
```

This opaque data is preserved and passed back in subsequent requests for providers that require it (e.g., Google Gemini 3 function calling).

## Validation Rules

### Request Validation

- `model` is required
- `messages` must have at least one message
- `messages[0].role` should be USER (some providers allow flexibility)
- Tool calls in assistant messages must have corresponding tool results in next user message
- `max_output_tokens` must be > 0 if specified

### Response Validation

- `content` must have at least one block (unless error)
- `finish_reason` must be valid enum value
- `usage.total_tokens` should equal sum of input/output/thinking/cached (if provider reports breakdown)

## Builder Pattern

Convenience builders for common operations:

```c
// Create request
ik_request_t *req = ik_request_create(ctx);

// Set system prompt
ik_request_set_system(req, "You are helpful");

// Add user message
ik_request_add_message(req, IK_ROLE_USER, "Hello");

// Add assistant message with thinking
ik_content_block_t blocks[] = {
    {.type = IK_CONTENT_THINKING, .data.thinking.text = "Let me think..."},
    {.type = IK_CONTENT_TEXT, .data.text.text = "Hello!"}
};
ik_request_add_message_blocks(req, IK_ROLE_ASSISTANT, blocks, 2);

// Configure thinking
ik_request_set_thinking(req, IK_THINKING_HIGH, true);  // include_summary

// Add tools
ik_tool_def_t tool = {
    .name = "read_file",
    .description = "Read file contents",
    .parameters = parameters_json
};
ik_request_add_tool(req, &tool);
```

See [transformation.md](transformation.md) for adapter-specific serialization details.
