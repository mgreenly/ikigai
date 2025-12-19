# Meta Llama API Technical Specification

## 1. API Endpoints

### Base URLs

**Official Meta Llama API:**
- Base URL: `https://api.llama.com`
- Chat Completions: `https://api.llama.com/v1/chat/completions`
- Models List: `https://api.llama.com/v1/models`
- Moderations: `https://api.llama.com/v1/moderations`

**OpenAI-Compatible Endpoint:**
- Base URL: `https://api.llama.com/compat/v1`
- Chat Completions: `https://api.llama.com/compat/v1/chat/completions`
- Models: `https://api.llama.com/compat/v1/models`

### HTTP Methods

All endpoints use `POST` for requests (except models list which uses `GET`).

## 2. Authentication

### Header Format

```
Authorization: Bearer $LLAMA_API_KEY
Content-Type: application/json
```

### Example Request Headers

```json
{
  "Authorization": "Bearer llama_api_abc123def456",
  "Content-Type": "application/json"
}
```

## 3. Request Format - Chat Completions

### Endpoint

`POST https://api.llama.com/v1/chat/completions`

### Required Parameters

- **model** (string): Model identifier
  - Examples: `"Llama-4-Maverick-17B-128E-Instruct-FP8"`, `"meta-llama/llama-4-maverick"`, `"meta-llama/llama-3.3-70b-versatile"`

- **messages** (array): Array of message objects with roles and content

### Optional Parameters

- **stream** (boolean): Enable SSE streaming. Default: `false`
- **temperature** (number): Controls randomness (0.0-1.0). Default: `0.6`
- **top_p** (number): Nucleus sampling threshold (0.0-1.0). Default: `0.9`
- **top_k** (integer): Sample from top K tokens. Default: varies by model
- **max_completion_tokens** (integer): Maximum tokens in response. Default: `4096`
- **repetition_penalty** (number): Penalty for token repetition (1.0-2.0). Default: `1.0`
- **user** (string): End-user identifier for abuse monitoring
- **tools** (array): Array of tool/function definitions
- **tool_choice** (string | object): Control tool calling behavior
- **response_format** (object): Structured output format specification

### Basic Request Example

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful AI assistant."
    },
    {
      "role": "user",
      "content": "What is the capital of France?"
    }
  ],
  "temperature": 0.7,
  "max_completion_tokens": 1000
}
```

### Request with Images (Llama 4 Maverick)

```json
{
  "model": "meta-llama/llama-4-maverick",
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
            "url": "https://example.com/image.jpg"
          }
        }
      ]
    }
  ]
}
```

### Request with Base64 Image

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "Describe this image"
        },
        {
          "type": "image_url",
          "image_url": {
            "url": "data:image/jpeg;base64,/9j/4AAQSkZJRg..."
          }
        }
      ]
    }
  ]
}
```

## 4. Response Format

### Non-Streaming Response

#### Success Response (200 OK)

```json
{
  "id": "chatcmpl-abc123xyz",
  "completion_message": {
    "role": "assistant",
    "content": "The capital of France is Paris.",
    "stop_reason": "stop"
  },
  "metrics": [
    {
      "metric": "prompt_tokens",
      "value": 25,
      "unit": "tokens"
    },
    {
      "metric": "completion_tokens",
      "value": 8,
      "unit": "tokens"
    },
    {
      "metric": "total_tokens",
      "value": 33,
      "unit": "tokens"
    },
    {
      "metric": "time_to_first_token",
      "value": 0.045,
      "unit": "seconds"
    }
  ]
}
```

#### Response Fields

- **id** (string): Unique identifier for the completion request
- **completion_message** (object): The assistant's response
  - **role** (string): Always `"assistant"`
  - **content** (string | array): Response text or content items
  - **stop_reason** (string): Reason for completion - `"stop"`, `"length"`, or `"tool_calls"`
  - **tool_calls** (array, optional): Tool calls made by the model
- **metrics** (array): Usage and performance metrics
  - **metric** (string): Metric name
  - **value** (number | integer): Metric value
  - **unit** (string): Unit of measurement

### OpenAI-Compatible Response Format

When using the `compat/v1` endpoint, responses follow OpenAI's format:

```json
{
  "id": "chatcmpl-abc123",
  "object": "chat.completion",
  "created": 1735062000,
  "model": "meta-llama/llama-4-maverick",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "The capital of France is Paris."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 25,
    "completion_tokens": 8,
    "total_tokens": 33
  }
}
```

## 5. System Messages

### Placement

