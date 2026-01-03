# xAI Grok API Technical Specification

## Overview

The xAI Grok API provides access to Grok models through an OpenAI-compatible REST API. Base URL: `https://api.x.ai/v1`

## 1. API Endpoints

### 1.1 Chat Completions
- **URL**: `https://api.x.ai/v1/chat/completions`
- **Method**: `POST`
- **Content-Type**: `application/json`

### 1.2 Other Endpoints
- `GET https://api.x.ai/v1/models` - List available models
- `POST https://api.x.ai/v1/completions` - Text completions
- `POST https://api.x.ai/v1/embeddings` - Embeddings
- `POST https://api.x.ai/v1/tokenize` - Tokenization
- `POST https://api.x.ai/v1/fine-tunes` - Fine-tuning management

## 2. Authentication

### 2.1 Header Format
```
Authorization: Bearer <your-api-key>
```

### 2.2 API Key Format
- API keys start with `xai-`
- Environment variable: `XAI_API_KEY`

### 2.3 Example cURL
```bash
curl https://api.x.ai/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $XAI_API_KEY" \
  -d '{
    "messages": [
      {
        "role": "system",
        "content": "You are Grok, a highly intelligent, helpful AI assistant."
      },
      {
        "role": "user",
        "content": "What is the meaning of life?"
      }
    ],
    "model": "grok-4",
    "stream": false
  }'
```

## 3. Available Models

### 3.1 Model IDs and Context Windows

| Model ID | Context Window | Capabilities |
|----------|----------------|--------------|
| `grok-4` | 256,000 tokens | Frontier-level multimodal, advanced reasoning |
| `grok-4-0709` | 256,000 tokens | July 2025 snapshot |
| `grok-4-1-fast-reasoning` | 2,000,000 tokens | Maximum intelligence with reasoning |
| `grok-4-1-fast-non-reasoning` | 2,000,000 tokens | Instant responses, optimized for agentic tool calling |
| `grok-4-fast-reasoning` | - | Fast variant with reasoning |
| `grok-4-fast-non-reasoning` | - | Fast variant without reasoning |
| `grok-code-fast-1` | - | Code-specialized model |
| `grok-3-beta` | 131,072 tokens | General purpose |
| `grok-3-mini-beta` | - | Compact with reasoning capabilities |
| `grok-2-1212` | - | Better accuracy, multilingual (Dec 2024 release) |
| `grok-2-latest` | - | Alias to latest grok-2 (currently grok-2-1212) |
| `grok-2-vision-1212` | - | Vision model (Dec 2024 release) |
| `grok-2-image-1212` | - | Image generation model |
| `grok-beta` | 128,000 tokens | Function calling, system prompts |
| `grok-vision-beta` | 8,000 tokens | Image understanding |

**Knowledge Cutoff**: November 2024 (for Grok 3 and Grok 4)

## 4. Chat Completions Request Format

### 4.1 Required Parameters

```json
{
  "model": "grok-4",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant."
    },
    {
      "role": "user",
      "content": "Hello!"
    }
  ]
}
```

### 4.2 Optional Parameters

| Parameter | Type | Default | Range/Values | Description |
|-----------|------|---------|--------------|-------------|
| `temperature` | float | 1.0 | 0.0 - 2.0 | Sampling temperature. Higher = more random |
| `max_tokens` | integer | - | - | Maximum tokens to generate |
| `top_p` | float | 1.0 | 0.0 - 1.0 | Nucleus sampling. Alternative to temperature |
| `stream` | boolean | false | true/false | Enable SSE streaming |
| `stop` | string or array | null | Up to 4 sequences | Stop sequences |
| `presence_penalty` | float | 0.0 | -2.0 - 2.0 | Penalize tokens based on presence |
| `frequency_penalty` | float | 0.0 | -2.0 - 2.0 | Penalize tokens based on frequency |
| `n` | integer | 1 | - | Number of completions to generate |
| `logprobs` | boolean | false | true/false | Return log probabilities |
| `top_logprobs` | integer | null | 0 - 20 | Number of most likely tokens to return |
| `logit_bias` | object | null | - | Token ID bias dictionary |
| `seed` | integer | null | - | Random seed for deterministic outputs |
| `tools` | array | null | Max 128 | Function calling tools |
| `tool_choice` | string or object | "auto" | "none", "auto", or object | Control tool selection |
| `response_format` | object | null | - | Structured output format |

