# Google Gemini countTokens API

Reference documentation for the Google Gemini `countTokens` API, covering both the Gemini Developer API and the Vertex AI variant. Implementation: `apps/ikigai/providers/google/count_tokens.c` (goal #259).

---

## Endpoints

### Gemini Developer API (API Key)

```
POST https://generativelanguage.googleapis.com/v1beta/models/{MODEL_ID}:countTokens?key={API_KEY}
```

Authentication: API key as query parameter. This is the simpler variant — no OAuth, no project setup. Suitable for direct API key usage, which is how ikigai currently authenticates with Google.

### Vertex AI

```
POST https://{LOCATION}-aiplatform.googleapis.com/v1/projects/{PROJECT_ID}/locations/{LOCATION}/publishers/google/models/{MODEL_ID}:countTokens
```

Authentication: OAuth bearer token via `Authorization: Bearer $(gcloud auth print-access-token)`. Requires a GCP project and service account or user credentials.

The two endpoints accept the same request body schema and return the same response format. ikigai should target the Gemini Developer API since it already uses API key auth for Google.

---

## Cost

**Free.** No charge for calling countTokens. This is confirmed in Google's documentation and matches the design doc's assumption.

---

## Rate Limits

- **3000 requests per minute (RPM)** — confirmed for the Gemini Developer API
- Vertex AI rate limits are not explicitly documented for countTokens but are generally higher (tied to project quotas)

3000 RPM is generous. At one countTokens call per turn (after each IDLE transition), ikigai would need to sustain 50 turns per second to hit the limit. Not a practical concern.

---

## Request Format

The request body supports two mutually exclusive modes:

### Mode 1: Contents Only

Pass raw content without system instructions or tools. The count reflects only the content tokens, not the full request overhead.

```json
{
  "contents": [
    {
      "role": "user",
      "parts": [{ "text": "Hello, world!" }]
    }
  ]
}
```

### Mode 2: GenerateContentRequest (Recommended)

Pass a complete `generateContentRequest` — the same structure sent to `generateContent`. This counts everything: system instruction, tools, tool config, all messages, and formatting overhead. **This is the mode ikigai should use** because it matches the design doc's requirement to count the serialized request body.

```json
{
  "generateContentRequest": {
    "model": "models/gemini-2.5-flash",
    "contents": [
      {
        "role": "user",
        "parts": [{ "text": "What is the weather in London?" }]
      },
      {
        "role": "model",
        "parts": [{
          "functionCall": {
            "name": "get_weather",
            "args": { "location": "London" }
          }
        }]
      },
      {
        "role": "user",
        "parts": [{
          "functionResponse": {
            "name": "get_weather",
            "response": { "temperature": "15C", "condition": "cloudy" }
          }
        }]
      }
    ],
    "systemInstruction": {
      "parts": [{ "text": "You are a helpful weather assistant." }]
    },
    "tools": [
      {
        "functionDeclarations": [
          {
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
        ]
      }
    ],
    "toolConfig": {
      "functionCallingConfig": {
        "mode": "AUTO"
      }
    },
    "generationConfig": {
      "temperature": 0.7,
      "maxOutputTokens": 8192
    }
  }
}
```

**The two modes are mutually exclusive.** You send either `contents` at the top level or `generateContentRequest`, never both. If both are present, the API returns an error.

### GenerateContentRequest Fields

| Field | Type | Description |
|-------|------|-------------|
| `model` | string | Model resource name, e.g. `"models/gemini-2.5-flash"` |
| `contents` | Content[] | Conversation messages. Each has `role` (`"user"` or `"model"`) and `parts` |
| `systemInstruction` | Content | System instruction. Has `parts` array but no `role` field |
| `tools` | Tool[] | Tool definitions with `functionDeclarations` |
| `toolConfig` | ToolConfig | Tool calling configuration (mode: AUTO, ANY, NONE) |
| `cachedContent` | string | Resource name of cached content, if using context caching |
| `generationConfig` | GenerationConfig | Generation parameters (temperature, maxOutputTokens, etc.) |

### Content Parts

Parts within a Content object can be:

| Part Type | Fields | Description |
|-----------|--------|-------------|
| Text | `text` | Plain text |
| Inline data | `inlineData.mimeType`, `inlineData.data` | Base64-encoded binary (images, audio) |
| File data | `fileData.fileUri`, `fileData.mimeType` | Cloud Storage URI or HTTP URL |
| Function call | `functionCall.name`, `functionCall.args` | Tool invocation (role: model) |
| Function response | `functionResponse.name`, `functionResponse.response` | Tool result (role: user) |

For ikigai's text-only use case, only `text`, `functionCall`, and `functionResponse` parts are relevant.

---

## Response Format

```json
{
  "totalTokens": 42,
  "cachedContentTokenCount": 0,
  "promptTokensDetails": [
    {
      "modality": "TEXT",
      "tokenCount": 42
    }
  ],
  "cacheTokensDetails": []
}
```

| Field | Type | Description |
|-------|------|-------------|
| `totalTokens` | integer | Total token count for the entire prompt. Always non-negative. **This is the value ikigai needs.** |
| `cachedContentTokenCount` | integer | Tokens in the cached portion (0 if no caching) |
| `promptTokensDetails` | ModalityTokenCount[] | Per-modality breakdown (TEXT, IMAGE, VIDEO, AUDIO) |
| `cacheTokensDetails` | ModalityTokenCount[] | Per-modality breakdown of cached tokens |

For ikigai, only `totalTokens` matters. The other fields are informational.

---

## What Gets Counted

When using `generateContentRequest` mode, the count includes:

- **System instruction** (`systemInstruction` field)
- **Tool definitions** (all `functionDeclarations` — names, descriptions, parameter schemas)
- **All messages** in `contents` (user text, model responses, function calls, function responses)
- **Formatting overhead** (role markers, structural JSON tokens, internal formatting)

This matches the design doc's requirement: counting the serialized request body captures everything the provider charges for.

Google does not document the exact formatting overhead (unlike OpenAI's documented ChatML overhead). The countTokens API is the only way to get an exact count that includes all internal formatting.

---

## Tokenizer Details

- **Algorithm**: SentencePiece BPE (byte-pair encoding)
- **Vocabulary size**: ~256,000 tokens
- **Shared with**: Gemma open-weights models (same tokenizer.model file)
- **Model file**: ~4.24 MB, publicly available via the Gemma release

The 256K vocabulary is the largest among the three providers (Anthropic ~65K, OpenAI ~200K). Larger vocabularies generally produce fewer tokens for the same text, meaning Google's byte-to-token ratio may be slightly better than the ~4 bytes/token average.

The tokenizer model file is available at:
https://github.com/google/gemma_pytorch/blob/main/tokenizer/tokenizer.model

An offline SentencePiece-based tokenizer is feasible (the C++ library is available), but Phase 2 targets the API approach. Offline tokenization could be a Phase 3 optimization if the 3000 RPM limit ever becomes a concern.

---

## Caching Considerations

Context caching does **not** change what gets counted as input tokens. It only affects billing rate (75% discount on cache hits for Google). The `cachedContentTokenCount` field in the response shows how many tokens came from cache, but `totalTokens` always reports the full count regardless of caching.

For sliding window budget purposes, caching is irrelevant — the budget tracks total input tokens, not billed tokens.

---

## Supported Models

countTokens works with all Gemini models available through the API. As of early 2026, this includes:

- Gemini 3.x (Flash, Pro)
- Gemini 2.5 (Flash, Pro)
- Gemini 2.0 (Flash)

The model is specified in the URL path (`models/{MODEL_ID}`). If a model supports `generateContent`, it supports `countTokens`.

---

## Example: curl

### Simple text count

```bash
curl -s -X POST \
  "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:countTokens?key=$GEMINI_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "contents": [{
      "parts": [{ "text": "The quick brown fox jumps over the lazy dog." }]
    }]
  }'
```

Response:

```json
{
  "totalTokens": 10,
  "promptTokensDetails": [
    { "modality": "TEXT", "tokenCount": 10 }
  ]
}
```

### Full request with system instruction and tools

```bash
curl -s -X POST \
  "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:countTokens?key=$GEMINI_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "generateContentRequest": {
      "model": "models/gemini-2.5-flash",
      "systemInstruction": {
        "parts": [{ "text": "You are a helpful coding assistant." }]
      },
      "contents": [
        {
          "role": "user",
          "parts": [{ "text": "Write a hello world program in C." }]
        }
      ],
      "tools": [{
        "functionDeclarations": [{
          "name": "run_code",
          "description": "Execute code in a sandbox",
          "parameters": {
            "type": "object",
            "properties": {
              "language": { "type": "string" },
              "code": { "type": "string" }
            },
            "required": ["language", "code"]
          }
        }]
      }]
    }
  }'
```

---

## Implementation Notes for ikigai

### Request construction

The token counting module receives the serialized request body (the same JSON sent to `generateContent`). For Google, the implementation should:

1. Wrap the existing request body in a `generateContentRequest` envelope
2. POST to the countTokens endpoint
3. Parse `totalTokens` from the response

The request body for `countTokens` with `generateContentRequest` is almost identical to what ikigai already builds for `generateContent` — it just gets nested one level deeper.

### Error handling

- **400 Bad Request**: Invalid request body (malformed JSON, both `contents` and `generateContentRequest` present)
- **403 Forbidden**: Invalid API key
- **429 Too Many Requests**: Rate limit exceeded (3000 RPM)
- **404 Not Found**: Invalid model name

On error, fall back to the bytes-based estimate (~4 bytes/token). The sliding window must never block on a failed countTokens call.

### Latency

countTokens is a lightweight endpoint — it runs the tokenizer without inference. Expect single-digit millisecond latency for text-only requests. Network round-trip dominates.

### Comparison: API count vs bytes estimation

| Aspect | countTokens API | ~4 bytes/token |
|--------|----------------|----------------|
| Accuracy | Exact | ~10-30% variance |
| Includes formatting overhead | Yes | No (but overcounts content, roughly cancels) |
| Latency | Network round-trip (~10-50ms) | Instant (strlen / 4) |
| Availability | Requires network + valid API key | Always available |
| Rate limit | 3000 RPM | None |

The bytes estimate is the correct Phase 1 approach. The API count is a Phase 2 upgrade that trades a network call for exact counts. The dispatch pattern in the token counting module makes swapping trivial.

---

## Quirks and Gotchas

1. **Role naming**: Google uses `"user"` and `"model"` for roles, not `"user"` and `"assistant"`. The existing Google provider code in ikigai already handles this mapping.

2. **Function calls/responses are parts, not messages**: Unlike Anthropic and OpenAI where tool calls are separate message types, Google embeds `functionCall` and `functionResponse` as parts within a Content object. This is already handled by ikigai's Google request builder.

3. **No `role` on systemInstruction**: The `systemInstruction` Content object should not have a `role` field (or if present, it's ignored). Only `parts` matters.

4. **generateContentRequest includes `model`**: The model appears both in the URL path and inside the `generateContentRequest.model` field. Both should match. If they differ, behavior is undefined.

5. **Empty contents**: Sending an empty `contents` array is valid and returns the token count for just the system instruction and tools (if provided via `generateContentRequest`). Useful for measuring fixed overhead.

6. **v1beta vs v1**: The Gemini Developer API uses `v1beta` in the path. This has been stable for years despite the "beta" label. Vertex AI uses `v1`.

---

## References

- [Gemini API Token Counting Guide](https://ai.google.dev/gemini-api/docs/tokens)
- [countTokens API Reference](https://ai.google.dev/api/tokens)
- [Vertex AI CountTokens Reference](https://docs.cloud.google.com/vertex-ai/generative-ai/docs/model-reference/count-tokens)
- [SentencePiece GitHub](https://github.com/google/sentencepiece)
- [Gemma Tokenizer Model](https://github.com/google/gemma_pytorch/blob/main/tokenizer/tokenizer.model)
- [Existing project notes: project/tokens/gemini-tokens.md](../tokens/gemini-tokens.md)