System messages are placed as the first message in the `messages` array with `role: "system"`.

### Example

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful AI assistant that specializes in travel recommendations. Be concise and friendly."
    },
    {
      "role": "user",
      "content": "What should I visit in Tokyo?"
    }
  ]
}
```

### Multiple System Messages

While the API supports multiple system messages, best practice is to use a single system message at the start:

```json
{
  "messages": [
    {
      "role": "system",
      "content": "You are an expert programmer. Always provide working code examples."
    },
    {
      "role": "user",
      "content": "How do I sort an array in Python?"
    }
  ]
}
```

## 6. Tool Calling (Function Calling)

### Tools Parameter Format

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "user",
      "content": "What's the weather like in San Francisco?"
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
              "description": "The city and state, e.g., San Francisco, CA"
            },
            "unit": {
              "type": "string",
              "enum": ["celsius", "fahrenheit"],
              "description": "The unit of temperature to return"
            }
          },
          "required": ["location"]
        },
        "strict": false
      }
    }
  ],
  "tool_choice": "auto"
}
```

### Tool Definition Schema

```json
{
  "type": "function",
  "function": {
    "name": "string",
    "description": "string",
    "parameters": {
      "type": "object",
      "properties": {
        "parameter_name": {
          "type": "string | number | boolean | array | object",
          "description": "string",
          "enum": ["optional", "array", "of", "values"]
        }
      },
      "required": ["array", "of", "required", "parameter", "names"]
    },
    "strict": false
  }
}
```

### Tool Choice Options

#### Auto (Default)

Model decides whether to call tools and which ones:

```json
{
  "tool_choice": "auto"
}
```

#### None

Disable tool calling - model only generates text:

```json
{
  "tool_choice": "none"
}
```

#### Required

Model must call at least one tool:

```json
{
  "tool_choice": "required"
}
```

#### Specific Function

Force model to call a specific function:

```json
{
  "tool_choice": {
    "type": "function",
    "function": {
      "name": "get_current_weather"
    }
  }
}
```

### Tool Call Response

When the model decides to call tools, the response includes:

```json
{
  "id": "chatcmpl-xyz789",
  "completion_message": {
    "role": "assistant",
    "content": null,
    "stop_reason": "tool_calls",
    "tool_calls": [
      {
        "id": "call_abc123",
        "type": "function",
        "function": {
          "name": "get_current_weather",
          "arguments": "{\"location\": \"San Francisco, CA\", \"unit\": \"fahrenheit\"}"
        }
      }
    ]
  },
  "metrics": [...]
}
```

### Sending Tool Results Back

After executing the tool, send results back in the conversation:

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "user",
      "content": "What's the weather like in San Francisco?"
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
            "arguments": "{\"location\": \"San Francisco, CA\", \"unit\": \"fahrenheit\"}"
          }
        }
      ]
    },
    {
      "role": "tool",
      "tool_call_id": "call_abc123",
      "content": "{\"temperature\": 72, \"condition\": \"sunny\", \"humidity\": 65}"
    }
  ],
  "tools": [...]
}
```

### Complete Tool Calling Example

**Request:**

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "user",
      "content": "Calculate 25 * 48"
    }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "calculate",
        "description": "Evaluate a mathematical expression",
        "parameters": {
          "type": "object",
          "properties": {
            "expression": {
              "type": "string",
              "description": "The mathematical expression to evaluate"
            }
          },
          "required": ["expression"]
        }
      }
    }
  ],
  "tool_choice": "auto"
}
```

**Response:**

```json
{
  "id": "chatcmpl-calc001",
  "completion_message": {
    "role": "assistant",
    "content": null,
    "stop_reason": "tool_calls",
    "tool_calls": [
      {
        "id": "call_calc123",
        "type": "function",
        "function": {
          "name": "calculate",
          "arguments": "{\"expression\": \"25 * 48\"}"
        }
      }
    ]
  },
  "metrics": [...]
}
```

## 7. Structured Output (JSON Schema)

### Response Format Parameter

