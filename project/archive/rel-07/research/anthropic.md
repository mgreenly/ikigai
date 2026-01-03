# Anthropic Claude API - Technical Specification

Complete technical reference for implementing the Anthropic Claude API with exact request/response formats, parameters, and JSON schemas.

## Table of Contents

1. [API Endpoints](#api-endpoints)
2. [Authentication](#authentication)
3. [Request Format](#request-format)
4. [Response Format](#response-format)
5. [Chat Completions](#chat-completions)
6. [Tool Use](#tool-use)
7. [System Messages](#system-messages)
8. [Extended Thinking](#extended-thinking)
9. [Streaming](#streaming)
10. [Error Responses](#error-responses)
11. [Available Models](#available-models)
12. [Batch Processing](#batch-processing)

---

## API Endpoints

### Base URL
```
https://api.anthropic.com
```

### Primary Endpoints

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/v1/messages` | Create a message (chat completion) |
| POST | `/v1/messages/count_tokens` | Count tokens in a message (free, rate-limited) |
| POST | `/v1/messages/batches` | Create a batch of messages |
| GET | `/v1/messages/batches/{message_batch_id}` | Retrieve batch status |
| GET | `/v1/messages/batches` | List all batches |
| POST | `/v1/messages/batches/{message_batch_id}/cancel` | Cancel a batch |
| DELETE | `/v1/messages/batches/{message_batch_id}` | Delete a batch |
| GET | `/v1/messages/batches/{message_batch_id}/results` | Stream batch results |

---

## Authentication

### Required Headers

All API requests require these headers:

```http
x-api-key: YOUR_API_KEY
anthropic-version: 2023-06-01
content-type: application/json
```

### Header Descriptions

- **x-api-key**: Your API key for authentication (NOT `Authorization: Bearer`)
- **anthropic-version**: Required API version header (currently `2023-06-01`)
- **content-type**: Must be `application/json`

### Example with curl

```bash
curl https://api.anthropic.com/v1/messages \
  -H "x-api-key: $ANTHROPIC_API_KEY" \
  -H "anthropic-version: 2023-06-01" \
  -H "content-type: application/json" \
  -d '{...}'
```

---

## Request Format

### Basic Request Structure

```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "messages": [
    {
      "role": "user",
      "content": "Hello, Claude"
    }
  ]
}
```

### Complete Request Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `model` | string | Yes | Model identifier |
| `max_tokens` | integer | Yes | Maximum tokens to generate |
| `messages` | array | Yes | Array of message objects |
| `system` | string or array | No | System prompt (separate from messages) |
| `temperature` | number | No | Randomness 0.0-1.0 (default: 1.0) |
| `top_p` | number | No | Nucleus sampling parameter |
| `top_k` | number | No | Sample from top K tokens |
| `stop_sequences` | array | No | Custom sequences to stop generation |
| `stream` | boolean | No | Enable SSE streaming |
| `metadata` | object | No | User metadata with optional `user_id` |
| `tools` | array | No | Tool definitions for function calling |
| `tool_choice` | object | No | How model should use tools |
| `thinking` | object | No | Extended thinking configuration |

### Message Object Structure

```json
{
  "role": "user",  // "user" or "assistant"
  "content": "string or array of content blocks"
}
```

### Content Block Types

**Text Block:**
```json
{
  "type": "text",
  "text": "The message content"
}
```

**Image Block:**
```json
{
  "type": "image",
  "source": {
    "type": "base64",
    "media_type": "image/jpeg",
    "data": "base64_encoded_image_data"
  }
}
```

**Tool Use Block:**
```json
{
  "type": "tool_use",
  "id": "toolu_01A09q90qw90lq917835lq9",
  "name": "get_weather",
  "input": {
    "location": "San Francisco, CA"
  }
}
```

**Tool Result Block:**
```json
{
  "type": "tool_result",
  "tool_use_id": "toolu_01A09q90qw90lq917835lq9",
  "content": "65 degrees"
}
```

---

## Response Format

### Standard Response

```json
{
  "id": "msg_1234567890abcdef",
  "type": "message",
  "role": "assistant",
  "model": "claude-sonnet-4-5-20250929",
  "content": [
    {
      "type": "text",
      "text": "Hello! I'm Claude, an AI assistant made by Anthropic. How can I help you today?"
    }
  ],
  "stop_reason": "end_turn",
  "stop_sequence": null,
  "usage": {
    "input_tokens": 10,
    "output_tokens": 25,
    "cache_creation_input_tokens": 0,
    "cache_read_input_tokens": 0
  }
}
```

### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique message identifier |
| `type` | string | Always `"message"` |
| `role` | string | Always `"assistant"` |
| `model` | string | Model used for generation |
| `content` | array | Array of content blocks |
| `stop_reason` | string | Why generation stopped |
| `stop_sequence` | string | Matched stop sequence if applicable |
| `usage` | object | Token usage information |

### Stop Reasons

- `"end_turn"` - Natural completion
- `"max_tokens"` - Reached max_tokens limit
- `"stop_sequence"` - Matched a stop_sequence
- `"tool_use"` - Model wants to use a tool
- `"pause_turn"` - Model paused (rare)
- `"refusal"` - Model refused to respond

### Usage Object

```json
{
  "input_tokens": 10,
  "output_tokens": 25,
  "cache_creation_input_tokens": 0,
  "cache_read_input_tokens": 0
}
```

---

## Chat Completions

### Single Turn Conversation

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "messages": [
    {
      "role": "user",
      "content": "What is the capital of France?"
    }
  ]
}
```

**Response:**
```json
{
  "id": "msg_01XYZ",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "The capital of France is Paris."
    }
  ],
  "stop_reason": "end_turn",
  "usage": {
    "input_tokens": 12,
    "output_tokens": 8
  }
}
```

### Multi-Turn Conversation

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "messages": [
    {
      "role": "user",
      "content": "What is the capital of France?"
    },
    {
      "role": "assistant",
      "content": "The capital of France is Paris."
    },
    {
      "role": "user",
      "content": "What is its population?"
    }
  ]
}
```

### Vision (Image Input)

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "image",
          "source": {
            "type": "base64",
            "media_type": "image/jpeg",
            "data": "/9j/4AAQSkZJRgABAQEAYABgAAD..."
          }
        },
        {
          "type": "text",
          "text": "What's in this image?"
        }
      ]
    }
  ]
}
```

---

## Tool Use

### Defining Tools

**Request with Tool Definition:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "tools": [
    {
      "name": "get_weather",
      "description": "Get the current weather in a given location",
      "input_schema": {
        "type": "object",
        "properties": {
          "location": {
            "type": "string",
            "description": "The city and state, e.g. San Francisco, CA"
          },
          "unit": {
            "type": "string",
            "enum": ["celsius", "fahrenheit"],
            "description": "The unit of temperature"
          }
        },
        "required": ["location"]
      }
    }
  ],
  "messages": [
    {
      "role": "user",
      "content": "What's the weather like in San Francisco?"
    }
  ]
}
```

### Tool Use Response

**Response:**
```json
{
  "id": "msg_01Aq9w938a90dw8q",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "text",
      "text": "I'll check the current weather in San Francisco for you."
    },
    {
      "type": "tool_use",
      "id": "toolu_01A09q90qw90lq917835lq9",
      "name": "get_weather",
      "input": {
        "location": "San Francisco, CA",
        "unit": "celsius"
      }
    }
  ],
  "stop_reason": "tool_use",
  "usage": {
    "input_tokens": 472,
    "output_tokens": 89
  }
}
```

### Providing Tool Results

**Request with Tool Result:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "tools": [
    {
      "name": "get_weather",
      "description": "Get the current weather in a given location",
      "input_schema": {
        "type": "object",
        "properties": {
          "location": {"type": "string"}
        },
        "required": ["location"]
      }
    }
  ],
  "messages": [
    {
      "role": "user",
      "content": "What's the weather like in San Francisco?"
    },
    {
      "role": "assistant",
      "content": [
        {
          "type": "text",
          "text": "I'll check the current weather in San Francisco for you."
        },
        {
          "type": "tool_use",
          "id": "toolu_01A09q90qw90lq917835lq9",
          "name": "get_weather",
          "input": {"location": "San Francisco, CA"}
        }
      ]
    },
    {
      "role": "user",
      "content": [
        {
          "type": "tool_result",
          "tool_use_id": "toolu_01A09q90qw90lq917835lq9",
          "content": "65 degrees"
        }
      ]
    }
  ]
}
```

### Tool Choice

Control how the model uses tools:

```json
{
  "tool_choice": {"type": "auto"}  // Default: model decides
}
```

```json
{
  "tool_choice": {"type": "any"}  // Must use a tool
}
```

```json
{
  "tool_choice": {
    "type": "tool",
    "name": "get_weather"  // Must use specific tool
  }
}
```

```json
{
  "tool_choice": {"type": "none"}  // Never use tools
}
```

### Parallel Tool Use

Claude can call multiple tools in a single response:

**Response with Multiple Tools:**
```json
{
  "content": [
    {
      "type": "tool_use",
      "id": "toolu_01A",
      "name": "get_weather",
      "input": {"location": "San Francisco"}
    },
    {
      "type": "tool_use",
      "id": "toolu_01B",
      "name": "get_time",
      "input": {"timezone": "America/Los_Angeles"}
    }
  ],
  "stop_reason": "tool_use"
}
```

---

## System Messages

### System Parameter

System prompts are passed via a separate `system` parameter, NOT in the messages array.

**String Format:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "system": "You are a helpful assistant that speaks like a pirate.",
  "messages": [
    {
      "role": "user",
      "content": "Hello!"
    }
  ]
}
```

**Array Format (for caching):**
```json
{
  "system": [
    {
      "type": "text",
      "text": "You are a helpful assistant.",
      "cache_control": {"type": "ephemeral"}
    }
  ]
}
```

---

## Extended Thinking

### Enabling Extended Thinking

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 16000,
  "thinking": {
    "type": "enabled",
    "budget_tokens": 10000
  },
  "messages": [
    {
      "role": "user",
      "content": "What is 27 * 453?"
    }
  ]
}
```

### Extended Thinking Response

**Response:**
```json
{
  "id": "msg_01XYZ",
  "type": "message",
  "role": "assistant",
  "content": [
    {
      "type": "thinking",
      "thinking": "Let me solve this step by step:\n\n1. First break down 27 * 453\n2. 453 = 400 + 50 + 3\n3. 27 * 400 = 10,800\n4. 27 * 50 = 1,350\n5. 27 * 3 = 81\n6. 10,800 + 1,350 + 81 = 12,231",
      "signature": "EqQBCgIYAhIM1gbcDa9GJwZA2b3hGgxBdjrkzLoky3dl1pkiMOYds..."
    },
    {
      "type": "text",
      "text": "27 * 453 = 12,231"
    }
  ],
  "stop_reason": "end_turn",
  "usage": {
    "input_tokens": 15,
    "output_tokens": 150
  }
}
```

### Thinking Parameters

```json
{
  "thinking": {
    "type": "enabled",        // or "disabled"
    "budget_tokens": 10000    // Max tokens for thinking (1024 minimum)
  }
}
```

### Redacted Thinking

When safety systems flag thinking content:

```json
{
  "content": [
    {
      "type": "redacted_thinking",
      "data": "EmwKAhgBEgy3va3pzix/LafPsn4aDFIT2Xlxh0L5L8rLVyIwxtE3rAFBa8cr..."
    }
  ]
}
```

---

## Streaming

### Enabling Streaming

**Request:**
```json
{
  "model": "claude-sonnet-4-5-20250929",
  "max_tokens": 1024,
  "stream": true,
  "messages": [
    {
      "role": "user",
      "content": "Hello"
    }
  ]
}
```

### SSE Event Format

Server-Sent Events (SSE) use the following format:

```
event: event_name
data: {json_data}

```

### Event Types

1. **message_start** - Message begins
2. **content_block_start** - Content block begins
3. **content_block_delta** - Content chunk
4. **content_block_stop** - Content block ends
5. **message_delta** - Message metadata update
6. **message_stop** - Message complete
7. **ping** - Keep-alive ping

### Streaming Response Example

```
event: message_start
data: {"type": "message_start", "message": {"id": "msg_1nZdL29xx5MUA1yADyHTEsnR8uuvGzszyY", "type": "message", "role": "assistant", "content": [], "model": "claude-sonnet-4-5-20250929", "stop_reason": null, "stop_sequence": null, "usage": {"input_tokens": 25, "output_tokens": 1}}}

event: content_block_start
data: {"type": "content_block_start", "index": 0, "content_block": {"type": "text", "text": ""}}

event: ping
data: {"type": "ping"}

event: content_block_delta
data: {"type": "content_block_delta", "index": 0, "delta": {"type": "text_delta", "text": "Hello"}}

event: content_block_delta
data: {"type": "content_block_delta", "index": 0, "delta": {"type": "text_delta", "text": "!"}}

event: content_block_stop
data: {"type": "content_block_stop", "index": 0}

event: message_delta
data: {"type": "message_delta", "delta": {"stop_reason": "end_turn", "stop_sequence": null}, "usage": {"output_tokens": 15}}

event: message_stop
data: {"type": "message_stop"}
```

### Delta Types

**Text Delta:**
```json
{
  "type": "text_delta",
  "text": "Hello"
}
```

**Input JSON Delta (for tool use):**
```json
{
  "type": "input_json_delta",
  "partial_json": "{\"location\": \"San Fra"
}
```

**Thinking Delta:**
```json
{
  "type": "thinking_delta",
  "thinking": "Let me solve this step by step..."
}
```

**Signature Delta:**
```json
{
  "type": "signature_delta",
  "signature": "EqQBCgIYAhIM1gbcDa9GJwZA..."
}
```

---

## Error Responses

### HTTP Status Codes

| Status | Error Type | Description |
|--------|------------|-------------|
| 400 | `invalid_request_error` | Malformed request |
| 401 | `authentication_error` | Invalid API key |
| 402 | `billing_error` | Billing issue |
| 403 | `permission_error` | Insufficient permissions |
| 404 | `not_found_error` | Resource not found |
| 429 | `rate_limit_error` | Rate limit exceeded |
| 500 | `api_error` | Server error |
| 502 | `timeout_error` | Gateway timeout |
| 529 | `overloaded_error` | Server overloaded |

### Error Response Format

```json
{
  "type": "error",
  "error": {
    "type": "invalid_request_error",
    "message": "max_tokens must be a positive integer"
  }
}
```

### Error Examples

**Authentication Error (401):**
```json
{
  "type": "error",
  "error": {
    "type": "authentication_error",
    "message": "Invalid API key"
  }
}
```

**Rate Limit Error (429):**
```json
{
  "type": "error",
  "error": {
    "type": "rate_limit_error",
    "message": "Rate limit exceeded"
  }
}
```

**Overloaded Error (529):**
```json
{
  "type": "error",
  "error": {
    "type": "overloaded_error",
    "message": "Overloaded"
  }
}
```

### Streaming Errors

Errors can also occur mid-stream:

```
event: error
data: {"type": "error", "error": {"type": "overloaded_error", "message": "Overloaded"}}
```

---

## Available Models

### Current Models (as of 2025)

| Model ID | Description | Context Window |
|----------|-------------|----------------|
| `claude-opus-4-5-20251101` | Most capable, professional software engineering | 200k tokens |
| `claude-opus-4-5` | Alias for latest Opus 4.5 | 200k tokens |
| `claude-3-7-sonnet-latest` | High-performance with extended thinking | 200k tokens |
| `claude-3-7-sonnet-20250219` | Specific version | 200k tokens |
| `claude-3-5-haiku-latest` | Fastest and most compact | 200k tokens |
| `claude-3-5-haiku-20241022` | Previous version | 200k tokens |
| `claude-haiku-4-5` | Hybrid model (instant + extended thinking) | 200k tokens |
| `claude-haiku-4-5-20251001` | Specific version | 200k tokens |
| `claude-sonnet-4-5` | Best for real-world agents/coding | 200k tokens |
| `claude-sonnet-4-5-20250929` | Specific version | 200k tokens |

### Model Capabilities

**Extended Thinking Support:**
- Claude Opus 4.5
- Claude Opus 4.1
- Claude Opus 4
- Claude Sonnet 4.5
- Claude Sonnet 4
- Claude Sonnet 3.7
- Claude Haiku 4.5

**Vision Support:**
- All current models support image input

**Tool Use:**
- All current models support tool use
- Opus 4.5 has improved tool accuracy

---

## Batch Processing

### Create Batch

**Request:**
```json
{
  "requests": [
    {
      "custom_id": "request-1",
      "params": {
        "model": "claude-sonnet-4-5-20250929",
        "max_tokens": 1024,
        "messages": [
          {
            "role": "user",
            "content": "Hello, world"
          }
        ]
      }
    },
    {
      "custom_id": "request-2",
      "params": {
        "model": "claude-sonnet-4-5-20250929",
        "max_tokens": 1024,
        "messages": [
          {
            "role": "user",
            "content": "Another request"
          }
        ]
      }
    }
  ]
}
```

**Response:**
```json
{
  "id": "msgbatch_abc123",
  "type": "message_batch",
  "processing_status": "in_progress",
  "created_at": "2024-01-15T10:30:00Z",
  "expires_at": "2024-01-16T10:30:00Z",
  "request_counts": {
    "processing": 2,
    "succeeded": 0,
    "errored": 0,
    "canceled": 0,
    "expired": 0
  }
}
```

### Batch Results Format

Results are returned as JSONL (one JSON object per line):

```json
{"custom_id": "request-1", "result": {"type": "succeeded", "message": {...}}}
{"custom_id": "request-2", "result": {"type": "errored", "error": {...}}}
```

---

## Token Counting

### Count Tokens Endpoint

**Request:**
```bash
POST https://api.anthropic.com/v1/messages/count_tokens
```

```json
{
  "model": "claude-sonnet-4-5-20250929",
  "messages": [
    {
      "role": "user",
      "content": "Hello, Claude"
    }
  ]
}
```

**Response:**
```json
{
  "input_tokens": 10
}
```

**Note:** This endpoint is free but rate-limited.

---

## Implementation Notes

### Important Differences from OpenAI

1. **Authentication**: Uses `x-api-key` header, NOT `Authorization: Bearer`
2. **System Messages**: Separate `system` parameter, NOT in messages array
3. **Required Version Header**: Must include `anthropic-version: 2023-06-01`
4. **Tool Results**: Different format, must include `tool_use_id`
5. **Streaming**: Different SSE event structure

### Rate Limits

- Vary by usage tier
- Separate limits for different endpoints
- Token counting has its own rate limit
- Check response headers for limit information

### Prompt Caching

Add `cache_control` to content blocks for caching:

```json
{
  "type": "text",
  "text": "Large document...",
  "cache_control": {"type": "ephemeral"}
}
```

Cache TTL options:
- 5 minutes (default)
- 1 hour (for extended thinking)

### Best Practices

1. Always include `anthropic-version` header
2. Handle streaming errors gracefully
3. Preserve thinking blocks when using tools
4. Use token counting for estimation
5. Implement exponential backoff for 5xx errors
6. Monitor usage in response objects
7. Set appropriate `max_tokens` limits

---

## Sources

- [Messages API Documentation](https://platform.claude.com/docs/en/api/messages)
- [Tool Use Documentation](https://platform.claude.com/docs/en/build-with-claude/tool-use)
- [Streaming Messages Documentation](https://platform.claude.com/docs/en/build-with-claude/streaming)
- [Extended Thinking Documentation](https://platform.claude.com/docs/en/build-with-claude/extended-thinking)
- [API Errors Documentation](https://platform.claude.com/docs/en/api/errors)
- [API Versioning Documentation](https://docs.claude.com/en/api/versioning)
- [API Overview](https://platform.claude.com/docs/en/api/overview)
- [Anthropic Academy](https://www.anthropic.com/learn/build-with-claude)
