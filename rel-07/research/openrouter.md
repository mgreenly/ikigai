# OpenRouter API Technical Specification

## Overview

OpenRouter provides unified access to 500+ models from 60+ providers through an OpenAI-compatible API. This specification documents the exact technical implementation details needed to integrate with the OpenRouter API.

**Base URL:** `https://openrouter.ai/api/v1`

**API Version:** v1 (current as of 2025)

---

## 1. API Endpoints

### 1.1 Chat Completions (Primary Endpoint)

**Endpoint:** `POST /api/v1/chat/completions`

**Full URL:** `https://openrouter.ai/api/v1/chat/completions`

**Purpose:** Create chat completions with any supported model. Supports streaming, tool calling, multi-modal inputs, and advanced routing.

### 1.2 List Models

**Endpoint:** `GET /api/v1/models`

**Full URL:** `https://openrouter.ai/api/v1/models`

**Purpose:** Retrieve list of all available models with their properties, pricing, and capabilities.

**Optional Parameters:**
- `category` - Filter by model category
- `supported_parameters` - Filter by parameter support
- `use_rss` - Return RSS feed format
- `use_rss_chat_links` - Include chat links in RSS

### 1.3 List User-Filtered Models

**Endpoint:** `GET /api/v1/models/me`

**Full URL:** `https://openrouter.ai/api/v1/models/me`

**Purpose:** Returns models filtered by user's provider preferences (excludes ignored providers).

### 1.4 Generation Details

**Endpoint:** `GET /api/v1/generation?id={GENERATION_ID}`

**Full URL:** `https://openrouter.ai/api/v1/generation?id={GENERATION_ID}`

**Purpose:** Retrieve detailed generation statistics including native token counts and cost for a specific generation.

### 1.5 List Model Endpoints

**Endpoint:** `GET /api/v1/models/{model_id}/endpoints`

**Purpose:** List all available provider endpoints for a specific model.

---

## 2. Authentication

### 2.1 Authentication Method

OpenRouter uses **Bearer token authentication** via the `Authorization` header.

### 2.2 Required Header

```http
Authorization: Bearer <OPENROUTER_API_KEY>
```

### 2.3 Optional Headers

For app identification and ranking on OpenRouter's site:

```http
HTTP-Referer: <YOUR_SITE_URL>
X-Title: <YOUR_SITE_NAME>
```

### 2.4 Complete Header Example

```http
POST /api/v1/chat/completions HTTP/1.1
Host: openrouter.ai
Authorization: Bearer sk-or-v1-xxxxxxxxxxxxxxxxxxxxx
Content-Type: application/json
HTTP-Referer: https://your-app.com
X-Title: Your App Name
```

### 2.5 API Key Management

- Create API keys at: https://openrouter.ai/settings/keys
- Keys can have optional credit limits
- OpenRouter detects exposed keys and sends email notifications
- Store keys in environment variables, never commit to repositories

---

## 3. Request Format

### 3.1 Complete Request Schema

```typescript
{
  // Model Selection (one of these required)
  model?: string,              // Single model ID (e.g., "openai/gpt-4")
  models?: string[],           // Array for fallback routing

  // Input (one required)
  messages?: Message[],        // Chat format (recommended)
  prompt?: string,             // Legacy text completion format

  // Sampling Parameters
  temperature?: number,        // Range: 0.0-2.0, Default: 1.0
  max_tokens?: number,         // Maximum tokens to generate
  top_p?: number,              // Range: 0.0-1.0, Default: 1.0
  top_k?: number,              // Range: 0+, Default: 0 (disabled)
  min_p?: number,              // Range: 0.0-1.0, Default: 0.0
  top_a?: number,              // Range: 0.0-1.0, Default: 0.0

  // Penalties
  frequency_penalty?: number,  // Range: -2.0 to 2.0, Default: 0.0
  presence_penalty?: number,   // Range: -2.0 to 2.0, Default: 0.0
  repetition_penalty?: number, // Range: 0.0-2.0, Default: 1.0

  // Response Control
  stop?: string | string[],    // Stop sequences
  response_format?: {          // Force output format
    type: 'json_object'
  },
  stream?: boolean,            // Enable SSE streaming, Default: false

  // Tool Calling / Function Calling
  tools?: Tool[],              // Available functions
  tool_choice?: ToolChoice,    // Tool selection strategy
  parallel_tool_calls?: boolean, // Allow simultaneous tool calls, Default: true

  // Advanced Features
  seed?: number,               // For deterministic sampling
  logprobs?: boolean,          // Return log probabilities, Default: false
  top_logprobs?: number,       // Range: 0-20, number of top tokens to return
  logit_bias?: {               // Range: -100 to 100 per token
    [token_id: number]: number
  },

  // Prompt Processing
  transforms?: string[],       // e.g., ["middle-out"] for compression
  prediction?: {               // Speculative execution
    type: 'content',
    content: string
  },

  // Routing & Fallbacks
  route?: 'fallback',          // Routing strategy
  provider?: ProviderPreferences, // Provider selection & ordering

  // Metadata
  user?: string,               // End-user identifier for abuse detection

  // Debugging
  debug?: {
    echo_upstream_body?: boolean // Show transformed request (stream only)
  }
}
```