Request structured JSON output using `response_format`:

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "system",
      "content": "Extract the name, age, and occupation from the user's message. Return the data as JSON following the provided schema."
    },
    {
      "role": "user",
      "content": "My name is Sarah Johnson, I'm 32 years old and I work as a software engineer."
    }
  ],
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "person_info",
      "strict": true,
      "schema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string",
            "description": "Full name of the person"
          },
          "age": {
            "type": "integer",
            "description": "Age in years"
          },
          "occupation": {
            "type": "string",
            "description": "Current job or profession"
          }
        },
        "required": ["name", "age", "occupation"],
        "additionalProperties": false
      }
    }
  }
}
```

### Structured Response Example

```json
{
  "id": "chatcmpl-struct001",
  "completion_message": {
    "role": "assistant",
    "content": "{\"name\": \"Sarah Johnson\", \"age\": 32, \"occupation\": \"software engineer\"}",
    "stop_reason": "stop"
  },
  "metrics": [...]
}
```

### Complex Schema Example

```json
{
  "response_format": {
    "type": "json_schema",
    "json_schema": {
      "name": "meeting_notes",
      "schema": {
        "type": "object",
        "properties": {
          "title": {
            "type": "string"
          },
          "summary": {
            "type": "string"
          },
          "attendees": {
            "type": "array",
            "items": {
              "type": "string"
            }
          },
          "action_items": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "task": {
                  "type": "string"
                },
                "assignee": {
                  "type": "string"
                },
                "deadline": {
                  "type": "string"
                }
              },
              "required": ["task", "assignee"]
            }
          }
        },
        "required": ["title", "summary", "attendees"]
      }
    }
  }
}
```

### Best Practices for Structured Output

1. Always include schema instructions in the system message or user prompt
2. Use `"strict": true` to enforce exact schema compliance
3. Set `"additionalProperties": false` to prevent extra fields
4. Provide clear descriptions for each property
5. Test with example inputs to validate schema

## 8. Streaming (Server-Sent Events)

### Enable Streaming

Set `stream: true` in the request:

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "user",
      "content": "Write a short poem about AI"
    }
  ],
  "stream": true
}
```

### Response Headers

```
Content-Type: text/event-stream
Cache-Control: no-cache
Connection: keep-alive
```

### SSE Event Format

Each event follows this structure:

```
data: {"id":"chatcmpl-stream001","event":{"event_type":"start"}}

data: {"id":"chatcmpl-stream001","event":{"event_type":"progress","delta":{"type":"text","text":"In"}}}

data: {"id":"chatcmpl-stream001","event":{"event_type":"progress","delta":{"type":"text","text":" silicon"}}}

data: {"id":"chatcmpl-stream001","event":{"event_type":"progress","delta":{"type":"text","text":" minds"}}}

data: {"id":"chatcmpl-stream001","event":{"event_type":"complete","stop_reason":"stop","metrics":[...]}}

data: [DONE]
```

### Event Types

#### Start Event

```json
{
  "id": "chatcmpl-stream001",
  "event": {
    "event_type": "start"
  }
}
```

#### Progress Event (Text)

```json
{
  "id": "chatcmpl-stream001",
  "event": {
    "event_type": "progress",
    "delta": {
      "type": "text",
      "text": " artificial"
    }
  }
}
```

#### Progress Event (Tool Call Start)

```json
{
  "id": "chatcmpl-stream002",
  "event": {
    "event_type": "progress",
    "delta": {
      "type": "tool_call",
      "id": "call_func123",
      "function": {
        "name": "get_weather",
        "arguments": ""
      }
    }
  }
}
```

#### Progress Event (Tool Call Arguments)

```json
{
  "id": "chatcmpl-stream002",
  "event": {
    "event_type": "progress",
    "delta": {
      "type": "tool_call",
      "id": "call_func123",
      "function": {
        "arguments": "{\"location\":"
      }
    }
  }
}
```

#### Complete Event

```json
{
  "id": "chatcmpl-stream001",
  "event": {
    "event_type": "complete",
    "stop_reason": "stop",
    "metrics": [
      {
        "metric": "total_tokens",
        "value": 150,
        "unit": "tokens"
      }
    ]
  }
}
```

#### Done Signal

```
data: [DONE]
```

### Streaming Delta Types

- **"text"**: Contains partial text content
- **"tool_call"**: Contains partial tool call information

### Client-Side Consumption Example

```javascript
const response = await fetch('https://api.llama.com/v1/chat/completions', {
  method: 'POST',
  headers: {
    'Authorization': `Bearer ${LLAMA_API_KEY}`,
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    model: 'meta-llama/llama-4-maverick',
    messages: [{role: 'user', content: 'Hello!'}],
    stream: true
  })
});

const reader = response.body.getReader();
const decoder = new TextDecoder();

while (true) {
  const {done, value} = await reader.read();
  if (done) break;

  const chunk = decoder.decode(value);
  const lines = chunk.split('\n');

  for (const line of lines) {
    if (line.startsWith('data: ')) {
      const data = line.slice(6);

      if (data === '[DONE]') {
        console.log('Stream complete');
        break;
      }

      try {
        const parsed = JSON.parse(data);
        if (parsed.event?.delta?.text) {
          process.stdout.write(parsed.event.delta.text);
        }
      } catch (e) {
        // Skip invalid JSON
      }
    }
  }
}
```

