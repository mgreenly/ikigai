# Google Gemini API Technical Specification (2025)

## Available Models

### Gemini 3 Series
- **gemini-3-pro** (Preview) - Latest reasoning-first model, optimized for complex agentic workflows and coding
  - Adaptive thinking
  - 1M token context window
  - Integrated grounding
  - Requires pay-as-you-go Blaze pricing plan

- **gemini-3-flash** - Fast, efficient model balancing speed and capability
  - 1M token context window
  - Thinking mode support (thinkingLevel: LOW/HIGH)
  - Optimized for interactive use cases

- **gemini-3-pro-image** (Preview) - High-fidelity image generation with reasoning-enhanced composition
  - Legible text rendering
  - Complex multi-turn editing
  - Character consistency (up to 14 reference inputs)

### Gemini 2.5 Series
- **gemini-2.5-pro** - Stable production version, 2M token context window
- **gemini-2.5-flash** - Best price-performance, 1M token context window, optimized for large scale processing
- **gemini-2.5-flash-lite** - Optimized for cost efficiency and low latency

### Gemini 2.0 Series
- **gemini-2.0-flash** - Next-gen features, superior speed, native tool use, 1M token context window
- **gemini-2.0-flash-lite** - Optimized for cost efficiency and low latency
- **gemini-2.5-flash-live** - Native audio models supporting Live API

### Embeddings
- **gemini-embedding-001** - Generally available, 3072 dimensions, 100+ languages, 8K token input limit

---

## 1. API Endpoints

### Native API (Google AI Studio)

#### Base URL
```
https://generativelanguage.googleapis.com/v1beta/
```

#### Generate Content (Non-Streaming)
```
POST https://generativelanguage.googleapis.com/v1beta/{model=models/*}:generateContent
```

**Path Parameter:**
- `model` (string, required) - Format: `models/{model}` (e.g., `models/gemini-2.5-flash`)

**Query Parameter:**
- `key` (string, required) - Your API key

**Example:**
```
POST https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=YOUR_API_KEY
```

#### Stream Generate Content
```
POST https://generativelanguage.googleapis.com/v1beta/{model=models/*}:streamGenerateContent
```

**Query Parameters:**
- `key` (string, required) - Your API key
- `alt=sse` (string) - Enables Server-Sent Events streaming

**Example:**
```
POST https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:streamGenerateContent?key=YOUR_API_KEY&alt=sse
```

### OpenAI-Compatible API

#### Base URL
```
https://generativelanguage.googleapis.com/v1beta/openai/
```

**Standard OpenAI Endpoints:**
- `/chat/completions` - Chat completions
- `/embeddings` - Text embeddings

**Full Example:**
```
POST https://generativelanguage.googleapis.com/v1beta/openai/chat/completions
```

### Vertex AI API

#### Base URL Format
```
https://{REGION}-aiplatform.googleapis.com/v1/projects/{PROJECT_ID}/locations/{REGION}/publishers/google/models/{MODEL_ID}:{METHOD}
```

**Example:**
```
POST https://us-central1-aiplatform.googleapis.com/v1/projects/my-project/locations/us-central1/publishers/google/models/gemini-2.5-flash:generateContent
```

---

## 2. Authentication

### Native API (Google AI Studio)

#### API Key via Query Parameter
```
?key=YOUR_API_KEY
```

#### Bearer Token (Alternative)
```
Authorization: Bearer YOUR_API_KEY
```

### OpenAI-Compatible API

**Required Header:**
```
Authorization: Bearer YOUR_API_KEY
```

### Vertex AI

**Required Header:**
```
Authorization: Bearer $(gcloud auth print-access-token)
```

**Required Scope:**
```
https://www.googleapis.com/auth/cloud-platform
```

---

## 3. Request Format

### Required Headers

**Native API:**
```
Content-Type: application/json
```

**Streaming:**
```
Content-Type: application/json
Accept: text/event-stream
```

### Request Body Schema (Native API)