### 3.2 Message Types

```typescript
type Message =
  // Standard chat messages
  | {
      role: 'user' | 'assistant' | 'system',
      content: string | ContentPart[],
      name?: string
    }
  // Tool response messages
  | {
      role: 'tool',
      content: string,
      tool_call_id: string,
      name?: string
    }

type ContentPart =
  // Text content
  | {
      type: 'text',
      text: string
    }
  // Image content (multi-modal)
  | {
      type: 'image_url',
      image_url: {
        url: string,        // Data URL or HTTP(S) URL
        detail?: string     // 'low' | 'high' | 'auto'
      }
    }
```

### 3.3 Tool / Function Calling Format

```typescript
type Tool = {
  type: 'function',
  function: {
    name: string,           // Function name
    description?: string,   // What the function does
    parameters: object      // JSON Schema for parameters
  }
}

type ToolChoice =
  | 'none'                  // Don't call any tools
  | 'auto'                  // Model decides
  | 'required'              // Must call at least one tool
  | {                       // Force specific tool
      type: 'function',
      function: {
        name: string
      }
    }
```

**Example Tool Definition:**

```json
{
  "type": "function",
  "function": {
    "name": "get_weather",
    "description": "Get the current weather for a location",
    "parameters": {
      "type": "object",
      "properties": {
        "location": {
          "type": "string",
          "description": "City and state, e.g., San Francisco, CA"
        },
        "unit": {
          "type": "string",
          "enum": ["celsius", "fahrenheit"],
          "description": "Temperature unit"
        }
      },
      "required": ["location"]
    }
  }
}
```

### 3.4 Provider Preferences

```typescript
type ProviderPreferences = {
  // Provider Selection
  order?: string[],              // Try providers in this order
  only?: string[],               // Whitelist: only use these providers
  ignore?: string[],             // Blacklist: exclude these providers

  // Routing Strategy
  sort?: 'price' | 'throughput' | 'latency', // Prioritization strategy
  allow_fallbacks?: boolean,     // Enable backup providers, Default: true
  require_parameters?: boolean,  // Only use providers supporting all params

  // Data & Privacy
  data_collection?: 'allow' | 'deny', // Control data retention
  zdr?: boolean,                 // Zero Data Retention only
  enforce_distillable_text?: boolean, // Allow text distillation

  // Performance & Quality
  quantizations?: string[],      // Filter by model optimization (int4, fp8, etc.)

  // Pricing Limits
  max_price?: {
    prompt?: number,             // Max cost per prompt token
    completion?: number,         // Max cost per completion token
    request?: number,            // Max cost per request
    image?: number               // Max cost per image
  }
}
```

**Important:** If `sort` or `order` is set, load balancing is disabled.

### 3.5 Complete Request Example

```json
{
  "model": "anthropic/claude-3.5-sonnet",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant that provides weather information."
    },
    {
      "role": "user",
      "content": "What's the weather in San Francisco?"
    }
  ],
  "temperature": 0.7,
  "max_tokens": 1000,
  "top_p": 0.9,
  "top_k": 40,
  "frequency_penalty": 0.5,
  "presence_penalty": 0.0,
  "stream": false,
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "get_weather",
        "description": "Get current weather for a location",
        "parameters": {
          "type": "object",
          "properties": {
            "location": {
              "type": "string",
              "description": "City name"
            }
          },
          "required": ["location"]
        }
      }
    }
  ],
  "tool_choice": "auto",
  "provider": {
    "order": ["Anthropic", "OpenAI"],
    "allow_fallbacks": true,
    "data_collection": "deny"
  }
}
```

---

## 4. Response Format

### 4.1 Non-Streaming Response Schema

```typescript
{
  id: string,                    // Generation ID (e.g., "gen-xxxxxxxxxxxxx")
  choices: NonStreamingChoice[], // Array of completion choices
  created: number,               // Unix timestamp
  model: string,                 // Actual model used (for billing)
  object: 'chat.completion',     // Response type
  system_fingerprint?: string,   // Provider system fingerprint
  usage?: ResponseUsage          // Token usage statistics
}
```

### 4.2 Choice Object (Non-Streaming)