## 9. Error Responses

### Error Response Structure

```json
{
  "error": {
    "message": "Invalid API key provided",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

### HTTP Status Codes

- **200**: Success
- **400**: Bad Request - Invalid parameters or malformed request
- **401**: Unauthorized - Invalid or missing API key
- **403**: Forbidden - API key doesn't have required permissions
- **404**: Not Found - Model or endpoint doesn't exist
- **408**: Request Timeout - Request took too long
- **409**: Conflict - Resource conflict (auto-retried)
- **429**: Rate Limit Exceeded - Too many requests
- **500**: Internal Server Error - Server-side error (auto-retried)
- **502**: Bad Gateway - Upstream service error (auto-retried)
- **503**: Service Unavailable - Service temporarily down (auto-retried)

### Common Error Types

#### Invalid API Key (401)

```json
{
  "error": {
    "message": "Invalid authentication credentials",
    "type": "authentication_error",
    "code": "invalid_api_key"
  }
}
```

#### Invalid Request (400)

```json
{
  "error": {
    "message": "Invalid parameter: 'temperature' must be between 0 and 1",
    "type": "invalid_request_error",
    "code": "invalid_parameter",
    "param": "temperature"
  }
}
```

#### Model Not Found (404)

```json
{
  "error": {
    "message": "Model 'invalid-model-name' not found",
    "type": "invalid_request_error",
    "code": "model_not_found",
    "param": "model"
  }
}
```

#### Rate Limit (429)

```json
{
  "error": {
    "message": "Rate limit exceeded. Please retry after 60 seconds.",
    "type": "rate_limit_error",
    "code": "rate_limit_exceeded",
    "retry_after": 60
  }
}
```

#### Context Length Exceeded (400)

```json
{
  "error": {
    "message": "Maximum context length exceeded: 150000 tokens requested, 128000 tokens maximum",
    "type": "invalid_request_error",
    "code": "context_length_exceeded",
    "param": "messages"
  }
}
```

#### Server Error (500)

```json
{
  "error": {
    "message": "Internal server error. The issue has been logged and will be investigated.",
    "type": "server_error",
    "code": "internal_error"
  }
}
```

### Error Handling Best Practices

1. **Automatic Retries**: The SDK automatically retries on:
   - Connection errors
   - 408 Request Timeout
   - 409 Conflict
   - 429 Rate Limit (with exponential backoff)
   - 500+ Server errors

2. **Error Types**:
   - `APIConnectionError`: Network connectivity issues
   - `APIStatusError`: Non-200 status codes
   - `RateLimitError`: 429 status specifically
   - `APIError`: Base error class

3. **Access Error Details**:
   ```python
   try:
       response = client.chat.completions.create(...)
   except APIStatusError as e:
       print(f"Status: {e.status_code}")
       print(f"Response: {e.response}")
       print(f"Message: {e.message}")
   ```

## 10. Available Models

### Model Identifiers

#### Llama 4 Models (April 2025)

```
meta-llama/llama-4-maverick
Llama-4-Maverick-17B-128E-Instruct-FP8
meta-llama/Llama-4-Maverick-17B-128E-Instruct

meta-llama/llama-4-scout
Llama-4-Scout-17B-16E-Instruct
```

#### Llama 3.3 Models

```
meta-llama/llama-3.3-70b-versatile
meta-llama/Llama-3.3-70B-Instruct
```

#### Llama 3.1 Models

```
meta-llama/Meta-Llama-3.1-8B-Instruct
meta-llama/Meta-Llama-3.1-70B-Instruct
meta-llama/Meta-Llama-3.1-405B-Instruct
```

### Model Capabilities

| Model | Context | Multimodal | Strengths |
|-------|---------|------------|-----------|
| Llama 4 Maverick | 256K-1M | Yes (vision) | Best multimodal, coding, reasoning |
| Llama 4 Scout | 10M | Text only | Industry-leading context length |
| Llama 3.3 70B | 131K | Text only | General purpose, versatile |
| Llama 3.1 405B | 128K | Text only | Largest, highest quality |
| Llama 3.1 70B | 128K | Text only | Balanced performance |
| Llama 3.1 8B | 128K | Text only | Fast, efficient |

## 11. Complete Request/Response Examples

### Example 1: Simple Chat

**Request:**

```bash
curl https://api.llama.com/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $LLAMA_API_KEY" \
  -d '{
    "model": "meta-llama/llama-4-maverick",
    "messages": [
      {
        "role": "system",
        "content": "You are a helpful assistant."
      },
      {
        "role": "user",
        "content": "Explain quantum computing in one sentence."
      }
    ],
    "temperature": 0.7,
    "max_completion_tokens": 100
  }'