**Note**: Do not use `temperature` and `top_p` together. Choose one.

**Note**: Reasoning models do NOT support `presence_penalty`, `frequency_penalty`, or `stop` parameters. Adding them will cause an error.

### 4.3 Complete Request Example

```json
{
  "model": "grok-4",
  "messages": [
    {
      "role": "system",
      "content": "You are Grok, a highly intelligent, helpful AI assistant."
    },
    {
      "role": "user",
      "content": "What is the capital of France?"
    }
  ],
  "temperature": 0.7,
  "max_tokens": 100,
  "top_p": 1.0,
  "stream": false,
  "stop": ["\n\n"],
  "presence_penalty": 0.0,
  "frequency_penalty": 0.0
}
```

## 5. Chat Completions Response Format

### 5.1 Non-Streaming Response

```json
{
  "id": "0daf962f-a275-4a3c-839a-047854645532",
  "object": "chat.completion",
  "created": 1739301120,
  "model": "grok-3-latest",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "The capital of France is Paris.",
        "refusal": null
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 41,
    "completion_tokens": 104,
    "total_tokens": 145
  },
  "system_fingerprint": "fp_84ff176447"
}
```

### 5.2 Response Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | Unique completion ID |
| `object` | string | Always "chat.completion" |
| `created` | integer | Unix timestamp |
| `model` | string | Model used |
| `choices` | array | Array of completion choices |
| `choices[].index` | integer | Choice index |
| `choices[].message` | object | Message object |
| `choices[].message.role` | string | Always "assistant" |
| `choices[].message.content` | string | Response text |
| `choices[].message.refusal` | string or null | Refusal message if applicable |
| `choices[].finish_reason` | string | "stop", "length", "tool_calls", "content_filter" |
| `usage` | object | Token usage statistics |
| `usage.prompt_tokens` | integer | Tokens in prompt |
| `usage.completion_tokens` | integer | Tokens in completion |
| `usage.total_tokens` | integer | Total tokens used |
| `system_fingerprint` | string | System configuration fingerprint |

## 6. System Messages

### 6.1 Unique Feature: Flexible Role Ordering
Unlike most APIs, xAI allows **any order** of message roles. You can mix `system`, `user`, and `assistant` messages in any sequence.

```json
{
  "messages": [
    {"role": "user", "content": "Hello"},
    {"role": "system", "content": "Be concise"},
    {"role": "assistant", "content": "Hi there!"},
    {"role": "system", "content": "Now be verbose"},
    {"role": "user", "content": "Tell me about AI"}
  ]
}
```

## 7. Streaming (Server-Sent Events)

### 7.1 Enable Streaming
Set `"stream": true` in request.

### 7.2 SSE Event Format

Each event is prefixed with `data: ` followed by JSON:

```
data: {"id":"chat-abc123","object":"chat.completion.chunk","created":1739301120,"model":"grok-4","choices":[{"index":0,"delta":{"role":"assistant","content":"Ah"}}],"usage":{"prompt_tokens":41,"completion_tokens":1,"total_tokens":42,"prompt_tokens_details":{"text_tokens":41,"audio_tokens":0,"image_tokens":0,"cached_tokens":0}},"system_fingerprint":"fp_xxxxxxxxxx"}

data: {"id":"chat-abc123","object":"chat.completion.chunk","created":1739301120,"model":"grok-4","choices":[{"index":0,"delta":{"content":","}}],"usage":{"prompt_tokens":41,"completion_tokens":2,"total_tokens":43,"prompt_tokens_details":{"text_tokens":41,"audio_tokens":0,"image_tokens":0,"cached_tokens":0}},"system_fingerprint":"fp_xxxxxxxxxx"}

data: {"id":"chat-abc123","object":"chat.completion.chunk","created":1739301120,"model":"grok-4","choices":[{"index":0,"delta":{"content":" the"}}],"usage":{"prompt_tokens":41,"completion_tokens":3,"total_tokens":44,"prompt_tokens_details":{"text_tokens":41,"audio_tokens":0,"image_tokens":0,"cached_tokens":0}},"system_fingerprint":"fp_xxxxxxxxxx"}

data: [DONE]
```

### 7.3 Stream Chunk Structure