```typescript
type NonStreamingChoice = {
  finish_reason: string | null,        // 'stop' | 'length' | 'tool_calls' | 'error'
  native_finish_reason: string | null, // Provider's original finish reason
  message: {
    content: string | null,            // Response text
    role: string,                      // Always 'assistant'
    tool_calls?: ToolCall[]            // Function calls (if any)
  },
  error?: ErrorResponse                // Error details (if any)
}

type ToolCall = {
  id: string,                    // Tool call ID
  type: 'function',              // Always 'function'
  function: {
    name: string,                // Function name
    arguments: string            // JSON string of arguments
  }
}
```

### 4.3 Usage Object

```typescript
type ResponseUsage = {
  prompt_tokens: number,       // Input tokens (normalized count via GPT-4o tokenizer)
  completion_tokens: number,   // Output tokens (normalized)
  total_tokens: number         // Sum of prompt + completion
}
```

**Important:** Token counts use normalized, model-agnostic counting (GPT-4o tokenizer), not native model tokenizers. Billing uses native counts. For native counts, enable usage accounting or query the generation endpoint.

### 4.4 Complete Response Example

```json
{
  "id": "gen-1234567890abcdef",
  "choices": [
    {
      "finish_reason": "tool_calls",
      "native_finish_reason": "tool_use",
      "message": {
        "role": "assistant",
        "content": null,
        "tool_calls": [
          {
            "id": "call_abc123",
            "type": "function",
            "function": {
              "name": "get_weather",
              "arguments": "{\"location\": \"San Francisco, CA\"}"
            }
          }
        ]
      }
    }
  ],
  "created": 1702845234,
  "model": "anthropic/claude-3.5-sonnet",
  "object": "chat.completion",
  "usage": {
    "prompt_tokens": 45,
    "completion_tokens": 12,
    "total_tokens": 57
  }
}
```

---

## 5. Streaming (Server-Sent Events)

### 5.1 Enabling Streaming

Set `"stream": true` in the request body.

**Optional:** Include usage statistics in the final chunk:
```json
{
  "stream": true,
  "stream_options": {
    "include_usage": true
  }
}
```

### 5.2 SSE Response Format

**MIME Type:** `text/event-stream`

**Format:** Each event is a line starting with `data:` followed by JSON, terminated by double newlines.

```
data: {"id":"gen-xxx","object":"chat.completion.chunk","created":1702845234,"model":"anthropic/claude-3.5-sonnet","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"gen-xxx","object":"chat.completion.chunk","created":1702845234,"model":"anthropic/claude-3.5-sonnet","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"gen-xxx","object":"chat.completion.chunk","created":1702845234,"model":"anthropic/claude-3.5-sonnet","choices":[{"index":0,"delta":{"content":" there"},"finish_reason":null}]}

data: {"id":"gen-xxx","object":"chat.completion.chunk","created":1702845234,"model":"anthropic/claude-3.5-sonnet","choices":[{"index":0,"delta":{"content":"!"},"finish_reason":"stop"}]}

data: [DONE]
```

### 5.3 Streaming Choice Object

```typescript
type StreamingChoice = {
  index: number,
  finish_reason: string | null,        // Present in final chunk
  native_finish_reason: string | null,
  delta: {
    role?: string,                     // Only in first chunk
    content?: string | null,           // Text delta
    tool_calls?: ToolCall[]            // Function call deltas
  },
  error?: ErrorResponse                // Error (if mid-stream failure)
}
```

### 5.4 SSE Comments

OpenRouter occasionally sends comment lines to prevent connection timeouts:

```
: OPENROUTER PROCESSING
```

**These should be ignored per SSE spec.**

### 5.5 Tool Calls in Streaming

Tool calls are streamed incrementally:

```json
// First chunk with tool call
{
  "delta": {
    "tool_calls": [
      {
        "index": 0,
        "id": "call_abc123",
        "type": "function",
        "function": {
          "name": "get_weather",
          "arguments": ""
        }
      }
    ]
  }
}

// Subsequent chunks add to arguments
{
  "delta": {
    "tool_calls": [
      {
        "index": 0,
        "function": {
          "arguments": "{\"loc"
        }
      }
    ]
  }
}

{
  "delta": {
    "tool_calls": [
      {
        "index": 0,
        "function": {
          "arguments": "ation\": \"San"
        }
      }
    ]
  }
}

// ... more chunks ...

{
  "delta": {
    "tool_calls": [
      {
        "index": 0,
        "function": {
          "arguments": "\"}"
        }
      }
    ]
  },
  "finish_reason": "tool_calls"
}
```

### 5.6 Stream Cancellation

Cancel streaming requests by aborting the HTTP connection.