```json
{
  "contents": [
    {
      "role": "user|model",
      "parts": [
        {
          "text": "string"
        },
        {
          "inline_data": {
            "mime_type": "image/jpeg|image/png|audio/mpeg|video/mp4|application/pdf",
            "data": "base64_encoded_bytes"
          }
        },
        {
          "file_data": {
            "mime_type": "string",
            "file_uri": "string"
          }
        }
      ]
    }
  ],
  "systemInstruction": {
    "role": "user",
    "parts": [
      {
        "text": "string"
      }
    ]
  },
  "tools": [
    {
      "functionDeclarations": [
        {
          "name": "string",
          "description": "string",
          "parameters": {
            "type": "object",
            "properties": {
              "param_name": {
                "type": "string|number|boolean|array|object",
                "description": "string",
                "enum": ["value1", "value2"]
              }
            },
            "required": ["param_name"]
          }
        }
      ]
    }
  ],
  "toolConfig": {
    "functionCallingConfig": {
      "mode": "AUTO|ANY|NONE",
      "allowedFunctionNames": ["string"]
    }
  },
  "safetySettings": [
    {
      "category": "HARM_CATEGORY_HATE_SPEECH|HARM_CATEGORY_SEXUALLY_EXPLICIT|HARM_CATEGORY_DANGEROUS_CONTENT|HARM_CATEGORY_HARASSMENT|HARM_CATEGORY_CIVIC_INTEGRITY",
      "threshold": "BLOCK_LOW_AND_ABOVE|BLOCK_MEDIUM_AND_ABOVE|BLOCK_ONLY_HIGH|BLOCK_NONE|OFF"
    }
  ],
  "generationConfig": {
    "stopSequences": ["string"],
    "responseMimeType": "text/plain|application/json|text/x.enum",
    "responseSchema": {
      "type": "object",
      "properties": {},
      "required": []
    },
    "responseModalities": ["TEXT|IMAGE|AUDIO"],
    "candidateCount": 1,
    "maxOutputTokens": 8192,
    "temperature": 1.0,
    "topP": 0.95,
    "topK": 40,
    "seed": 0,
    "presencePenalty": 0.0,
    "frequencyPenalty": 0.0,
    "responseLogprobs": false,
    "logprobs": 0,
    "speechConfig": {
      "voiceConfig": {
        "prebuiltVoiceConfig": {
          "voiceName": "string"
        }
      },
      "languageCode": "string"
    },
    "thinkingConfig": {
      "includeThoughts": true,
      "thinkingBudget": -1,
      "thinkingLevel": "LOW|HIGH"
    },
    "imageConfig": {
      "aspectRatio": "1:1|16:9|9:16|3:4|4:3|2:3|3:2",
      "imageSize": "1K|2K|4K"
    }
  },
  "cachedContent": "cachedContents/{cachedContent}"
}
```

