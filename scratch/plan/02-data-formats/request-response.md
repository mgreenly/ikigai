# Internal Request/Response Format

## Overview

ikigai uses a **superset internal format** that can represent all features from all supported providers. Each provider adapter translates between this internal format and the provider's wire format.

**Design Principle:** The internal format is the union of all provider capabilities. Adapters strip/combine/reformat as needed for each provider.

## Request Format

### Core Structure

**ik_request_t** - Main request structure containing all parameters for an LLM API call

| Field | Type | Description |
|-------|------|-------------|
| system_prompt | Array of content blocks | System prompt (separate from messages) |
| system_prompt_count | size_t | Number of system prompt blocks |
| messages | Array of ik_message_t | Conversation messages |
| message_count | size_t | Number of messages |
| model | string | Provider-specific model ID |
| thinking | ik_thinking_config_t | Thinking/reasoning configuration |
| tools | Array of ik_tool_def_t | Tool definitions |
| tool_count | size_t | Number of tools |
| tool_choice | ik_tool_choice_t | Tool selection strategy (AUTO, NONE, REQUIRED, SPECIFIC) |
| max_output_tokens | int32_t | Maximum output tokens (-1 = use provider default) |
| provider_data | yyjson_mut_val | Provider-specific metadata (opaque) |

**Note:** temperature, top_p, and other generation parameters are deferred to future release.

### Content Blocks

Messages and system prompts are composed of content blocks.

**ik_content_type_t** - Content block type enumeration:
- IK_CONTENT_TEXT - Plain text
- IK_CONTENT_IMAGE - Image (future: base64 + media_type)
- IK_CONTENT_TOOL_CALL - Tool call request
- IK_CONTENT_TOOL_RESULT - Tool call result
- IK_CONTENT_THINKING - Thinking/reasoning (in history)

**ik_content_block_t** - Content block with type-specific data

| Field | Type | Description |
|-------|------|-------------|
| type | ik_content_type_t | Content block type |
| data | union | Type-specific content (see below) |

**Content block data variants:**

**Text content:**
- text: string - Plain text content

**Tool call content:**
- id: string - Tool call ID (provider-generated or UUID for Google)
- name: string - Function name
- arguments: yyjson_val - Parsed JSON object (not string)

**Tool result content:**
- tool_call_id: string - Matches id from tool_call
- content: string - Result as string (JSON or plain text)
- is_error: bool - Whether this is an error result

**Thinking content:**
- text: string - Thinking/reasoning text summary

**Note:** Image support deferred to rel-08.

### Messages

**ik_role_t** - Message role enumeration:
- IK_ROLE_USER - User message
- IK_ROLE_ASSISTANT - Assistant message
- IK_ROLE_TOOL - Tool results (for some providers)

**ik_message_t** - Conversation message with role and content

| Field | Type | Description |
|-------|------|-------------|
| role | ik_role_t | Message role |
| content | Array of ik_content_block_t | Content blocks |
| content_count | size_t | Number of content blocks |
| provider | string | Which provider generated this (metadata for DB) |
| model | string | Which model was used (metadata for DB) |
| thinking | ik_thinking_level_t | Thinking level used (metadata for DB) |
| provider_data | yyjson_val | Opaque provider-specific data (metadata for DB) |

### Thinking Configuration

**ik_thinking_level_t** - Thinking effort level enumeration:
- IK_THINKING_NONE - Disabled or minimum
- IK_THINKING_LOW - ~1/3 of max budget
- IK_THINKING_MED - ~2/3 of max budget
- IK_THINKING_HIGH - Maximum budget

**ik_thinking_config_t** - Thinking configuration

| Field | Type | Description |
|-------|------|-------------|
| level | ik_thinking_level_t | Thinking effort level |
| include_summary | bool | Request thinking summary in response |

### Tool Definitions

**ik_tool_def_t** - Tool/function definition

| Field | Type | Description |
|-------|------|-------------|
| name | string | Tool name |
| description | string | Tool description |
| parameters | yyjson_val | JSON Schema for parameters |
| strict | bool | OpenAI strict mode (default: true) |

**ik_tool_choice_t** - Tool selection strategy enumeration:
- IK_TOOL_CHOICE_AUTO - Model decides whether to use tools
- IK_TOOL_CHOICE_NONE - No tools allowed
- IK_TOOL_CHOICE_REQUIRED - Must use at least one tool (deferred)
- IK_TOOL_CHOICE_SPECIFIC - Must use named tool (deferred)

## Response Format

### Core Structure

**ik_response_t** - Response from LLM API call

| Field | Type | Description |
|-------|------|-------------|
| content | Array of ik_content_block_t | Content blocks in response |
| content_count | size_t | Number of content blocks |
| finish_reason | ik_finish_reason_t | Why generation stopped |
| usage | ik_usage_t | Token usage statistics |
| model | string | Actual model used (may differ from request) |
| provider_data | yyjson_val | Opaque provider-specific metadata |

### Finish Reasons