**Supported Providers (cancellation immediately stops billing):**
- OpenAI
- Anthropic
- Fireworks
- Together
- 20+ others

**Not Supported:**
- Groq
- Google
- Mistral
- Replicate

### 5.7 Error Handling in Streams

**Pre-Stream Errors:** Standard HTTP error response with appropriate status code.

**Mid-Stream Errors:** Error sent as SSE event. HTTP status remains 200 OK.

```json
{
  "error": {
    "code": 502,
    "message": "Provider connection lost"
  },
  "choices": [
    {
      "delta": {
        "content": ""
      },
      "finish_reason": "error"
    }
  ]
}
```

---

## 6. Error Responses

### 6.1 Error Response Structure

```typescript
type ErrorResponse = {
  error: {
    code: number,                  // HTTP status code
    message: string,               // Human-readable error
    metadata?: Record<string, unknown> // Additional context
  }
}
```

**The HTTP response status code matches `error.code`.**

### 6.2 HTTP Status Codes

| Code | Meaning |
|------|---------|
| 400  | Bad Request (invalid/missing params, CORS issues) |
| 401  | Invalid credentials (expired OAuth, invalid API key) |
| 402  | Insufficient credits |
| 403  | Content flagged by moderation |
| 408  | Request timeout |
| 429  | Rate limit exceeded |
| 502  | Model provider unavailable or returned invalid response |
| 503  | No provider available meeting routing requirements |

### 6.3 Error Metadata

**Moderation Errors (403):**
```typescript
{
  error: {
    code: 403,
    message: "Your input was flagged",
    metadata: {
      reasons: string[],           // Violation types
      flagged_input: string,       // Problematic text (max 100 chars)
      provider_name: string,       // Moderation provider
      model_slug: string           // Model that flagged content
    }
  }
}
```

**Provider Errors (502):**
```typescript
{
  error: {
    code: 502,
    message: "Provider error",
    metadata: {
      provider_name: string,       // Provider that failed
      raw: object                  // Original error from provider
    }
  }
}
```

### 6.4 Error Examples

**Invalid API Key:**
```json
{
  "error": {
    "code": 401,
    "message": "Invalid API key"
  }
}
```

**Insufficient Credits:**
```json
{
  "error": {
    "code": 402,
    "message": "Insufficient credits. Please add more credits and retry."
  }
}
```

**Content Moderation:**
```json
{
  "error": {
    "code": 403,
    "message": "Your input was flagged",
    "metadata": {
      "reasons": ["sexual", "violence"],
      "flagged_input": "This is the problematic text...",
      "provider_name": "OpenAI",
      "model_slug": "openai/gpt-4"
    }
  }
}
```

**Provider Unavailable:**
```json
{
  "error": {
    "code": 502,
    "message": "Model provider returned an error",
    "metadata": {
      "provider_name": "Anthropic",
      "raw": {
        "type": "overloaded_error",
        "message": "The model is currently overloaded"
      }
    }
  }
}
```

---

## 7. Model Selection

### 7.1 Model ID Format

**Format:** `provider/model-name`

**Examples:**
```
anthropic/claude-3.5-sonnet
anthropic/claude-opus-4
openai/gpt-4o
openai/gpt-4-turbo
openai/gpt-3.5-turbo
google/gemini-2.0-flash-exp
google/gemini-1.5-pro
meta-llama/llama-3.3-70b-instruct
xai/grok-2-1212
mistralai/mixtral-8x7b-instruct
```

### 7.2 Model Variants (Shortcuts)

**Nitro Variant (`:nitro` suffix):**
- Prioritizes throughput/speed
- Equivalent to `provider.sort = "throughput"`
- Example: `anthropic/claude-3.5-sonnet:nitro`

**Floor Variant (`:floor` suffix):**
- Prioritizes lowest price
- Equivalent to `provider.sort = "price"`
- Example: `openai/gpt-4:floor`

### 7.3 Model Fallbacks

Use the `models` array for automatic failover:

```json
{
  "models": [
    "anthropic/claude-3.5-sonnet",
    "openai/gpt-4",
    "anthropic/claude-3-haiku"
  ],
  "messages": [...]
}
```

**Fallback triggers:**
- Context length validation errors
- Moderation flags
- Rate limiting
- Provider downtime
- Any 5xx response

**Billing:** The `model` field in the response indicates which model was actually used.

---

## 8. Provider Routing

### 8.1 Default Routing Strategy

OpenRouter's default load balancing:
1. Prioritizes providers without significant outages in last 30 seconds
2. Selects from lowest-cost candidates
3. Weighted by inverse square of price

### 8.2 Provider Ordering