### Basic Request Example

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [
        {
          "text": "Explain how AI works in a few words"
        }
      ]
    }
  ]
}
```

### Request with System Instruction

```json
{
  "systemInstruction": {
    "parts": [
      {
        "text": "You are a helpful language translator. Your mission is to translate text in English to French."
      }
    ]
  },
  "contents": [
    {
      "role": "user",
      "parts": [
        {
          "text": "Hello, how are you?"
        }
      ]
    }
  ]
}
```

---

## 4. Response Format

### Response Body Schema

```json
{
  "candidates": [
    {
      "content": {
        "role": "model",
        "parts": [
          {
            "text": "string"
          },
          {
            "thought": true,
            "text": "thought summary text"
          }
        ]
      },
      "finishReason": "STOP|MAX_TOKENS|SAFETY|RECITATION|LANGUAGE|OTHER|BLOCKLIST|PROHIBITED_CONTENT|SPII|MALFORMED_FUNCTION_CALL|IMAGE_SAFETY|IMAGE_PROHIBITED_CONTENT|NO_IMAGE|UNEXPECTED_TOOL_CALL",
      "safetyRatings": [
        {
          "category": "HARM_CATEGORY_HATE_SPEECH|HARM_CATEGORY_SEXUALLY_EXPLICIT|HARM_CATEGORY_DANGEROUS_CONTENT|HARM_CATEGORY_HARASSMENT",
          "probability": "NEGLIGIBLE|LOW|MEDIUM|HIGH",
          "blocked": false
        }
      ],
      "citationMetadata": {
        "citationSources": [
          {
            "startIndex": 0,
            "endIndex": 0,
            "uri": "string",
            "license": "string"
          }
        ]
      },
      "tokenCount": 0,
      "groundingAttributions": [],
      "groundingMetadata": {
        "groundingChunks": [],
        "groundingSupports": [],
        "webSearchQueries": ["string"],
        "retrievalMetadata": {
          "googleSearchDynamicRetrievalScore": 0.0
        }
      },
      "logprobsResult": {
        "topCandidates": [],
        "chosenCandidates": [],
        "logProbabilitySum": 0.0
      },
      "index": 0,
      "finishMessage": "string"
    }
  ],
  "promptFeedback": {
    "blockReason": "SAFETY|OTHER|BLOCKLIST|PROHIBITED_CONTENT|IMAGE_SAFETY",
    "safetyRatings": []
  },
  "usageMetadata": {
    "promptTokenCount": 0,
    "cachedContentTokenCount": 0,
    "candidatesTokenCount": 0,
    "totalTokenCount": 0,
    "toolUsePromptTokenCount": 0,
    "thoughtsTokenCount": 0
  },
  "modelVersion": "string",
  "responseId": "string"
}
```

### Basic Response Example

```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {
            "text": "AI works by using algorithms and large amounts of data to learn patterns and make predictions or decisions."
          }
        ],
        "role": "model"
      },
      "finishReason": "STOP",
      "index": 0,
      "safetyRatings": [
        {
          "category": "HARM_CATEGORY_SEXUALLY_EXPLICIT",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_HATE_SPEECH",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_HARASSMENT",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_DANGEROUS_CONTENT",
          "probability": "NEGLIGIBLE"
        }
      ]
    }
  ],
  "usageMetadata": {
    "promptTokenCount": 10,
    "candidatesTokenCount": 25,
    "totalTokenCount": 35
  },
  "modelVersion": "gemini-2.5-flash"
}
```

---

## 5. Chat Completions

### Multi-Turn Conversation Request

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "Hello! What's the weather like?"}]
    },
    {
      "role": "model",
      "parts": [{"text": "I don't have access to real-time weather data. Can you provide your location?"}]
    },
    {
      "role": "user",
      "parts": [{"text": "I'm in San Francisco."}]
    }
  ]
}
```

### Multimodal Request (Text + Image)

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [
        {
          "text": "What's in this image?"
        },
        {
          "inline_data": {
            "mime_type": "image/jpeg",
            "data": "base64_encoded_image_data_here"
          }
        }
      ]
    }
  ]
}
```

### JSON Mode Request

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "List three popular programming languages"}]
    }
  ],
  "generationConfig": {
    "responseMimeType": "application/json",
    "responseSchema": {
      "type": "object",
      "properties": {
        "languages": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "name": {"type": "string"},
              "year": {"type": "number"}
            },
            "required": ["name", "year"]
          }
        }
      },
      "required": ["languages"]
    }
  }
}
```

---

## 6. Function Calling

### Function Declaration Format

Functions use OpenAPI schema format with these supported attributes:
- `type`, `nullable`, `required`, `format`, `description`, `properties`, `items`, `enum`

**Not supported:** `default`, `optional`, `maximum`, `oneOf`

### Request with Function Declarations

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "Which theaters in Mountain View show Barbie movie?"}]
    }
  ],
  "tools": [
    {
      "functionDeclarations": [
        {
          "name": "find_theaters",
          "description": "Find theaters based on location and optionally movie title",
          "parameters": {
            "type": "object",
            "properties": {
              "location": {
                "type": "string",
                "description": "The city and state, e.g. San Francisco, CA"
              },
              "movie": {
                "type": "string",
                "description": "Any movie title"
              }
            },
            "required": ["location"]
          }
        },
        {
          "name": "get_showtimes",
          "description": "Get movie showtimes for a specific theater",
          "parameters": {
            "type": "object",
            "properties": {
              "theater_name": {
                "type": "string",
                "description": "Name of the theater"
              },
              "date": {
                "type": "string",
                "description": "Date in YYYY-MM-DD format"
              }
            },
            "required": ["theater_name", "date"]
          }
        }
      ]
    }
  ],
  "toolConfig": {
    "functionCallingConfig": {
      "mode": "AUTO"
    }
  }
}
```

### Function Call in Response

```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {
            "functionCall": {
              "name": "find_theaters",
              "args": {
                "location": "Mountain View, CA",
                "movie": "Barbie"
              }
            }
          }
        ],
        "role": "model"
      },
      "finishReason": "STOP",
      "index": 0
    }
  ]
}
```

### Sending Function Response Back

After executing the function, return results in the next request:

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "Which theaters in Mountain View show Barbie movie?"}]
    },
    {
      "role": "model",
      "parts": [
        {
          "functionCall": {
            "name": "find_theaters",
            "args": {
              "location": "Mountain View, CA",
              "movie": "Barbie"
            }
          }
        }
      ]
    },
    {
      "role": "function",
      "parts": [
        {
          "functionResponse": {
            "name": "find_theaters",
            "response": {
              "theaters": [
                {
                  "name": "AMC Mountain View 16",
                  "address": "2000 W El Camino Real, Mountain View, CA 94040"
                },
                {
                  "name": "Century 16 Mountain View",
                  "address": "1500 N Shoreline Blvd, Mountain View, CA 94043"
                }
              ]
            }
          }
        }
      ]
    }
  ],
  "tools": [
    {
      "functionDeclarations": [
        {
          "name": "find_theaters",
          "description": "Find theaters based on location and optionally movie title",
          "parameters": {
            "type": "object",
            "properties": {
              "location": {"type": "string"},
              "movie": {"type": "string"}
            },
            "required": ["location"]
          }
        }
      ]
    }
  ]
}
```

