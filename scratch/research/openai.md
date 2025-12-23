# OpenAI API Technical Specification

Complete technical implementation reference for OpenAI API integration.

## Table of Contents

1. [API Endpoints](#api-endpoints)
2. [Authentication](#authentication)
3. [Chat Completions API](#chat-completions-api)
4. [Responses API](#responses-api)
5. [Streaming](#streaming)
6. [Tools / Function Calling](#tools--function-calling)
7. [Error Responses](#error-responses)
8. [Models](#models)

---

## API Endpoints

### Base URL
```
https://api.openai.com
```

### Available Endpoints

#### 1. Chat Completions
- **URL**: `https://api.openai.com/v1/chat/completions`
- **Method**: `POST`
- **Purpose**: Traditional chat and completion tasks
- **Status**: Supported indefinitely

#### 2. Responses API
- **URL**: `https://api.openai.com/v1/responses`
- **Method**: `POST`
- **Purpose**: Agent-focused interactions with reasoning state preservation
- **Status**: Released March 2025 (recommended for new projects)

---

## Authentication

### Header Format

```http
Authorization: Bearer YOUR_API_KEY
Content-Type: application/json
```

### Optional Headers

```http
OpenAI-Organization: org-XXXXXXXXXXXXX
OpenAI-Project: proj-XXXXXXXXXXXXX
```

### Example curl Request

```bash
curl https://api.openai.com/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [
      {"role": "system", "content": "You are a helpful assistant."},
      {"role": "user", "content": "Hello!"}
    ]
  }'
```

---

## Chat Completions API

### Request Format

#### HTTP Method
```
POST /v1/chat/completions
```

#### Request Headers
```http
Authorization: Bearer YOUR_API_KEY
Content-Type: application/json
```

#### Request Body Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `model` | string | Yes | Model ID (e.g., "gpt-5", "gpt-4o-mini") |
| `messages` | array | Yes | Array of message objects |
| `max_completion_tokens` | integer | No | Maximum tokens in completion (replaces deprecated `max_tokens`) |
| `temperature` | number | No | Sampling temperature 0-2 (default: 1.0) |
| `top_p` | number | No | Nucleus sampling parameter (default: 1.0) |
| `n` | integer | No | Number of completions to generate |
| `stream` | boolean | No | Enable streaming (default: false) |
| `presence_penalty` | number | No | Penalty for new tokens -2.0 to 2.0 |
| `frequency_penalty` | number | No | Penalty for frequent tokens -2.0 to 2.0 |
| `logprobs` | boolean | No | Return log probabilities |
| `top_logprobs` | integer | No | Number of top logprobs to return (0-20) |
| `tools` | array | No | Available tools for function calling |
| `tool_choice` | string/object | No | Control tool selection ("auto", "none", or specific tool) |
| `parallel_tool_calls` | boolean | No | Enable parallel function calling |
| `response_format` | object | No | Output format specification (e.g., JSON mode) |
| `user` | string | No | User identifier for tracking |

#### Message Object Structure

```json
{
  "role": "system" | "user" | "assistant" | "tool",
  "content": "string or array",
  "name": "optional_name",
  "tool_calls": []  // for assistant messages with tool calls
}
```

#### Complete Request Example

```json
{
  "model": "gpt-5",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant."
    },
    {
      "role": "user",
      "content": "What is the weather like in Paris?"
    }
  ],
  "max_completion_tokens": 1000,
  "temperature": 0.7,
  "top_p": 1.0,
  "stream": false,
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "get_weather",
        "description": "Get current temperature for a given location.",
        "parameters": {
          "type": "object",
          "properties": {
            "location": {
              "type": "string",
              "description": "City and country e.g. Paris, France"
            }
          },
          "required": ["location"],
          "additionalProperties": false
        },
        "strict": true
      }
    }
  ],
  "tool_choice": "auto"
}
```

### Response Format

#### Success Response Structure

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1686676106,
  "model": "gpt-4o-mini",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "This is the response text!"
      },
      "logprobs": null,
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 13,
    "completion_tokens": 7,
    "total_tokens": 20
  },
  "system_fingerprint": "fp_abc123"
}
```

#### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique identifier for the completion |
| `object` | string | Always "chat.completion" |
| `created` | integer | Unix timestamp of creation |
| `model` | string | Model used for completion |
| `choices` | array | Array of completion choices |
| `usage` | object | Token usage statistics |
| `system_fingerprint` | string | System configuration fingerprint |

#### Choice Object

| Field | Type | Description |
|-------|------|-------------|
| `index` | integer | Choice index |
| `message` | object | The completion message |
| `finish_reason` | string | Reason completion stopped: "stop", "length", "tool_calls", "content_filter", "error" |
| `logprobs` | object/null | Log probabilities if requested |

#### Response with Tool Calls

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1686676106,
  "model": "gpt-5",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": null,
        "tool_calls": [
          {
            "id": "call_abc123",
            "type": "function",
            "function": {
              "name": "get_weather",
              "arguments": "{\"location\": \"Paris, France\"}"
            }
          }
        ]
      },
      "finish_reason": "tool_calls"
    }
  ],
  "usage": {
    "prompt_tokens": 45,
    "completion_tokens": 12,
    "total_tokens": 57
  }
}
```

---

## Responses API

### Request Format

#### HTTP Method
```
POST /v1/responses
```

#### Key Differences from Chat Completions

| Feature | Chat Completions | Responses API |
|---------|-----------------|---------------|
| System messages | `messages[].role = "system"` | `instructions` parameter |
| Input format | `messages` array | `input` (string or array) |
| State management | Client-side | Server-side with `store` and `previous_response_id` |
| Response field | `choices[].message` | `output[]` array |

#### Request Body Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `model` | string | Yes | Model ID (e.g., "gpt-5", "o3", "o3-mini") |
| `input` | string/array | Yes | Input text or array of messages |
| `instructions` | string | No | High-level behavioral instructions (replaces system messages) |
| `tools` | array | No | Available tools (built-in, MCP, or custom functions) |
| `tool_choice` | string/object | No | Control tool selection |
| `parallel_tool_calls` | boolean | No | Enable parallel function calling |
| `store` | boolean | No | Maintain state from turn to turn |
| `previous_response_id` | string | No | Reference to previous response for chaining |
| `conversation` | string | No | Conversation ID for automatic state management |
| `text` | object | No | Text output configuration |
| `text.format` | object | No | Structured output format (e.g., JSON schema) |
| `max_output_tokens` | integer | No | Maximum tokens in output |
| `temperature` | number | No | Sampling temperature 0-2 (default: 1.0) |
| `top_p` | number | No | Nucleus sampling parameter (default: 1.0) |
| `logprobs` | integer | No | Number of top logprobs to return (0-20) |
| `reasoning` | object | No | Reasoning configuration for o-series models |
| `reasoning.effort` | string | No | Reasoning effort level ("low", "medium", "high") |
| `background` | boolean | No | Run response in background |
| `truncation` | string | No | Truncation strategy: "auto" or "disabled" (default) |
| `prompt` | object | No | Template-based prompting config |
| `metadata` | object | No | Key-value pairs for additional information |
| `user` | string | No | User identifier for tracking |
| `stream` | boolean | No | Enable streaming with semantic events |
| `include` | array | No | Additional fields to include in response |

#### Request Example (Simple)

```json
{
  "model": "gpt-5",
  "input": "What is the weather like in Paris?",
  "instructions": "You are a helpful assistant.",
  "max_output_tokens": 1000
}
```

#### Request Example (Conversational with Tools)

```json
{
  "model": "o3",
  "input": [
    {
      "role": "user",
      "content": "What is the weather like in Paris?"
    }
  ],
  "instructions": "You are a helpful assistant.",
  "tools": [
    {
      "type": "function",
      "name": "get_weather",
      "description": "Get current temperature for a given location.",
      "parameters": {
        "type": "object",
        "properties": {
          "location": {
            "type": "string",
            "description": "City and country"
          }
        },
        "required": ["location"],
        "additionalProperties": false
      },
      "strict": true
    }
  ],
  "tool_choice": "auto",
  "store": true,
  "reasoning": {
    "effort": "medium"
  }
}
```

### Response Format

#### Success Response Structure

```json
{
  "id": "resp_67cb32528d6881909eb2859a55e18a85",
  "object": "response",
  "created_at": 1741369938.0,
  "model": "gpt-4o-2024-08-06",
  "status": "completed",
  "output": [
    {
      "id": "msg_67cb3252cfac8190865744873aada798",
      "type": "message",
      "role": "assistant",
      "status": null,
      "content": [
        {
          "type": "output_text",
          "text": "Great! How can I help you today?",
          "annotations": []
        }
      ]
    }
  ],
  "instructions": null,
  "metadata": {},
  "error": null,
  "incomplete_details": null
}
```

#### Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique response identifier |
| `object` | string | Always "response" |
| `created_at` | number | Unix timestamp |
| `model` | string | Model used |
| `status` | string | "completed", "failed", "in_progress", "cancelled", "queued", "incomplete" |
| `output` | array | Array of output items (messages, tool calls, etc.) |
| `instructions` | string/null | Instructions used |
| `metadata` | object | Metadata passed in request |
| `error` | object/null | Error details if failed |
| `incomplete_details` | object/null | Details if status is "incomplete" |

#### Status Values

- `completed` - Response fully generated
- `failed` - Error occurred
- `in_progress` - Still generating (async mode)
- `cancelled` - Request cancelled
- `queued` - Waiting to process
- `incomplete` - Stopped due to limits (check `incomplete_details.reason`)

#### Incomplete Details Reasons

- `max_output_tokens` - Hit token limit
- `content_filter` - Content filtered
- `timeout` - Request timed out

---

## Streaming

### Chat Completions Streaming

#### Enable Streaming

```json
{
  "model": "gpt-5",
  "messages": [...],
  "stream": true
}
```

#### SSE Event Format

Each event follows Server-Sent Events (SSE) format:

```
data: <JSON>

data: <JSON>

data: [DONE]
```

#### Chunk Structure

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion.chunk",
  "created": 1686676106,
  "model": "gpt-4o-mini",
  "choices": [
    {
      "index": 0,
      "delta": {
        "role": "assistant",
        "content": "Hello"
      },
      "finish_reason": null
    }
  ]
}
```

#### Delta Field

The `delta` object contains incremental updates:

- **First chunk**: Contains `role` field
- **Subsequent chunks**: Contains `content` fragments
- **Tool call chunks**: Contains partial `tool_calls` data
- **Final chunk**: Empty delta with `finish_reason`

#### Example Stream Sequence

```
data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1686676106,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1686676106,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1686676106,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":" there"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1686676106,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":"!"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1686676106,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]
```

### Responses API Streaming

#### Semantic Events

The Responses API uses typed semantic events instead of raw deltas.

#### Event Types

- `response.created` - Response started
- `response.output_item.added` - New output item added
- `response.output_text.delta` - Text content delta
- `response.function_call_arguments.delta` - Function call arguments delta
- `response.reasoning_summary_text.delta` - Reasoning summary delta
- `response.refusal.delta` - Refusal text delta
- `response.completed` - Response completed
- `error` - Error occurred

#### Event Format

```
event: response.output_text.delta
data: <JSON>
```

#### Delta Event Structure

```json
{
  "type": "response.output_text.delta",
  "item_id": "msg_123",
  "output_index": 0,
  "content_index": 0,
  "delta": "In",
  "sequence_number": 1
}
```

#### Example Event Sequence

```
event: response.created
data: {"type":"response.created","id":"resp_123","created_at":1686676106}

event: response.output_item.added
data: {"type":"response.output_item.added","item_id":"msg_456","output_index":0}

event: response.output_text.delta
data: {"type":"response.output_text.delta","item_id":"msg_456","output_index":0,"content_index":0,"delta":"Hello","sequence_number":1}

event: response.output_text.delta
data: {"type":"response.output_text.delta","item_id":"msg_456","output_index":0,"content_index":0,"delta":" there","sequence_number":2}

event: response.completed
data: {"type":"response.completed","id":"resp_123","status":"completed"}
```

---

## Tools / Function Calling

### Tool Definition

#### Function Tool Structure

```json
{
  "type": "function",
  "function": {
    "name": "function_name",
    "description": "Description of what the function does",
    "parameters": {
      "type": "object",
      "properties": {
        "param_name": {
          "type": "string|number|boolean|array|object",
          "description": "Parameter description",
          "enum": ["optional", "enum", "values"]
        }
      },
      "required": ["param_name"],
      "additionalProperties": false
    },
    "strict": true
  }
}
```

#### Parameters Schema

The `parameters` field uses JSON Schema specification:

- **type**: Data type (object, string, number, boolean, array, null)
- **properties**: Object properties
- **required**: Required property names
- **description**: Human-readable description
- **enum**: Allowed values
- **additionalProperties**: Whether additional properties are allowed
- **items**: Array item schema

#### Strict Mode

Setting `"strict": true` ensures function calls reliably adhere to the schema (recommended).

### Tool Choice Parameter

Control which tool the model uses:

```json
{
  "tool_choice": "auto"  // Model decides (default)
}

{
  "tool_choice": "none"  // No tool calls
}

{
  "tool_choice": {
    "type": "function",
    "function": {
      "name": "specific_function"
    }
  }
}
```

### Parallel Tool Calls

Enable or disable parallel function calling:

```json
{
  "parallel_tool_calls": true  // Allow multiple tools in one turn (default)
}

{
  "parallel_tool_calls": false  // Exactly zero or one tool call
}
```

### Request Example with Tools

```json
{
  "model": "gpt-5",
  "messages": [
    {
      "role": "user",
      "content": "What's the weather in Boston and New York?"
    }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "get_current_weather",
        "description": "Get the current weather in a given location",
        "parameters": {
          "type": "object",
          "properties": {
            "location": {
              "type": "string",
              "description": "The city and state, e.g. San Francisco, CA"
            },
            "unit": {
              "type": "string",
              "enum": ["celsius", "fahrenheit"],
              "description": "The temperature unit"
            }
          },
          "required": ["location"],
          "additionalProperties": false
        },
        "strict": true
      }
    }
  ],
  "tool_choice": "auto",
  "parallel_tool_calls": true
}
```

### Response with Tool Calls

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1686676106,
  "model": "gpt-5",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": null,
        "tool_calls": [
          {
            "id": "call_abc123",
            "type": "function",
            "function": {
              "name": "get_current_weather",
              "arguments": "{\"location\": \"Boston, MA\", \"unit\": \"fahrenheit\"}"
            }
          },
          {
            "id": "call_abc456",
            "type": "function",
            "function": {
              "name": "get_current_weather",
              "arguments": "{\"location\": \"New York, NY\", \"unit\": \"fahrenheit\"}"
            }
          }
        ]
      },
      "finish_reason": "tool_calls"
    }
  ],
  "usage": {
    "prompt_tokens": 82,
    "completion_tokens": 17,
    "total_tokens": 99
  }
}
```

### Submitting Tool Results

After executing the function, submit results back to the model:

```json
{
  "model": "gpt-5",
  "messages": [
    {
      "role": "user",
      "content": "What's the weather in Boston?"
    },
    {
      "role": "assistant",
      "content": null,
      "tool_calls": [
        {
          "id": "call_abc123",
          "type": "function",
          "function": {
            "name": "get_current_weather",
            "arguments": "{\"location\": \"Boston, MA\"}"
          }
        }
      ]
    },
    {
      "role": "tool",
      "tool_call_id": "call_abc123",
      "content": "{\"temperature\": 72, \"unit\": \"fahrenheit\", \"description\": \"sunny\"}"
    }
  ]
}
```

### Tool Call Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique identifier for this tool call |
| `type` | string | Always "function" |
| `function.name` | string | Function name called |
| `function.arguments` | string | JSON string of arguments |

### Parsing Arguments

The `arguments` field is a JSON string that must be parsed:

```python
import json
tool_call = response.choices[0].message.tool_calls[0]
args = json.loads(tool_call.function.arguments)
```

---

## Error Responses

### HTTP Status Codes

| Code | Meaning | Description |
|------|---------|-------------|
| 400 | Bad Request | Invalid request format or parameters |
| 401 | Unauthorized | Invalid or missing API key |
| 403 | Forbidden | Insufficient permissions |
| 404 | Not Found | Resource not found |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Server error |
| 503 | Service Unavailable | Service temporarily unavailable |

### Error Response Structure

```json
{
  "error": {
    "message": "Detailed error message",
    "type": "error_type",
    "param": "parameter_name_if_applicable",
    "code": "error_code"
  }
}
```

### Error Fields

| Field | Type | Description |
|-------|------|-------------|
| `error.message` | string | Human-readable error description |
| `error.type` | string | Error type classification |
| `error.param` | string/null | Parameter that caused the error |
| `error.code` | string/integer | Error code identifier |

### Common Error Types

- `invalid_request_error` - Malformed request
- `authentication_error` - Authentication failed
- `permission_error` - Insufficient permissions
- `not_found_error` - Resource not found
- `rate_limit_error` - Rate limit exceeded
- `api_error` - Server-side error
- `BadRequestError` - Bad request

### Error Code Examples

- `insufficient_quota` - Account quota exceeded
- `invalid_api_key` - API key is invalid
- `string_above_max_length` - Input exceeds max length
- `context_length_exceeded` - Context window exceeded

### Example Error Responses

#### Invalid API Key
```json
{
  "error": {
    "message": "Incorrect API key provided: sk-invalid. You can find your API key at https://platform.openai.com/account/api-keys.",
    "type": "invalid_request_error",
    "param": null,
    "code": "invalid_api_key"
  }
}
```

#### Rate Limit Exceeded
```json
{
  "error": {
    "message": "Rate limit reached for requests",
    "type": "rate_limit_error",
    "param": null,
    "code": "rate_limit_exceeded"
  }
}
```

#### Context Length Exceeded
```json
{
  "error": {
    "message": "This model's maximum context length is 8192 tokens. However, your messages resulted in 10000 tokens. Please reduce the length of the messages.",
    "type": "invalid_request_error",
    "param": "messages",
    "code": "context_length_exceeded"
  }
}
```

#### Invalid JSON Mode
```json
{
  "error": {
    "message": "'messages' must contain the word 'json' in some form, to use 'response_format' of type 'json_object'.",
    "type": "invalid_request_error",
    "param": "messages",
    "code": null
  }
}
```

### Streaming Errors

During streaming, errors can occur mid-stream. Check for error events:

```
event: error
data: {"error": {"message": "Error occurred", "type": "api_error"}}
```

---

## Models

### Current Models (2025)

#### GPT Series

- **gpt-5** - Latest flagship model, most capable
- **gpt-5-mini** - Balanced performance and cost, default model
- **gpt-5-nano** - Fast, cost-efficient for simple tasks
- **gpt-4.1-2025-04-14** - GPT-4 series update (April 2025)
- **gpt-4.1-nano-2025-04-14** - Smaller GPT-4.1 variant
- **gpt-4o** - Optimized GPT-4
- **gpt-4o-mini** - Smaller optimized variant

#### Reasoning Models

- **o3** - Advanced reasoning model
- **o3-mini** - Smaller reasoning model
- **o1** - Previous generation reasoning
- **o1-mini** - Smaller o1 variant

#### Embedding Models

- **text-embedding-3-large** - 3072 dimensions (most capable)
- **text-embedding-3-small** - 1536 dimensions (optimized for latency)

### Model Capabilities

#### Reasoning Models (o3, o3-mini, o1)

- Perform internal reasoning before generating output
- Support `tool_choice` parameter
- Better performance with Responses API
- Do NOT support: `temperature`, `top_p`, `presence_penalty`, `frequency_penalty`
- Use `max_completion_tokens` parameter

#### GPT Models

- Support all standard parameters
- Can use both Chat Completions and Responses APIs
- Support multimodal inputs (text, images, audio depending on model)

### Model Selection Guidance

| Task Type | Recommended API | Recommended Model |
|-----------|----------------|-------------------|
| Chat, completion | Chat Completions | gpt-5, gpt-4.1 |
| Reasoning, complex logic | Responses | o3, o3-mini |
| Agents, multi-turn | Responses | o3, o3-mini |
| Embeddings | Embeddings endpoint | text-embedding-3-large/small |

---

## API Comparison

### Chat Completions vs Responses API

| Feature | Chat Completions | Responses API |
|---------|-----------------|---------------|
| **Endpoint** | `/v1/chat/completions` | `/v1/responses` |
| **Input format** | `messages` array | `input` (string or array) |
| **System messages** | `messages[].role = "system"` | `instructions` parameter |
| **State management** | Client-side | Server-side (`store`, `previous_response_id`) |
| **Response field** | `choices[].message` | `output[]` |
| **Object type** | `chat.completion` | `response` |
| **Streaming object** | `chat.completion.chunk` | Semantic events |
| **Status field** | None (HTTP only) | `status` field in response |
| **Reasoning models** | Supported | Optimized (3% better performance) |
| **Built-in tools** | No | Yes (web search, file search, code interpreter, MCP) |
| **Maturity** | Stable, industry standard | New (March 2025) |
| **Recommendation** | Standard use cases | Agent workflows, reasoning tasks |

---

## Implementation Examples

### Basic Chat Completion

```bash
curl https://api.openai.com/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [
      {"role": "system", "content": "You are a helpful assistant."},
      {"role": "user", "content": "Explain quantum computing briefly."}
    ],
    "max_completion_tokens": 150,
    "temperature": 0.7
  }'
```

### Streaming Chat Completion

```bash
curl https://api.openai.com/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-4o-mini",
    "messages": [
      {"role": "user", "content": "Count to 10"}
    ],
    "stream": true
  }'
```

### Function Calling

```bash
curl https://api.openai.com/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-5",
    "messages": [
      {"role": "user", "content": "What is the weather in Boston?"}
    ],
    "tools": [
      {
        "type": "function",
        "function": {
          "name": "get_weather",
          "description": "Get current temperature for a location",
          "parameters": {
            "type": "object",
            "properties": {
              "location": {"type": "string"}
            },
            "required": ["location"]
          }
        }
      }
    ]
  }'
```

### Responses API

```bash
curl https://api.openai.com/v1/responses \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "o3",
    "input": "What is the weather in Boston?",
    "instructions": "You are a helpful assistant.",
    "tools": [
      {
        "type": "function",
        "name": "get_weather",
        "parameters": {
          "type": "object",
          "properties": {
            "location": {"type": "string"}
          },
          "required": ["location"]
        }
      }
    ],
    "reasoning": {
      "effort": "medium"
    },
    "store": true
  }'
```

### Chained Responses

```bash
# First request
curl https://api.openai.com/v1/responses \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "o3",
    "input": "What is 2+2?",
    "store": true
  }'
# Returns: {"id": "resp_123", ...}

# Second request referencing first
curl https://api.openai.com/v1/responses \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "o3",
    "input": "Now multiply that by 3",
    "previous_response_id": "resp_123",
    "store": true
  }'
```

---

## Best Practices

### 1. Error Handling

Always check:
- HTTP status codes
- `error` field in response
- `finish_reason` field (`length` may indicate truncation)
- `status` field in Responses API

### 2. Streaming

- Handle `data: [DONE]` termination marker
- Parse each SSE line as separate JSON
- Handle partial tool calls in streaming
- Check for error events mid-stream

### 3. Tool Calling

- Always use `"strict": true` for reliable schema adherence
- Parse `arguments` as JSON string
- Handle multiple tool calls with `parallel_tool_calls`
- Return tool results with correct `tool_call_id`

### 4. Rate Limits

- Implement exponential backoff for 429 errors
- Monitor usage via dashboard
- Use batch API for large workloads

### 5. Security

- Never embed API keys in client-side code
- Use environment variables for keys
- Rotate keys periodically
- Use organization/project headers for team accounts

### 6. Model Selection

- Use Responses API for reasoning models (o3, o3-mini)
- Use Chat Completions for standard workflows
- Enable `store: true` for multi-turn conversations
- Consider `max_completion_tokens` limits

---

## Sources

### Official OpenAI Documentation

- [Chat Completions API Reference](https://platform.openai.com/docs/api-reference/chat)
- [Responses API Reference](https://platform.openai.com/docs/api-reference/responses)
- [Streaming API Reference](https://platform.openai.com/docs/api-reference/streaming)
- [Responses Streaming Events](https://platform.openai.com/docs/api-reference/responses-streaming)
- [Function Calling Guide](https://platform.openai.com/docs/guides/function-calling)
- [Using Tools Guide](https://platform.openai.com/docs/guides/tools)
- [Streaming Responses Guide](https://platform.openai.com/docs/guides/streaming-responses)
- [Migrate to Responses API](https://platform.openai.com/docs/guides/migrate-to-responses)
- [Error Codes Guide](https://platform.openai.com/docs/guides/error-codes)
- [API Authentication](https://platform.openai.com/docs/api-reference/authentication)

### Community Resources

- [Responses API Community Guide](https://community.openai.com/t/responses-api-streaming-the-simple-guide-to-events/1363122)
- [OpenAI Cookbook - How to Call Functions](https://github.com/openai/openai-cookbook/blob/main/examples/How_to_call_functions_with_chat_models.ipynb)
- [OpenAI Cookbook - How to Stream Completions](https://github.com/openai/openai-cookbook/blob/main/examples/How_to_stream_completions.ipynb)
- [OpenAI Cookbook - Responses API Examples](https://cookbook.openai.com/examples/responses_api/responses_example)

### Third-Party Documentation

- [Azure OpenAI Responses API](https://learn.microsoft.com/en-us/azure/ai-foundry/openai/how-to/responses)
- [Azure OpenAI Function Calling](https://learn.microsoft.com/en-us/azure/ai-foundry/openai/how-to/function-calling)
- [OpenAI Responses API Guide - DataCamp](https://www.datacamp.com/tutorial/openai-responses-api)
- [OpenAI Function Calling Tutorial - DataCamp](https://www.datacamp.com/tutorial/open-ai-function-calling-tutorial)

### Blog Posts and Tutorials

- [OpenAI API: Responses vs. Chat Completions - Simon Willison](https://simonwillison.net/2025/Mar/11/responses-vs-chat-completions/)
- [How to Use cURL OpenAI API Tutorial](https://muneebdev.com/curl-openai-api-tutorial/)
- [Real-time OpenAI Streaming with FastAPI](https://sevalla.com/blog/real-time-openai-streaming-fastapi/)
- [Building OpenAI-Compatible Streaming Interface](https://medium.com/@moustafa.abdelbaky/building-an-openai-compatible-streaming-interface-using-server-sent-events-with-fastapi-and-8f014420bca7)

---

## Version Information

- **Document Version**: 2.0
- **Last Updated**: December 14, 2025
- **API Version**: v1
- **Latest Models**: GPT-5, o3, o3-mini (2025)
- **Responses API**: Released March 2025