```json
{
  "provider": {
    "order": ["Anthropic", "OpenAI", "Together"]
  }
}
```

Tries providers in the specified order. Falls back to others if `allow_fallbacks: true` (default).

### 8.3 Provider Filtering

**Whitelist (only):**
```json
{
  "provider": {
    "only": ["Anthropic", "OpenAI"]
  }
}
```

**Blacklist (ignore):**
```json
{
  "provider": {
    "ignore": ["Groq", "DeepInfra"]
  }
}
```

### 8.4 Sort Strategies

```json
{
  "provider": {
    "sort": "price"  // or "throughput" or "latency"
  }
}
```

- `price`: Always select cheapest provider
- `throughput`: Prioritize high-speed inference
- `latency`: Minimize response time

**Note:** Setting `sort` or `order` disables load balancing.

### 8.5 Disable Fallbacks

```json
{
  "provider": {
    "order": ["Anthropic"],
    "allow_fallbacks": false
  }
}
```

Only uses specified providers. Fails if none available.

### 8.6 Require Parameter Support

```json
{
  "provider": {
    "require_parameters": true
  },
  "response_format": {
    "type": "json_object"
  }
}
```

Only routes to providers supporting all request parameters (e.g., JSON mode).

### 8.7 Data Collection Control

```json
{
  "provider": {
    "data_collection": "deny"  // or "allow"
  }
}
```

- `deny`: Excludes providers that may store user data
- `allow`: All providers permitted (default)

### 8.8 Zero Data Retention

```json
{
  "provider": {
    "zdr": true
  }
}
```

Only routes to providers with Zero Data Retention guarantees.

### 8.9 Price Limits

```json
{
  "provider": {
    "max_price": {
      "prompt": 0.000001,      // $0.001 per 1k tokens
      "completion": 0.000003,  // $0.003 per 1k tokens
      "request": 0.01,         // $0.01 per request
      "image": 0.001           // $0.001 per image
    }
  }
}
```

Excludes providers exceeding price thresholds.

### 8.10 Quantization Filtering

```json
{
  "provider": {
    "quantizations": ["fp8", "int4"]
  }
}
```

Filter by model optimization level.

---

## 9. Unique OpenRouter Features

### 9.1 Transforms (Prompt Compression)

```json
{
  "transforms": ["middle-out"],
  "messages": [...]
}
```

**`middle-out` transform:**
- Compresses prompts exceeding context size
- Removes/compresses messages from middle of conversation
- LLMs pay less attention to middle anyway
- Enables longer conversations

**Disable transforms:**
```json
{
  "transforms": []
}
```

### 9.2 BYOK (Bring Your Own Key)

Use your own API keys from providers while leveraging OpenRouter's unified interface, analytics, and failover.

**How to configure:** Add provider API keys in the OpenRouter dashboard at https://openrouter.ai/settings/keys

**Supported Providers:**
- OpenAI
- Anthropic
- Google Vertex AI (requires service account JSON)
- AWS Bedrock (requires access key, secret, region)
- Azure AI Services (requires API key)

**Example Vertex AI configuration:**
```json
{
  "type": "service_account",
  "project_id": "your-project-id",
  "private_key_id": "...",
  "private_key": "...",
  "client_email": "...",
  "region": "us-central1"
}
```

**Example AWS Bedrock configuration:**
```json
{
  "accessKeyId": "AKIA...",
  "secretAccessKey": "...",
  "region": "us-east-1"
}
```

**Pricing:**
- First 1M BYOK requests/month: FREE
- Beyond 1M requests: 5% of normal OpenRouter cost
- Billed by upstream provider, not OpenRouter

**Behavior:**
- OpenRouter prioritizes your keys when available
- Default: Falls back to OpenRouter credits on rate limits/errors
- Option: "Always use this key" prevents fallback

### 9.3 Usage Accounting

Enable detailed native token counts:

```json
{
  "user": "your-user-id"  // Required for usage accounting
}
```