### Function Calling Modes

**toolConfig.functionCallingConfig.mode:**
- `AUTO` - Model decides whether to call functions
- `ANY` - Model must call at least one function
- `NONE` - Model cannot call functions

### Forcing Specific Function

```json
{
  "toolConfig": {
    "functionCallingConfig": {
      "mode": "ANY",
      "allowedFunctionNames": ["find_theaters"]
    }
  }
}
```

---

## 7. System Instructions

### systemInstruction Parameter

System instructions are separate from the conversation `contents` array and guide overall model behavior.

**Format:**
```json
{
  "systemInstruction": {
    "parts": [
      {
        "text": "You are a cat. Your name is Neko. You respond to all questions as a cat would, with curiosity and playfulness."
      }
    ]
  }
}
```

### Multi-Part System Instructions

```json
{
  "systemInstruction": {
    "parts": [
      {
        "text": "You are a helpful language translator."
      },
      {
        "text": "Your mission is to translate text in English to French."
      }
    ]
  }
}
```

### Best Practices
- Place behavioral constraints and role definitions in system instruction OR at the very top of the prompt
- Critical for anchoring the model's reasoning process
- Use for: persona definitions, output format requirements, behavioral guidelines

---

## 8. Thinking Mode

### Overview
- Available in Gemini 2.5 and 3 series
- Internal thinking process improves reasoning before response generation
- Thoughts are synthesized summaries of raw internal reasoning

### Parameters by Model Series

#### Gemini 2.5 Series: thinkingBudget

**Parameter:** `thinkingConfig.thinkingBudget`

**Values by Model:**
- **gemini-2.5-pro**: 128-32,768 tokens (cannot disable)
- **gemini-2.5-flash**: 0-24,576 tokens
- **gemini-2.5-flash-lite**: 512-24,576 tokens

**Special Values:**
- `0` - Disable thinking
- `-1` - Enable dynamic thinking (model adjusts based on complexity)

**Example:**
```json
{
  "generationConfig": {
    "thinkingConfig": {
      "thinkingBudget": -1,
      "includeThoughts": true
    }
  }
}
```

#### Gemini 3 Series: thinkingLevel

**Parameter:** `thinkingConfig.thinkingLevel`

**Values:**
- `"LOW"` - Minimizes latency and cost, best for simple instruction following or chat
- `"HIGH"` - Maximizes reasoning depth (default for Gemini 3 Pro), best for complex reasoning

**Example:**
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

### includeThoughts Parameter

**Purpose:** Enable thought summaries in responses

**Format:**
```json
{
  "generationConfig": {
    "thinkingConfig": {
      "includeThoughts": true
    }
  }
}
```

### Thoughts in Response

Thoughts appear as parts with `thought: true` flag:

```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {
            "thought": true,
            "text": "First, I need to understand the problem. The user is asking about..."
          },
          {
            "text": "The solution to your problem is..."
          }
        ],
        "role": "model"
      }
    }
  ],
  "usageMetadata": {
    "thoughtsTokenCount": 47,
    "candidatesTokenCount": 120,
    "totalTokenCount": 167
  }
}
```

### Thought Signatures

**Purpose:** Encrypted representations of internal thought process for maintaining reasoning context across API calls