```json
{
  "id": "chat-abc123",
  "object": "chat.completion.chunk",
  "created": 1739301120,
  "model": "grok-4",
  "choices": [
    {
      "index": 0,
      "delta": {
        "role": "assistant",
        "content": "text chunk"
      }
    }
  ],
  "usage": {
    "prompt_tokens": 41,
    "completion_tokens": 1,
    "total_tokens": 42,
    "prompt_tokens_details": {
      "text_tokens": 41,
      "audio_tokens": 0,
      "image_tokens": 0,
      "cached_tokens": 0
    }
  },
  "system_fingerprint": "fp_xxxxxxxxxx"
}
```

### 7.4 Delta Object

First chunk includes `role`:
```json
{"delta": {"role": "assistant", "content": "Hello"}}
```

Subsequent chunks:
```json
{"delta": {"content": " world"}}
```

Final chunk:
```json
{"delta": {}, "finish_reason": "stop"}
```

### 7.5 Stream Termination
Stream ends with:
```
data: [DONE]
```

## 8. Function Calling

### 8.1 Tool Definition Schema

```json
{
  "type": "function",
  "function": {
    "name": "get_current_temperature",
    "description": "Get the current temperature in a given location",
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
          "default": "fahrenheit"
        }
      },
      "required": ["location"]
    }
  }
}
```

### 8.2 Request with Tools

```json
{
  "model": "grok-4",
  "messages": [
    {
      "role": "user",
      "content": "What's the weather in San Francisco?"
    }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "get_current_temperature",
        "description": "Get the current temperature in a given location",
        "parameters": {
          "type": "object",
          "properties": {
            "location": {
              "type": "string",
              "description": "The city and state, e.g. San Francisco, CA"
            },
            "unit": {
              "type": "string",
              "enum": ["celsius", "fahrenheit"]
            }
          },
          "required": ["location"]
        }
      }
    }
  ],
  "tool_choice": "auto"
}
```

### 8.3 Response with Tool Calls

```json
{
  "id": "chat-abc123",
  "object": "chat.completion",
  "created": 1739301120,
  "model": "grok-4",
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
              "name": "get_current_temperature",
              "arguments": "{\"location\": \"San Francisco, CA\", \"unit\": \"fahrenheit\"}"
            }
          }
        ]
      },
      "finish_reason": "tool_calls"
    }
  ],
  "usage": {
    "prompt_tokens": 45,
    "completion_tokens": 20,
    "total_tokens": 65
  }
}
```

### 8.4 Sending Tool Results Back

```json
{
  "model": "grok-4",
  "messages": [
    {
      "role": "user",
      "content": "What's the weather in San Francisco?"
    },
    {
      "role": "assistant",
      "content": null,
      "tool_calls": [
        {
          "id": "call_abc123",
          "type": "function",
          "function": {
            "name": "get_current_temperature",
            "arguments": "{\"location\": \"San Francisco, CA\", \"unit\": \"fahrenheit\"}"
          }
        }
      ]
    },
    {
      "role": "tool",
      "content": "{\"temperature\": 72, \"unit\": \"fahrenheit\"}",
      "tool_call_id": "call_abc123"
    }
  ],
  "tools": [...]
}
```

### 8.5 Tool Choice Parameter

Control which tool is called:

```json
// Auto selection (default)
{"tool_choice": "auto"}

// No tool calling
{"tool_choice": "none"}

// Force specific function
{
  "tool_choice": {
    "type": "function",
    "function": {"name": "get_current_temperature"}
  }
}
```

### 8.6 Parallel Function Calling

Enabled by default. Multiple functions can be called in one response:

```json
{
  "choices": [
    {
      "message": {
        "role": "assistant",
        "tool_calls": [
          {
            "id": "call_1",
            "type": "function",
            "function": {"name": "get_temperature", "arguments": "..."}
          },
          {
            "id": "call_2",
            "type": "function",
            "function": {"name": "get_forecast", "arguments": "..."}
          }
        ]
      }
    }
  ]
}
```

### 8.7 Function Calling Limits

- **Maximum tools per request**: 128 functions
- **Streaming behavior**: Function calls returned **whole in a single chunk**, not streamed token-by-token

### 8.8 Streaming Function Calls

When `stream: true`:
```
data: {"choices":[{"delta":{"role":"assistant","tool_calls":[{"id":"call_abc","type":"function","function":{"name":"get_temperature","arguments":"{\"location\":\"NYC\"}"}}]}}]}

data: {"choices":[{"delta":{},"finish_reason":"tool_calls"}]}

data: [DONE]
```

## 9. Structured Outputs

### 9.1 Response Format Parameter