**ik_finish_reason_t** - Completion reason enumeration:
- IK_FINISH_STOP - Natural completion
- IK_FINISH_LENGTH - Hit max_output_tokens
- IK_FINISH_TOOL_USE - Stopped to call tools
- IK_FINISH_CONTENT_FILTER - Content policy violation
- IK_FINISH_ERROR - Error occurred
- IK_FINISH_UNKNOWN - Unmapped finish reason

### Token Usage

**ik_usage_t** - Token usage breakdown

| Field | Type | Description |
|-------|------|-------------|
| input_tokens | int32_t | Prompt tokens |
| output_tokens | int32_t | Generated tokens (excluding thinking) |
| thinking_tokens | int32_t | Reasoning/thinking tokens (if separate) |
| cached_tokens | int32_t | Cached input tokens (sum of cache_creation + cache_read) |
| total_tokens | int32_t | Sum of above |

## Provider Mapping Examples

### Anthropic Request

**Internal representation:**
- System prompt: Array of content blocks with text "You are helpful"
- Messages: Single user message with text "Hello"
- Thinking: IK_THINKING_MED level with include_summary=true
- max_output_tokens: 4096

**Anthropic Wire Format:**
```json
{
  "model": "claude-sonnet-4-5-20250514",
  "system": "You are helpful",
  "messages": [
    {"role": "user", "content": "Hello"}
  ],
  "thinking": {
    "type": "enabled",
    "budget_tokens": 43008
  },
  "max_tokens": 4096
}
```

**Transformation:**
- System prompt: Array → single string (concatenate blocks)
- Thinking: IK_THINKING_MED → `budget_tokens: 43008` (2/3 of 64K max)
- Messages: Internal format → Anthropic format

### OpenAI Request

**Internal representation:** (same as above)

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

**Internal representation:** (same as above)

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
      "thinkingBudget": 21888,
      "includeThoughts": true
    },
    "maxOutputTokens": 4096
  }
}
```

**Transformation:**
- System prompt → `systemInstruction.parts[]`
- Thinking: IK_THINKING_MED → `thinkingBudget: 21888` (2/3 of 32,768 max for 2.5-pro)
- Messages → `contents[]` with role/parts structure

## Tool Call Representation

### Internal Format (same for all providers)

**Tool call content block:**
- type: IK_CONTENT_TOOL_CALL
- id: "toolu_01A09..." (or UUID for Google)
- name: "read_file"
- arguments: Parsed JSON object `{"path": "/etc/hosts"}`

**Tool result content block:**
- type: IK_CONTENT_TOOL_RESULT
- tool_call_id: "toolu_01A09..." (matches tool call id)
- content: "127.0.0.1 localhost\n..."
- is_error: false

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

**Thinking content block:**
- type: IK_CONTENT_THINKING
- text: "Let me analyze this step by step..."

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

Convenience builders for common operations provide a friendlier API for constructing requests:

**Request Creation:**
- ik_request_create(ctx) - Create new request with talloc context

**System Prompt:**
- ik_request_set_system(req, text) - Set system prompt from string

**Message Management:**
- ik_request_add_message(req, role, text) - Add simple text message
- ik_request_add_message_blocks(req, role, blocks[], count) - Add message with multiple content blocks

**Thinking Configuration:**
- ik_request_set_thinking(req, level, include_summary) - Configure thinking parameters

**Tool Management:**
- ik_request_add_tool(req, tool_def) - Add tool definition

**Example usage pattern:**
1. Create request with ik_request_create()
2. Set system prompt with ik_request_set_system()
3. Add messages with ik_request_add_message() or ik_request_add_message_blocks()
4. Configure thinking with ik_request_set_thinking()
5. Add tools with ik_request_add_tool()

See [03-provider-types.md](../03-provider-types.md) for adapter-specific serialization details.

## yyjson Document Lifetime

yyjson documents and the `yyjson_val` pointers they contain follow these ownership rules:

1. **Talloc ownership:** yyjson documents (`yyjson_doc`, `yyjson_mut_doc`) must be allocated as talloc children of the struct that contains references to their values
2. **Pointer validity:** `yyjson_val` and `yyjson_mut_val` pointers are only valid while the parent document exists
3. **Automatic cleanup:** When the containing struct is freed via talloc, the yyjson document is freed automatically via talloc destructor
4. **No manual free:** Never call `yyjson_doc_free()` or `yyjson_mut_doc_free()` directly - use talloc hierarchy

**Example:**
```c
typedef struct ik_response {
    yyjson_doc *doc;        // Talloc child of this struct
    yyjson_val *content;    // Points into doc - valid while struct exists
} ik_response_t;

// Creation: doc is child of response
ik_response_t *resp = talloc_zero(ctx, ik_response_t);
resp->doc = yyjson_read_opts(...);  // Uses talloc allocator
talloc_steal(resp, resp->doc);       // Ensure doc is child of resp

// Cleanup: single free releases everything
talloc_free(resp);  // Also frees doc, invalidates all yyjson_val pointers
```

This pattern ensures no dangling pointers and integrates with the project's talloc-based memory management.