**Format:** Automatically included in responses when thinking is enabled, can be passed back in subsequent calls to maintain continuity

---

## 9. Streaming

### SSE (Server-Sent Events) Format

**Query Parameter:** `alt=sse`

**Accept Header:**
```
Accept: text/event-stream
```

### Streaming Request

```
POST https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:streamGenerateContent?alt=sse&key=YOUR_API_KEY
Content-Type: application/json
Accept: text/event-stream

{
  "contents": [
    {
      "role": "user",
      "parts": [{"text": "Write a story about a magic backpack"}]
    }
  ]
}
```

### SSE Event Format

Each event follows the SSE protocol format:
```
data: {JSON_OBJECT}

```

**Note:** Each event must be prefixed with `data: ` followed by newline characters (`\n\n`)

### Streaming Response Chunks

Each chunk is a complete `GenerateContentResponse` object:

```
data: {"candidates":[{"content":{"parts":[{"text":"Once upon a"}],"role":"model"},"index":0}]}

data: {"candidates":[{"content":{"parts":[{"text":" time, there was"}],"role":"model"},"index":0}]}

data: {"candidates":[{"content":{"parts":[{"text":" a magic backpack"}],"role":"model"},"index":0}]}

data: {"candidates":[{"content":{"parts":[{"text":"."}],"role":"model"},"finishReason":"STOP","index":0,"safetyRatings":[{"category":"HARM_CATEGORY_SEXUALLY_EXPLICIT","probability":"NEGLIGIBLE"},{"category":"HARM_CATEGORY_HATE_SPEECH","probability":"NEGLIGIBLE"},{"category":"HARM_CATEGORY_HARASSMENT","probability":"NEGLIGIBLE"},{"category":"HARM_CATEGORY_DANGEROUS_CONTENT","probability":"NEGLIGIBLE"}]}],"usageMetadata":{"promptTokenCount":8,"candidatesTokenCount":15,"totalTokenCount":23}}
```

### Delta Structure

Unlike OpenAI's explicit "delta" field, Gemini streaming:
- Sends incremental text chunks in `candidates[0].content.parts[0].text`
- Each chunk is a complete `GenerateContentResponse`
- Text accumulates across chunks
- Final chunk includes `finishReason: "STOP"` and `usageMetadata`
- Each response includes a `responseId` tying the full response together

### Full Streaming Response Example

```json
{
  "candidates": [
    {
      "content": {
        "parts": [{"text": "The image displays"}],
        "role": "model"
      },
      "index": 0
    }
  ],
  "usageMetadata": {
    "promptTokenCount": 10,
    "candidatesTokenCount": 3,
    "totalTokenCount": 13
  },
  "modelVersion": "gemini-2.5-flash",
  "responseId": "mAitaLmkHPPlz7IPvtfUqQ4"
}
```

---

## 10. Error Responses

### HTTP Status Codes

| Code | Status | Description |
|------|--------|-------------|
| 400 | INVALID_ARGUMENT | Request body is malformed (JSON errors, typos, missing fields) |
| 400 | FAILED_PRECONDITION | Free tier unavailable in region without billing enabled |
| 403 | PERMISSION_DENIED | API key doesn't have required permissions |
| 404 | NOT_FOUND | Requested resource wasn't found |
| 429 | RESOURCE_EXHAUSTED | Rate limit or quota exceeded |
| 500 | INTERNAL | Unexpected error on Google's side |
| 503 | UNAVAILABLE | Service temporarily overloaded or down |
| 504 | DEADLINE_EXCEEDED | Service unable to finish processing within deadline |

### Error Response Structure

```json
{
  "error": {
    "code": 400,
    "message": "string",
    "status": "ERROR_STATUS_STRING",
    "details": [
      {
        "@type": "type.googleapis.com/google.rpc.BadRequest",
        "fieldViolations": [
          {
            "field": "string",
            "description": "string"
          }
        ]
      }
    ],
    "errors": [
      {
        "message": "string",
        "domain": "string",
        "reason": "string"
      }
    ]
  }
}
```

### Error Response Examples

#### 400 INVALID_ARGUMENT