```json
{
  "model": "grok-4",
  "messages": [...],
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "person_info",
      "schema": {
        "type": "object",
        "properties": {
          "name": {"type": "string"},
          "age": {"type": "number"},
          "email": {"type": "string"}
        },
        "required": ["name", "age"],
        "additionalProperties": false
      },
      "strict": true
    }
  }
}
```

### 9.2 Model Support
Supported by models after `grok-2-1212` and `grok-2-vision-1212`.

### 9.3 Limitations
- **Not compatible with streaming**: Cannot use `stream: true` with structured outputs
- **No string length constraints**: `minLength` and `maxLength` are not supported

### 9.4 Example Response

```json
{
  "choices": [
    {
      "message": {
        "role": "assistant",
        "content": "{\"name\": \"John Doe\", \"age\": 30, \"email\": \"john@example.com\"}"
      }
    }
  ]
}
```

## 10. Image Understanding (Vision)

### 10.1 Vision Models
- `grok-2-vision-1212`
- `grok-vision-beta`

### 10.2 Image Input Formats

#### URL Format
```json
{
  "model": "grok-2-vision-1212",
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "What's in this image?"
        },
        {
          "type": "image_url",
          "image_url": {
            "url": "https://example.com/image.jpg",
            "detail": "high"
          }
        }
      ]
    }
  ]
}
```

#### Base64 Format
```json
{
  "type": "image_url",
  "image_url": {
    "url": "data:image/jpeg;base64,/9j/4AAQSkZJRg...",
    "detail": "high"
  }
}
```

### 10.3 Supported Image Types
- JPEG (.jpg, .jpeg)
- PNG (.png)

### 10.4 Detail Parameter
- `"high"` - High resolution analysis
- `"low"` - Lower resolution, faster processing

### 10.5 Multiple Images
```json
{
  "content": [
    {"type": "text", "text": "Compare these images:"},
    {"type": "image_url", "image_url": {"url": "https://example.com/img1.jpg"}},
    {"type": "image_url", "image_url": {"url": "https://example.com/img2.jpg"}}
  ]
}
```

## 11. Error Responses

### 11.1 Error Response Format