```

**Response:**

```json
{
  "id": "chatcmpl-qc-abc123",
  "completion_message": {
    "role": "assistant",
    "content": "Quantum computing uses quantum mechanical phenomena like superposition and entanglement to perform calculations that would be impractical for classical computers.",
    "stop_reason": "stop"
  },
  "metrics": [
    {"metric": "prompt_tokens", "value": 28, "unit": "tokens"},
    {"metric": "completion_tokens", "value": 24, "unit": "tokens"},
    {"metric": "total_tokens", "value": 52, "unit": "tokens"}
  ]
}
```

### Example 2: Multi-Turn Conversation

**Request:**

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful coding assistant."
    },
    {
      "role": "user",
      "content": "How do I reverse a string in Python?"
    },
    {
      "role": "assistant",
      "content": "You can reverse a string in Python using slicing: `reversed_string = original_string[::-1]`"
    },
    {
      "role": "user",
      "content": "Can you show a complete example?"
    }
  ]
}
```

**Response:**

```json
{
  "id": "chatcmpl-conv-xyz789",
  "completion_message": {
    "role": "assistant",
    "content": "Sure! Here's a complete example:\n\n```python\noriginal = \"Hello, World!\"\nreversed_string = original[::-1]\nprint(reversed_string)  # Output: !dlroW ,olleH\n```",
    "stop_reason": "stop"
  },
  "metrics": [...]
}
```

### Example 3: Vision (Image Understanding)

**Request:**

```json
{
  "model": "meta-llama/llama-4-maverick",
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "What objects do you see in this image?"
        },
        {
          "type": "image_url",
          "image_url": {
            "url": "https://example.com/kitchen.jpg"
          }
        }
      ]
    }
  ],
  "max_completion_tokens": 500
}
```

**Response:**

```json
{
  "id": "chatcmpl-vision-001",
  "completion_message": {
    "role": "assistant",
    "content": "I can see a modern kitchen with the following objects:\n- Stainless steel refrigerator\n- Gas stove with 4 burners\n- Granite countertop\n- Wooden cabinets\n- Microwave oven\n- Coffee maker\n- Fruit bowl with apples and bananas",
    "stop_reason": "stop"
  },
  "metrics": [...]
}
```

## 12. SDK Usage Examples

### Python

```python
from llama import Llama

client = Llama(api_key="llama_api_abc123")

# Simple completion
response = client.chat.completions.create(
    model="meta-llama/llama-4-maverick",
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "Hello!"}
    ],
    temperature=0.7
)

print(response.completion_message.content)

# Streaming
stream = client.chat.completions.create(
    model="meta-llama/llama-4-maverick",
    messages=[{"role": "user", "content": "Tell me a story"}],
    stream=True
)

for chunk in stream:
    if chunk.event.event_type == "progress":
        if chunk.event.delta.type == "text":
            print(chunk.event.delta.text, end="", flush=True)
```

### TypeScript

```typescript
import { Llama } from 'llama-api';

const client = new Llama({
  apiKey: process.env.LLAMA_API_KEY
});

// Simple completion
const response = await client.chat.completions.create({
  model: 'meta-llama/llama-4-maverick',
  messages: [
    { role: 'system', content: 'You are a helpful assistant.' },
    { role: 'user', content: 'Hello!' }
  ],
  temperature: 0.7
});

console.log(response.completion_message.content);

// Streaming
const stream = await client.chat.completions.create({
  model: 'meta-llama/llama-4-maverick',
  messages: [{ role: 'user', content: 'Tell me a story' }],
  stream: true
});

for await (const chunk of stream) {
  if (chunk.event.event_type === 'progress') {
    if (chunk.event.delta?.text) {
      process.stdout.write(chunk.event.delta.text);
    }
  }
}
```

## 13. OpenAI Compatibility

The Llama API provides OpenAI-compatible endpoints, allowing you to use existing OpenAI client libraries with minimal changes.

### Using OpenAI Python SDK

```python
from openai import OpenAI