```json
{
  "error": {
    "code": 400,
    "message": "Invalid JSON payload received. Unknown name \"fileData\" at 'contents[0].parts[0]': Cannot find field.",
    "status": "INVALID_ARGUMENT",
    "details": [
      {
        "@type": "type.googleapis.com/google.rpc.BadRequest",
        "fieldViolations": [
          {
            "field": "contents[0].parts[0]",
            "description": "Invalid JSON payload received. Unknown name \"fileData\" at 'contents[0].parts[0]': Cannot find field."
          }
        ]
      }
    ]
  }
}
```

#### 429 RESOURCE_EXHAUSTED

```json
{
  "error": {
    "code": 429,
    "message": "Resource exhausted. Please try again later. Please refer to https://cloud.google.com/vertex-ai/generative-ai/docs/error-code-429 for more details.",
    "status": "RESOURCE_EXHAUSTED",
    "errors": [
      {
        "message": "Resource exhausted. Please try again later.",
        "domain": "global",
        "reason": "rateLimitExceeded"
      }
    ]
  }
}
```

#### 500 INTERNAL

```json
{
  "error": {
    "code": 500,
    "message": "An internal error has occurred. Please retry or report in https://developers.generativeai.google/guide/troubleshooting",
    "status": "INTERNAL"
  }
}
```

#### Function Calling Error

```json
{
  "error": {
    "code": 400,
    "message": "Function calling with a response mime type: 'application/json' is unsupported",
    "status": "INVALID_ARGUMENT"
  }
}
```

#### Blocked API Key

```json
{
  "error": {
    "code": 403,
    "message": "Your API key was reported as leaked. Please use another API key.",
    "status": "PERMISSION_DENIED"
  }
}
```

---

## 11. OpenAI-Compatible API

### Base URL
```
https://generativelanguage.googleapis.com/v1beta/openai/
```

### Authentication
```
Authorization: Bearer YOUR_GEMINI_API_KEY
```

### Python Example (OpenAI SDK)

```python
from openai import OpenAI

client = OpenAI(
    api_key="YOUR_GEMINI_API_KEY",
    base_url="https://generativelanguage.googleapis.com/v1beta/openai/"
)

response = client.chat.completions.create(
    model="gemini-2.5-flash",
    messages=[
        {"role": "user", "content": "Explain to me how AI works"}
    ]
)

print(response.choices[0].message.content)
```

### Gemini-Specific Features via extra_body

Features not in OpenAI can be accessed through `extra_body`:

```python
response = client.chat.completions.create(
    model="gemini-3-pro",
    messages=[
        {"role": "user", "content": "Solve this complex problem..."}
    ],
    extra_body={
        "google": {
            "thinking_config": {
                "thinking_level": "high",
                "include_thoughts": True
            },
            "cached_content": "cachedContents/abc123"
        }
    }
)
```

### Parameter Mappings

| OpenAI Parameter | Gemini Equivalent | Details |
|-----------------|-------------------|---------|
| `reasoning_effort` | `thinking_level` / `thinking_budget` | Maps to Gemini's thinking parameters |
| `messages` | `contents` | Same format (roles: user, assistant, system) |
| `model` | `model` | Use Gemini model names |
| `temperature` | `temperature` | Same range (0-2) |
| `max_tokens` | `maxOutputTokens` | Same meaning |
| `stream` | `stream` | Boolean, same behavior |

### Supported Capabilities
- Text generation (streaming and non-streaming)
- Function calling (identical to OpenAI format)
- Image understanding (base64 encoded)
- Image generation (Imagen models, paid tier)
- Audio understanding (base64 encoded)
- Structured output (JSON via Pydantic/Zod)
- Embeddings (gemini-embedding-001)
- Batch API (24-hour completion window)

### Beta Status
OpenAI library support is in beta while feature support is extended.

---

## 12. Additional Features

### Context Caching

**Purpose:** Reduce latency and cost for repeated prompts with large context

**Parameter:**
```json
{
  "cachedContent": "cachedContents/{cache_id}"
}
```

### Grounding

**Purpose:** Ground responses in external data sources

**Configuration:**
```json
{
  "groundingMetadata": {
    "webSearchQueries": ["search query"],
    "retrievalMetadata": {
      "googleSearchDynamicRetrievalScore": 0.5
    }
  }
}
```

### Safety Settings

**Categories:**
- `HARM_CATEGORY_HATE_SPEECH`
- `HARM_CATEGORY_SEXUALLY_EXPLICIT`
- `HARM_CATEGORY_DANGEROUS_CONTENT`
- `HARM_CATEGORY_HARASSMENT`
- `HARM_CATEGORY_CIVIC_INTEGRITY`