**What it provides:**
- Native token counts (from model's actual tokenizer)
- Detailed cost breakdown
- Cached token information

**Trade-off:** Adds ~200-300ms latency to final response.

**Access via:** Generation details endpoint (`GET /api/v1/generation?id={ID}`)

### 9.4 Debugging

```json
{
  "stream": true,
  "debug": {
    "echo_upstream_body": true
  }
}
```

**Returns:** First streaming chunk contains transformed request details showing:
- Parameter mappings
- Message formatting
- Applied defaults
- Provider-specific transformations

**Important:** Streaming only. Development use only, not production-safe.

### 9.5 Speculative Execution (Prediction)

```json
{
  "prediction": {
    "type": "content",
    "content": "Expected response prefix"
  }
}
```

Speeds up inference when you can predict response start.

---

## 10. Rate Limits

### 10.1 OpenRouter Platform Limits

**Pay-as-you-go / Enterprise users:** No platform-level rate limits

**Free tier:**
- 50 requests/day total (no credits purchased)
- 1000 requests/day (purchased at least $10 credits)

### 10.2 BYOK Rate Limits

Rate limits determined by your API key with the upstream provider (OpenAI, Anthropic, etc.), not OpenRouter.

### 10.3 Provider Rate Limits

Each provider has its own rate limits. OpenRouter automatically:
- Detects rate limit responses (429)
- Triggers fallback to next provider (if enabled)
- Returns 429 error if all providers exhausted

---

## 11. System Messages

### 11.1 Placement

System messages go in the `messages` array with `role: "system"`:

```json
{
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant specialized in weather forecasting."
    },
    {
      "role": "user",
      "content": "What will the weather be like tomorrow?"
    }
  ]
}
```

### 11.2 Multiple System Messages

Some models support multiple system messages:

```json
{
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful assistant."
    },
    {
      "role": "user",
      "content": "Hello"
    },
    {
      "role": "assistant",
      "content": "Hi there!"
    },
    {
      "role": "system",
      "content": "Now switch to speaking in French."
    },
    {
      "role": "user",
      "content": "How are you?"
    }
  ]
}
```

**Provider compatibility:** OpenRouter normalizes system message handling across providers. Check model documentation for specific support.

---

## 12. Multi-Modal Support

### 12.1 Image Inputs

```json
{
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

### 12.2 Image URL Formats

**HTTP/HTTPS URLs:**
```json
{
  "image_url": {
    "url": "https://example.com/image.png"
  }
}
```

**Data URLs (base64):**
```json
{
  "image_url": {
    "url": "data:image/jpeg;base64,/9j/4AAQSkZJRg..."
  }
}
```

### 12.3 Image Detail Level

```json
{
  "image_url": {
    "url": "...",
    "detail": "high"  // "low" | "high" | "auto"
  }
}
```

- `low`: Faster, cheaper, less detail
- `high`: Slower, more expensive, more detail
- `auto`: Model decides (default)

---

## 13. Advanced Examples

### 13.1 Complete Multi-Modal Request

```json
{
  "model": "anthropic/claude-3.5-sonnet",
  "messages": [
    {
      "role": "system",
      "content": "You are a helpful image analysis assistant."
    },
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "Describe this image in detail and identify any objects."
        },
        {
          "type": "image_url",
          "image_url": {
            "url": "https://example.com/photo.jpg",
            "detail": "high"
          }
        }
      ]
    }
  ],
  "temperature": 0.7,
  "max_tokens": 500
}
```

### 13.2 Tool Calling with Fallbacks

```json
{
  "models": [
    "anthropic/claude-3.5-sonnet",
    "openai/gpt-4-turbo",
    "google/gemini-1.5-pro"
  ],
  "messages": [
    {
      "role": "user",
      "content": "What's the weather in Tokyo?"
    }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "get_weather",
        "description": "Get current weather for a city",
        "parameters": {
          "type": "object",
          "properties": {
            "city": {
              "type": "string",
              "description": "City name"
            },
            "units": {
              "type": "string",
              "enum": ["celsius", "fahrenheit"],
              "default": "celsius"
            }
          },
          "required": ["city"]
        }
      }
    }
  ],
  "tool_choice": "auto",
  "provider": {
    "require_parameters": true,
    "data_collection": "deny"
  }
}
```

### 13.3 Constrained Routing with JSON Output

```json
{
  "model": "openai/gpt-4o",
  "messages": [
    {
      "role": "user",
      "content": "Extract the name, age, and city from: John is 30 years old and lives in Paris."
    }
  ],
  "response_format": {
    "type": "json_object"
  },
  "provider": {
    "order": ["OpenAI", "Anthropic"],
    "require_parameters": true,
    "allow_fallbacks": false,
    "data_collection": "deny",
    "max_price": {
      "prompt": 0.000005,
      "completion": 0.000015
    }
  }
}
```

### 13.4 Streaming with Tool Calls

```json
{
  "model": "anthropic/claude-3.5-sonnet",
  "stream": true,
  "stream_options": {
    "include_usage": true
  },
  "messages": [
    {
      "role": "user",
      "content": "Search for recent papers on quantum computing and summarize the top 3."
    }
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "search_papers",
        "description": "Search academic papers",
        "parameters": {
          "type": "object",
          "properties": {
            "query": {
              "type": "string"
            },
            "limit": {
              "type": "integer",
              "default": 10
            }
          },
          "required": ["query"]
        }
      }
    }
  ],
  "tool_choice": "auto",
  "temperature": 0.3
}
```

---

## 14. SDK Integration Examples

### 14.1 Using OpenAI Python SDK

```python
import os
from openai import OpenAI

