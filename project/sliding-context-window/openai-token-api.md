# OpenAI Token Counting — API and Tokenizer Reference

**Purpose**: Reference for OpenAI token counting in the sliding context window. Implementation: `apps/ikigai/providers/openai/count_tokens.c` (goal #258).
**Related**: `project/sliding-context-window.md` (design doc), `apps/ikigai/token_count.h` (bytes estimator fallback)

---

## Server-Side API: Count Input Tokens

OpenAI provides a server-side token counting endpoint as part of the Responses API. It accepts the same input format as the Responses API itself, so you can send your exact payload and get back the token count the model would see.

### Endpoint

```
POST https://api.openai.com/v1/responses/input_tokens
```

### Authentication

Standard Bearer token via `Authorization` header:

```
Authorization: Bearer $OPENAI_API_KEY
```

### Request Format

The request body mirrors the Responses API `create` endpoint. Key parameters:

```json
{
  "model": "gpt-4o",
  "input": "Tell me a joke."
}
```

The `input` field accepts the same types as the Responses API:

- **String**: plain text input
- **Array of input items**: multi-turn conversations with role/content pairs
  - Roles: `user`, `assistant`, `system`, `developer`
  - Content types: text, images (via `image_url` or `file_id`), files (PDF via `file_id`, `file_url`, or `file_data`), audio
- **Tools**: function/tool definitions (JSON schema format)
- **Conversation ID**: reference an existing conversation by ID

### Response Format

```json
{
  "object": "response.input_tokens",
  "input_tokens": 42
}
```

Returns a single integer count representing the exact number of input tokens the model would consume.

### Example curl Request

```bash
curl -X POST https://api.openai.com/v1/responses/input_tokens \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-4o",
    "input": [
      { "role": "system", "content": "You are a helpful assistant." },
      { "role": "user", "content": "Hello, how are you?" }
    ]
  }'
```

### Cost

OpenAI does not document the count endpoint as free. The Responses API overall bills at the chosen model's input/output token rates. Whether the count endpoint itself incurs a charge is not explicitly stated — it likely does not generate output tokens but may count against rate limits. This needs verification during implementation.

**Contrast with Anthropic**: Anthropic explicitly documents `messages.countTokens` as free. Google explicitly documents `countTokens` as free. OpenAI's documentation is silent on this point.

### Rate Limits

The endpoint shares rate limits with the Responses API. OpenAI rate limits are measured across five dimensions:

| Dimension | Description |
|-----------|-------------|
| RPM | Requests per minute |
| RPD | Requests per day |
| TPM | Tokens per minute |
| TPD | Tokens per day |
| IPM | Images per minute |

Specific limits depend on the account tier. Rate limit headers are returned in HTTP responses. The count endpoint likely counts as one request against RPM/RPD but consumes zero output tokens against TPM/TPD.

### Why Use the Server-Side API

The server-side endpoint handles things that tiktoken alone cannot:

- **Images**: token count depends on resolution and tiling, not just bytes
- **Files/PDFs**: token count depends on extraction and formatting
- **Tools**: schemas are internally converted to TypeScript declarations before tokenizing (see below)
- **Model-specific behavior**: formatting overhead varies by model family

For text-only requests without tools, client-side tiktoken gives equivalent results (with ChatML overhead adjustments).

---

## Client-Side: tiktoken

tiktoken is OpenAI's open-source BPE tokenizer, written in Python and Rust. It provides exact token counts for text content without making API calls.

### How BPE Works

Byte Pair Encoding starts with a base vocabulary of 256 individual UTF-8 bytes, then iteratively merges the most frequent consecutive byte pairs. Each merge creates a new token added to the vocabulary. The process continues until the target vocabulary size is reached.

The encoding is defined by:
- **`pat_str`**: a regex pattern that splits text into pre-tokenization chunks
- **`mergeable_ranks`**: a dictionary mapping byte sequences to token IDs (the merge table)
- **Special tokens**: a separate mapping of control tokens to IDs

### Encodings and Vocabulary Sizes

| Encoding | Vocabulary Size | Models |
|----------|----------------|--------|
| `o200k_base` | 200,000 tokens | GPT-4o, GPT-4o-mini, o1, o3 |
| `cl100k_base` | 100,000 tokens | GPT-4-turbo, GPT-4, GPT-3.5-turbo |
| `p50k_base` | 50,000 tokens | Codex, text-davinci-002/003 (legacy) |

For ikigai's purposes, only `o200k_base` and `cl100k_base` matter. New OpenAI models use `o200k_base`.

### Special Tokens

`o200k_base` special tokens include:
- `<|im_start|>` (ID 200264): marks the start of a ChatML message
- `<|im_end|>` (ID 200265): marks the end of a ChatML message
- `<|im_sep|>` (ID 200266): separator within ChatML messages

`cl100k_base` uses the same token names but with different IDs:
- `<|im_start|>` (ID 100264)
- `<|im_end|>` (ID 100265)

### C Implementation Options

tiktoken itself is Python/Rust with no C bindings. Options for a C implementation:

1. **Port the BPE algorithm**: The core algorithm is straightforward — load the merge table, apply the regex pattern to split input, then greedily merge byte pairs according to rank. The merge tables are distributed as downloadable data files (one per encoding).
2. **FFI to Rust**: Call tiktoken's Rust core via C FFI. More complex build setup.
3. **Use the server-side API**: Avoid client-side tokenization entirely. Simpler but adds network latency and a hard dependency on API availability.
4. **Bytes-based estimation**: The Phase 1 approach (~4 bytes/token). No tokenizer needed but less accurate.

For ikigai, option 3 (server-side API) is the natural Phase 2 path since the module already takes the serialized request body. Option 1 would be needed only if offline/low-latency counting becomes a requirement.

---

## ChatML Formatting Overhead

OpenAI models do not see raw JSON. Messages are converted to ChatML format internally using special tokens. This adds overhead that must be accounted for when counting tokens client-side.

### Per-Message Overhead

Every message in the conversation costs **3 extra tokens** for ChatML framing:

```
<|im_start|>{role}\n{content}<|im_end|>\n
```

The 3 tokens are: `<|im_start|>`, the role token, and `<|im_end|>`.

If a message includes the `name` field, it costs **1 fewer token** than it otherwise would (the name replaces some overhead).

### Reply Priming

Every completion is primed with **3 tokens**:

```
<|im_start|>assistant\n
```

This is added by the API automatically and counts against input tokens.

### Tool/Function Definition Overhead

When tools are defined in the request, an additional **9 tokens** of fixed overhead are added per completion. This likely corresponds to a system message containing the TypeScript-formatted tool declarations.

Per-function overhead varies by model family:

| Component | GPT-4o / GPT-4o-mini | GPT-3.5-turbo / GPT-4 |
|-----------|----------------------|------------------------|
| Function initialization | 7 tokens | 10 tokens |
| Function completion | 12 tokens | 12 tokens |
| Each property | 3 tokens | 3 tokens |
| Property key | 3 tokens | 3 tokens |

Enum values incur additional tokens per item.

### Tool Schema Conversion to TypeScript

OpenAI does not tokenize tool JSON schemas directly. Instead, it internally converts them to TypeScript type declarations, then tokenizes the TypeScript. Example conversion:

**JSON Schema input**:
```json
{
  "name": "get_weather",
  "description": "Get the current weather",
  "parameters": {
    "type": "object",
    "properties": {
      "location": {
        "type": "string",
        "description": "City name"
      },
      "unit": {
        "type": "string",
        "enum": ["celsius", "fahrenheit"]
      }
    },
    "required": ["location"]
  }
}
```

**Internal TypeScript representation** (approximate):
```typescript
namespace functions {
  // Get the current weather
  type get_weather = (_: {
    // City name
    location: string,
    unit?: "celsius" | "fahrenheit",
  }) => any;
}
```

The TypeScript representation is what actually gets tokenized. This means the token count for tools depends on description length, parameter names, type names, and enum values — not on the JSON schema's structural verbosity.

**Implication for server-side counting**: This TypeScript conversion is one of the main reasons client-side token counting for tool-bearing requests is unreliable. The server-side `/v1/responses/input_tokens` endpoint handles this automatically.

---

## Counting Summary

### What Gets Counted

For an OpenAI chat request, the total input token count is:

```
total = system_tokens
      + sum(message_tokens + 3) for each message
      + 3                       (reply priming)
      + tool_tokens             (TypeScript-converted schemas)
      + 9                       (if tools are defined)
```

Where `message_tokens` is the tiktoken count of the message content, and `tool_tokens` is the tiktoken count of the TypeScript-converted tool declarations.

### Server-Side vs Client-Side

| Approach | Accuracy | Latency | Complexity | Handles Tools | Handles Images |
|----------|----------|---------|------------|---------------|----------------|
| Server-side API | Exact | Network RTT | Low (HTTP call) | Yes | Yes |
| tiktoken + ChatML overhead | Exact for text | None | Medium (BPE + overhead rules) | Approximate | No |
| Bytes estimate (~4 bytes/token) | ~10-20% error | None | Trivial | Implicitly | Implicitly |

### Comparison to ~4 Bytes/Token Estimation

The bytes-based estimate from Phase 1 divides the serialized JSON request size by 4. How this compares:

- **Plain text**: ~4 bytes/token is a reasonable average for English with `o200k_base`. The larger vocabulary (200K vs 100K) means slightly fewer tokens per byte on average.
- **JSON overhead**: The serialized request contains JSON structural bytes (`"role":`, `"content":`, braces, brackets) that inflate byte count without proportionally inflating token count. This makes the estimate slightly conservative (overcounts tokens).
- **Tool schemas**: JSON schema serialization is verbose. The actual token count is based on the TypeScript conversion, which is more compact. The bytes estimate overcounts tool tokens.
- **ChatML overhead**: The 3-tokens-per-message overhead is not represented in the JSON bytes. This undercounts.
- **Net effect**: Overcounting from JSON structure roughly cancels undercounting from ChatML overhead. The estimate is conservative (prunes slightly early), which is the correct direction for a budget ceiling.

---

## Supported Models and Encodings

Current model-to-encoding mapping for models ikigai is likely to target:

| Model | Encoding | Vocabulary |
|-------|----------|------------|
| gpt-4o | o200k_base | 200K |
| gpt-4o-mini | o200k_base | 200K |
| o1 | o200k_base | 200K |
| o1-mini | o200k_base | 200K |
| o3 | o200k_base | 200K |
| o3-mini | o200k_base | 200K |
| gpt-4-turbo | cl100k_base | 100K |
| gpt-4 | cl100k_base | 100K |
| gpt-3.5-turbo | cl100k_base | 100K |

New models default to `o200k_base`. The `cl100k_base` encoding is effectively legacy.

---

## Quirks and Gotchas

### No Explicit Free Guarantee

Unlike Anthropic and Google, OpenAI does not explicitly state that the count endpoint is free. It likely does not generate output tokens (and thus incurs no per-token output cost), but it may count against request-based rate limits. Budget for this uncertainty.

### Responses API, Not Chat Completions

The count endpoint is part of the **Responses API** (`/v1/responses/input_tokens`), not the older Chat Completions API (`/v1/chat/completions`). If ikigai currently uses Chat Completions, the count endpoint still works — you just need to format the request in Responses API format for the count call.

### Tool Counting is a Black Box

The TypeScript conversion of tool schemas is an internal OpenAI implementation detail. It is not formally specified. The conversion rules described above are reverse-engineered from community testing. For exact tool token counts, use the server-side API.

### Image Token Counting

Image tokens depend on resolution and internal tiling strategy. There is no reliable client-side formula. The server-side API is the only accurate option for image-bearing requests. This is less relevant for ikigai (which is terminal-based and unlikely to send images), but worth noting for completeness.

### Model Parameter is Required

The count endpoint requires a `model` parameter because tokenization (encoding, overhead rules) varies by model. Always pass the same model you intend to use for the actual request.

---

## Implementation Recommendation for ikigai

For Phase 2 OpenAI token counting:

1. **Use the server-side API** (`POST /v1/responses/input_tokens`). It handles ChatML overhead, tool TypeScript conversion, and model-specific behavior automatically.
2. **Format the request body** to match the Responses API schema. If ikigai builds requests in Chat Completions format, a thin adapter may be needed to reformat for the count call.
3. **Parse the response**: extract `input_tokens` from the JSON response.
4. **Fall back to bytes estimate** if the API call fails (network error, rate limit). The bytes estimate is conservative and safe for budget enforcement.
5. **Cache aggressively**: the system prompt and tool definitions are stable across turns. Count them once and cache the result. Only re-count the message delta on each turn.

This matches the dispatch pattern in the design doc — the `openai_estimate()` branch makes one HTTP call and returns the integer count.