**Thresholds:**
- `BLOCK_NONE` / `OFF`
- `BLOCK_ONLY_HIGH`
- `BLOCK_MEDIUM_AND_ABOVE`
- `BLOCK_LOW_AND_ABOVE`

**Example:**
```json
{
  "safetySettings": [
    {
      "category": "HARM_CATEGORY_HATE_SPEECH",
      "threshold": "BLOCK_MEDIUM_AND_ABOVE"
    }
  ]
}
```

### Log Probabilities

**Enable:**
```json
{
  "generationConfig": {
    "responseLogprobs": true,
    "logprobs": 5
  }
}
```

**Response:**
```json
{
  "logprobsResult": {
    "topCandidates": [],
    "chosenCandidates": [],
    "logProbabilitySum": -12.34
  }
}
```

---

## 13. Rate Limits and Quotas

### Varies by:
- Model (2.5 vs 3 series)
- Service (AI Studio vs Vertex AI)
- Pricing tier (Free vs Paid)

### Common Limits:
- Requests per minute (RPM)
- Tokens per minute (TPM)
- Requests per day (RPD)

### Free Tier Limitations:
- Not available in all regions
- Lower rate limits
- Some models require paid tier (e.g., Gemini 3 Pro)

---

## 14. Token Information

### Approximations:
- 1 token ≈ 4 characters
- 100 tokens ≈ 60-80 English words

### Token Counting API:
- Endpoint: `countTokens`
- Free tier: 3000 requests per minute

---

## 15. Unique Features

1. **Thinking Parameters** - Explicit control over reasoning depth via `thinkingLevel` or `thinkingBudget`
2. **Thought Signatures** - Encrypted reasoning state preservation across calls
3. **Massive Context Windows** - Up to 2M tokens (gemini-2.5-pro)
4. **Multimodal Native** - Text, image, video, audio, PDF in single unified API
5. **System Instruction Placement** - Separate `systemInstruction` parameter for anchoring reasoning
6. **OpenAI Compatibility** - Drop-in support for OpenAI SDK
7. **JSON Schema Support** - Structured output with full JSON Schema validation
8. **Grounding** - Built-in web search and retrieval capabilities
9. **Context Caching** - Reduce costs for repeated large contexts

---

## Sources

- [Gemini API Reference | Google AI for Developers](https://ai.google.dev/api)
- [Generating Content | Gemini API](https://ai.google.dev/api/generate-content)
- [Function Calling with the Gemini API](https://ai.google.dev/gemini-api/docs/function-calling)
- [OpenAI Compatibility | Gemini API](https://ai.google.dev/gemini-api/docs/openai)
- [Gemini Thinking | Gemini API](https://ai.google.dev/gemini-api/docs/thinking)
- [System Instructions | Gemini API](https://ai.google.dev/gemini-api/docs/system-instructions)
- [Structured Outputs | Gemini API](https://ai.google.dev/gemini-api/docs/structured-output)
- [Troubleshooting Guide | Gemini API](https://ai.google.dev/gemini-api/docs/troubleshooting)
- [Gemini Models | Gemini API](https://ai.google.dev/gemini-api/docs/models)
- [New Gemini API Updates for Gemini 3 - Google Developers Blog](https://developers.googleblog.com/new-gemini-api-updates-for-gemini-3/)
- [Gemini is now accessible from the OpenAI Library - Google Developers Blog](https://developers.googleblog.com/en/gemini-is-now-accessible-from-the-openai-library/)
- [Generate Content with Gemini API in Vertex AI | Google Cloud](https://docs.cloud.google.com/vertex-ai/generative-ai/docs/model-reference/inference)
- [Using OpenAI Libraries with Vertex AI | Google Cloud](https://cloud.google.com/vertex-ai/generative-ai/docs/migrate/openai/overview)
- [Gemini API Cookbook - Streaming REST](https://github.com/google-gemini/cookbook/blob/main/quickstarts/rest/Streaming_REST.ipynb)
- [Gemini API Cookbook - Function Calling REST](https://github.com/google-gemini/cookbook/blob/main/quickstarts/rest/Function_calling_REST.ipynb)
- [Gemini API Cookbook - System Instructions](https://github.com/google-gemini/cookbook/blob/main/quickstarts/System_instructions.ipynb)