client = OpenAI(
    api_key="llama_api_abc123",
    base_url="https://api.llama.com/compat/v1"
)

response = client.chat.completions.create(
    model="meta-llama/llama-4-maverick",
    messages=[
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "Hello!"}
    ]
)

print(response.choices[0].message.content)
```

### Using OpenAI TypeScript SDK

```typescript
import OpenAI from 'openai';

const client = new OpenAI({
  apiKey: process.env.LLAMA_API_KEY,
  baseURL: 'https://api.llama.com/compat/v1'
});

const response = await client.chat.completions.create({
  model: 'meta-llama/llama-4-maverick',
  messages: [
    { role: 'system', content: 'You are a helpful assistant.' },
    { role: 'user', content: 'Hello!' }
  ]
});

console.log(response.choices[0].message.content);
```

## 14. Rate Limits and Quotas

### Preview Period (Current)

- Free access during preview
- Rate limits vary by usage tier
- Limits subject to change

### Production (Future)

Rate limits will be announced when the API exits preview. Expected tiers:
- Free tier: Limited requests per minute
- Paid tier: Higher limits based on subscription
- Enterprise: Custom limits

## 15. Best Practices

### 1. API Key Security

- Never commit API keys to version control
- Use environment variables
- Rotate keys regularly
- Use separate keys for development and production

### 2. Error Handling

- Implement retry logic with exponential backoff
- Log errors for debugging
- Handle rate limits gracefully
- Provide fallback responses

### 3. Performance Optimization

- Use streaming for long responses
- Set appropriate `max_completion_tokens`
- Adjust `temperature` based on use case
- Cache responses when appropriate

### 4. Context Management

- Keep conversation history concise
- Remove old messages to stay within limits
- Use system messages effectively
- Summarize long conversations

### 5. Tool Calling

- Provide clear, concise function descriptions
- Use descriptive parameter names
- Validate tool responses before sending back
- Handle tool errors gracefully

## 16. Additional Resources

### Official Documentation

- API Reference: https://llama.developer.meta.com/docs/api/chat/
- Models Overview: https://llama.developer.meta.com/docs/models/
- API Keys: https://llama.developer.meta.com/docs/api-keys/
- Structured Output: https://llama.developer.meta.com/docs/features/structured-output/
- Tool Calling Guide: https://llama.developer.meta.com/docs/guides/tool-guide/

### SDKs

- Python: https://github.com/meta-llama/llama-api-python
- TypeScript: https://github.com/meta-llama/llama-api-typescript

### Community

- Llama Developer Forum: https://llama.developer.meta.com/community
- GitHub Issues: https://github.com/meta-llama

## Sources

- [Chat completion | Llama API](https://llama.developer.meta.com/docs/api/chat/)
- [Llama API: Convenient access to Llama models | Meta](https://llama.developer.meta.com/docs/overview/)
- [Llama API Models Overview](https://llama.developer.meta.com/docs/models/)
- [JSON Structured Output](https://llama.developer.meta.com/docs/features/structured-output/)
- [Tool calling | Llama API](https://llama.developer.meta.com/docs/guides/tool-guide/)
- [GitHub - meta-llama/llama-api-python](https://github.com/meta-llama/llama-api-python)
- [Llama 3 | Model Cards and Prompt formats](https://www.llama.com/docs/model-cards-and-prompt-formats/meta-llama-3/)
- [Tool Calling - vLLM](https://docs.vllm.ai/en/stable/features/tool_calling/)
- [Structured Outputs - Together.ai Docs](https://docs.together.ai/docs/json-mode)
- [Meta Llama | liteLLM](https://docs.litellm.ai/docs/providers/meta_llama)
- [meta-llama/Meta-Llama-3-70B-Instruct - API Reference - DeepInfra](https://deepinfra.com/meta-llama/Meta-Llama-3-70B-Instruct/api)
- [Function Calling - Llama API - Quickstart](https://docs.llama-api.com/essentials/function)
- [How to Access Meta's Llama 4 Models via API](https://www.analyticsvidhya.com/blog/2025/04/access-llama-4-models-via-api/)
- [The Llama 4 herd - Meta AI Blog](https://ai.meta.com/blog/llama-4-multimodal-intelligence/)
- [Complete LLM API Reference 2025](https://www.token-calculator.com/blog/llm-api-reference-base-urls-model-ids-2025)
- [Meta's Llama API: Open Models, Meet Developer Convenience](https://michaelsolati.com/blog/metas-llama-api-open-models-meet-developer-convenience)