client = OpenAI(
    base_url="https://openrouter.ai/api/v1",
    api_key=os.environ["OPENROUTER_API_KEY"],
    default_headers={
        "HTTP-Referer": "https://your-app.com",
        "X-Title": "Your App Name"
    }
)

# Standard request
response = client.chat.completions.create(
    model="anthropic/claude-3.5-sonnet",
    messages=[
        {"role": "user", "content": "Hello!"}
    ]
)

# With fallbacks (using extra_body)
response = client.chat.completions.create(
    model="anthropic/claude-3.5-sonnet",
    messages=[
        {"role": "user", "content": "Hello!"}
    ],
    extra_body={
        "models": [
            "anthropic/claude-3.5-sonnet",
            "openai/gpt-4-turbo"
        ],
        "provider": {
            "data_collection": "deny"
        }
    }
)
```

### 14.2 Using OpenAI TypeScript/JavaScript SDK

```typescript
import OpenAI from "openai";

const openai = new OpenAI({
  baseURL: "https://openrouter.ai/api/v1",
  apiKey: process.env.OPENROUTER_API_KEY,
  defaultHeaders: {
    "HTTP-Referer": "https://your-app.com",
    "X-Title": "Your App Name"
  }
});

// Standard request
const completion = await openai.chat.completions.create({
  model: "anthropic/claude-3.5-sonnet",
  messages: [
    { role: "user", content: "Hello!" }
  ]
});

// With provider preferences
const completion = await openai.chat.completions.create({
  model: "openai/gpt-4",
  messages: [
    { role: "user", content: "Hello!" }
  ],
  provider: {
    order: ["OpenAI", "Together"],
    data_collection: "deny"
  }
});
```

### 14.3 Raw cURL Request

```bash
curl https://openrouter.ai/api/v1/chat/completions \
  -H "Authorization: Bearer $OPENROUTER_API_KEY" \
  -H "Content-Type: application/json" \
  -H "HTTP-Referer: https://your-app.com" \
  -H "X-Title: Your App Name" \
  -d '{
    "model": "anthropic/claude-3.5-sonnet",
    "messages": [
      {
        "role": "user",
        "content": "Hello!"
      }
    ],
    "temperature": 0.7,
    "max_tokens": 100
  }'
```

### 14.4 Streaming with cURL

```bash
curl https://openrouter.ai/api/v1/chat/completions \
  -H "Authorization: Bearer $OPENROUTER_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "anthropic/claude-3.5-sonnet",
    "messages": [
      {"role": "user", "content": "Write a haiku about coding"}
    ],
    "stream": true
  }' \
  --no-buffer
```

---

## 15. Best Practices

### 15.1 Error Handling

Always implement retry logic with exponential backoff:

```typescript
async function callOpenRouter(request, maxRetries = 3) {
  for (let i = 0; i < maxRetries; i++) {
    try {
      const response = await fetch("https://openrouter.ai/api/v1/chat/completions", {
        method: "POST",
        headers: {
          "Authorization": `Bearer ${API_KEY}`,
          "Content-Type": "application/json"
        },
        body: JSON.stringify(request)
      });

      if (response.ok) {
        return await response.json();
      }

      const error = await response.json();

      // Don't retry client errors (400, 401, 403)
      if (response.status >= 400 && response.status < 500) {
        throw new Error(error.error.message);
      }

      // Retry server errors (500+) and rate limits (429)
      if (i < maxRetries - 1) {
        await sleep(Math.pow(2, i) * 1000);
        continue;
      }

      throw new Error(error.error.message);
    } catch (e) {
      if (i === maxRetries - 1) throw e;
      await sleep(Math.pow(2, i) * 1000);
    }
  }
}
```

### 15.2 Fallback Configuration

Use model fallbacks for production resilience:

```json
{
  "models": [
    "anthropic/claude-3.5-sonnet",
    "openai/gpt-4-turbo",
    "google/gemini-1.5-pro",
    "anthropic/claude-3-haiku"
  ],
  "provider": {
    "allow_fallbacks": true
  }
}
```

### 15.3 Cost Optimization

```json
{
  "model": "anthropic/claude-3.5-sonnet:floor",
  "provider": {
    "sort": "price",
    "max_price": {
      "prompt": 0.000003,
      "completion": 0.000015
    }
  }
}
```

### 15.4 Privacy-First Routing

```json
{
  "provider": {
    "data_collection": "deny",
    "zdr": true
  }
}
```

### 15.5 Performance Optimization

```json
{
  "model": "anthropic/claude-3.5-sonnet:nitro",
  "provider": {
    "sort": "throughput"
  },
  "transforms": ["middle-out"],
  "stream": true
}
```

---

## 16. Reference Links

**Official Documentation:**
- API Reference: https://openrouter.ai/docs/api/reference/overview
- Authentication: https://openrouter.ai/docs/api/reference/authentication
- Parameters: https://openrouter.ai/docs/api/reference/parameters
- Streaming: https://openrouter.ai/docs/api/reference/streaming
- Error Handling: https://openrouter.ai/docs/api/reference/errors-and-debugging
- Quickstart: https://openrouter.ai/docs/quickstart

**Features:**
- Provider Routing: https://openrouter.ai/docs/guides/routing/provider-selection
- Model Fallbacks: https://openrouter.ai/docs/guides/routing/model-fallbacks
- BYOK: https://openrouter.ai/docs/use-cases/byok
- Nitro/Floor Routing: https://openrouter.ai/announcements/introducing-nitro-and-floor-price-shortcuts

**Models & Pricing:**
- Model List: https://openrouter.ai/models
- Pricing: https://openrouter.ai/pricing

**Account Management:**
- API Keys: https://openrouter.ai/settings/keys
- Usage Dashboard: https://openrouter.ai/activity

---

## 17. TypeScript Type Definitions

```typescript
// Complete type definitions for OpenRouter API