```json
{
  "error": {
    "message": "Invalid API key",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

### 11.2 HTTP Status Codes

| Status Code | Error Type | Description |
|-------------|------------|-------------|
| 400 | Bad Request | Invalid request parameters |
| 401 | Unauthorized | Invalid or missing API key |
| 403 | Forbidden | API key doesn't have permission |
| 404 | Not Found | Resource not found |
| 429 | Too Many Requests | Rate limit exceeded |
| 500 | Internal Server Error | Server-side error |
| 503 | Service Unavailable | Service temporarily unavailable |

### 11.3 Common Error Codes

**Authentication Errors**
```json
{
  "error": {
    "message": "Invalid authentication credentials",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

**Rate Limit Errors**
```json
{
  "error": {
    "message": "Rate limit exceeded. Please wait and retry.",
    "type": "rate_limit_error",
    "code": "rate_limit_exceeded"
  }
}
```

**Invalid Request Errors**
```json
{
  "error": {
    "message": "Invalid request: The prompt parameter is required",
    "type": "invalid_request_error",
    "code": "invalid_request"
  }
}
```

**Model Error**
```json
{
  "error": {
    "message": "The model 'invalid-model' does not exist",
    "type": "invalid_request_error",
    "code": "model_not_found"
  }
}
```

**Data Loss Error**
- Occurs when server fails to fetch image from provided URL
- Unrecoverable data loss or corruption

## 12. Rate Limits

### 12.1 Current Limits
- **1 request per second** (all users)
- **60 requests per hour** (some models)
- **1,200 requests per hour** (other models)
- **16,000 tokens per minute** (Grok 4 for teams)

### 12.2 Rate Limit Headers

Response includes headers:
```
x-ratelimit-limit-requests: 1200
x-ratelimit-remaining-requests: 1150
x-ratelimit-reset-requests: 1739305200
```

### 12.3 Rate Limit Exceeded Response

HTTP 429 with body:
```json
{
  "error": {
    "message": "Rate limit exceeded. Please wait and retry.",
    "type": "rate_limit_error",
    "code": "rate_limit_exceeded"
  }
}
```

### 12.4 Handling Rate Limits
- Implement exponential backoff
- Monitor rate limit headers
- Distribute requests over time
- Check xAI Console for account-specific limits

## 13. Pricing

### 13.1 Token-Based Pricing
- Input tokens (prompt) priced separately from output tokens (completion)
- Pricing varies by model
- Check xAI Console for current rates

### 13.2 Tool Invocation Pricing
Requests using server-side tools priced on:
- Token usage
- Server-side tool invocations
- Cost scales with query complexity

### 13.3 Free Tier
- Currently available in some regions
- Check xAI Console for eligibility

## 14. SDK Compatibility

### 14.1 OpenAI SDK
```python
from openai import OpenAI

client = OpenAI(
    api_key="xai-...",
    base_url="https://api.x.ai/v1"
)

response = client.chat.completions.create(
    model="grok-4",
    messages=[
        {"role": "user", "content": "Hello!"}
    ]
)
```

### 14.2 Anthropic SDK
Also compatible with Anthropic SDK (adapter required).

### 14.3 Official xAI SDK
```python
from xai_sdk import XAI

client = XAI(api_key="xai-...")
response = client.chat.completions.create(
    model="grok-4",
    messages=[{"role": "user", "content": "Hello!"}]
)
```

## 15. Unique Features

1. **Flexible role ordering**: No restrictions on message role sequence
2. **OpenAI + Anthropic SDK compatibility**: Works with both major SDKs
3. **Function call streaming**: Whole function calls in single chunk
4. **High tool limit**: Up to 128 tools per request
5. **Massive context windows**: Up to 2M tokens (Grok 4.1 Fast)
6. **Vision support**: Multimodal understanding
7. **Parallel function calling**: Multiple tools called simultaneously
8. **Structured outputs**: Guaranteed JSON schema compliance

## 16. Tokenization

### 16.1 Tokenizer Details
- **Algorithm**: SentencePiece BPE
- **Vocabulary size**: 131,072 tokens
- **Token counts vary by model**: Same prompt may have different counts on different Grok models

### 16.2 Tokenizer Endpoint
```bash
curl https://api.x.ai/v1/tokenize \
  -H "Authorization: Bearer $XAI_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"text": "Hello, world!"}'
```

### 16.3 Important Note
- xAI uses its own tokenizer (NOT OpenAI's)
- Token counts differ from OpenAI even with same text
- Use xAI tokenizer for accurate cost estimation
- Tokenizer file: https://github.com/xai-org/grok-1/blob/main/tokenizer.model

## 17. Best Practices

### 17.1 Message Structure
- Include system message for consistent behavior
- Maintain conversation history in messages array
- API is stateless - send full context each time

### 17.2 Temperature vs Top_P
- Use one or the other, not both
- Temperature 0.7-0.9 for creative tasks
- Temperature 0.1-0.3 for factual tasks
- Top_p 0.9 for balanced output

### 17.3 Streaming
- Use for long responses to improve UX
- Set timeout to avoid premature connection close (especially for reasoning models)
- Parse SSE format correctly

### 17.4 Function Calling
- Provide clear, detailed function descriptions
- Use JSON Schema for parameter validation
- Handle tool results in next request
- Max 128 tools per request

### 17.5 Error Handling
- Implement retry logic with exponential backoff
- Check status codes and error types
- Monitor rate limit headers
- Log errors for debugging

## Sources

- [xAI API Overview](https://docs.x.ai/docs/overview)
- [Chat Guide](https://docs.x.ai/docs/guides/chat)
- [Function Calling Guide](https://docs.x.ai/docs/guides/function-calling)
- [Streaming Response Guide](https://docs.x.ai/docs/guides/streaming-response)
- [Structured Outputs Guide](https://docs.x.ai/docs/guides/structured-outputs)
- [Image Understanding Guide](https://docs.x.ai/docs/guides/image-understanding)
- [Models and Pricing](https://docs.x.ai/docs/models)
- [REST API Reference](https://docs.x.ai/docs/api-reference)
- [Consumption and Rate Limits](https://docs.x.ai/docs/key-information/consumption-and-rate-limits)
- [The Hitchhiker's Guide to Grok](https://docs.x.ai/docs/tutorial)
- [Function Calling 101 - xAI Cookbook](https://docs.x.ai/cookbook/examples/function_calling_101)
- [Complete Guide to xAI's Grok API](https://latenode.com/blog/complete-guide-to-xais-grok-api-documentation-and-implementation)
- [Grok 4 Documentation](https://docs.x.ai/docs/models/grok-4-0709)
- [xAI Grok API Code Examples](https://mer.vin/2024/11/xai-grok-api-code/)