interface OpenRouterRequest {
  // Model selection
  model?: string;
  models?: string[];

  // Input
  messages?: Message[];
  prompt?: string;

  // Sampling
  temperature?: number;
  max_tokens?: number;
  top_p?: number;
  top_k?: number;
  min_p?: number;
  top_a?: number;

  // Penalties
  frequency_penalty?: number;
  presence_penalty?: number;
  repetition_penalty?: number;

  // Response control
  stop?: string | string[];
  response_format?: { type: 'json_object' };
  stream?: boolean;
  stream_options?: { include_usage?: boolean };

  // Tools
  tools?: Tool[];
  tool_choice?: ToolChoice;
  parallel_tool_calls?: boolean;

  // Advanced
  seed?: number;
  logprobs?: boolean;
  top_logprobs?: number;
  logit_bias?: { [token: number]: number };
  transforms?: string[];
  prediction?: { type: 'content'; content: string };

  // Routing
  route?: 'fallback';
  provider?: ProviderPreferences;

  // Metadata
  user?: string;
  debug?: { echo_upstream_body?: boolean };
}

type Message =
  | { role: 'user' | 'assistant' | 'system'; content: string | ContentPart[]; name?: string }
  | { role: 'tool'; content: string; tool_call_id: string; name?: string };

type ContentPart =
  | { type: 'text'; text: string }
  | { type: 'image_url'; image_url: { url: string; detail?: 'low' | 'high' | 'auto' } };

interface Tool {
  type: 'function';
  function: {
    name: string;
    description?: string;
    parameters: object;
  };
}

type ToolChoice =
  | 'none'
  | 'auto'
  | 'required'
  | { type: 'function'; function: { name: string } };

interface ProviderPreferences {
  order?: string[];
  only?: string[];
  ignore?: string[];
  sort?: 'price' | 'throughput' | 'latency';
  allow_fallbacks?: boolean;
  require_parameters?: boolean;
  data_collection?: 'allow' | 'deny';
  zdr?: boolean;
  enforce_distillable_text?: boolean;
  quantizations?: string[];
  max_price?: {
    prompt?: number;
    completion?: number;
    request?: number;
    image?: number;
  };
}

interface OpenRouterResponse {
  id: string;
  choices: Choice[];
  created: number;
  model: string;
  object: 'chat.completion' | 'chat.completion.chunk';
  system_fingerprint?: string;
  usage?: {
    prompt_tokens: number;
    completion_tokens: number;
    total_tokens: number;
  };
}

type Choice =
  | {
      finish_reason: string | null;
      native_finish_reason: string | null;
      message: {
        content: string | null;
        role: string;
        tool_calls?: ToolCall[];
      };
      error?: ErrorResponse;
    }
  | {
      index: number;
      finish_reason: string | null;
      native_finish_reason: string | null;
      delta: {
        role?: string;
        content?: string | null;
        tool_calls?: ToolCall[];
      };
      error?: ErrorResponse;
    };

interface ToolCall {
  id: string;
  type: 'function';
  function: {
    name: string;
    arguments: string;
  };
}

interface ErrorResponse {
  error: {
    code: number;
    message: string;
    metadata?: Record<string, unknown>;
  };
}
```

---

**Document Version:** 1.0
**Last Updated:** 2025-12-14
**API Version:** v1
