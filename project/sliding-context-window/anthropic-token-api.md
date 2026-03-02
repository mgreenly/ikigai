# Anthropic Token Counting API

Reference documentation for the `messages.countTokens` endpoint, used by the sliding context window token counting module (see `project/sliding-context-window.md`). Implementation: `apps/ikigai/providers/anthropic/count_tokens.c` (goal #257).

---

## Endpoint

```
POST https://api.anthropic.com/v1/messages/count_tokens
```

## Authentication

Three required headers:

| Header | Value |
|--------|-------|
| `x-api-key` | Your Anthropic API key |
| `content-type` | `application/json` |
| `anthropic-version` | `2023-06-01` |

No beta header required. The endpoint is GA (graduated from beta in late 2024).

---

## Request Format

The request body mirrors the Messages API create request. You send the same JSON you would send to `/v1/messages`, minus output-only fields like `max_tokens` and `stream`.

### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `model` | `string` | Model identifier (e.g., `"claude-opus-4-6"`, `"claude-sonnet-4-6"`, `"claude-haiku-4-5"`) |
| `messages` | `array` | Array of message objects with `role` and `content` fields. Same format as the Messages API. Limit: 100,000 messages per request. |

### Optional Fields

| Field | Type | Description |
|-------|------|-------------|
| `system` | `string` or `array` | System prompt. Can be a plain string or an array of `TextBlockParam` objects (with optional `cache_control`). |
| `tools` | `array` | Tool definitions. Array of tool objects with `name`, `description`, and `input_schema`. |
| `thinking` | `object` | Extended thinking configuration. `{"type": "enabled", "budget_tokens": N}` or `{"type": "disabled"}`. |

### Message Content Types

Messages support the same content block types as the Messages API:

- `TextBlockParam` — plain text
- `ImageBlockParam` — base64 or URL images (jpeg, png, gif, webp)
- `DocumentBlockParam` — base64 or URL PDFs, plain text documents
- `ToolUseBlockParam` — tool use blocks (in assistant messages)
- `ToolResultBlockParam` — tool results (in user messages)
- `ThinkingBlockParam` — thinking blocks (in assistant messages, with `signature`)
- `RedactedThinkingBlockParam` — redacted thinking blocks
- `SearchResultBlockParam` — search result blocks

All content blocks support optional `cache_control` with `type: "ephemeral"` and optional `ttl` (`"5m"` or `"1h"`).

---

## Response Format

```json
{
  "input_tokens": 14
}
```

That's it. A single object with one field:

| Field | Type | Description |
|-------|------|-------------|
| `input_tokens` | `integer` | Total number of input tokens across messages, system prompt, and tools. |

No breakdown by component. No cache information. Just the total.

---

## Cost

**Free.** Token counting does not incur any token charges. It is rate-limited only.

The endpoint is also ZDR (Zero Data Retention) eligible — when your organization has a ZDR arrangement, data sent through this endpoint is not stored after the response is returned.

---

## Rate Limits

Token counting has its own rate limits, **separate and independent** from the Messages API. Using one does not count against the other.

| Usage Tier | Requests per Minute (RPM) |
|------------|---------------------------|
| Tier 1 | 100 |
| Tier 2 | 2,000 |
| Tier 3 | 4,000 |
| Tier 4 | 8,000 |

The only limit is RPM. There are no input token per minute (ITPM) limits for counting.

For context, tier advancement requires cumulative credit purchases: Tier 1 = $5, Tier 2 = $40, Tier 3 = $200, Tier 4 = $400.

---

## Supported Models

All active Claude models support token counting. This includes the full Claude 4.x family (Opus, Sonnet, Haiku) and older models still in the active window.

All Claude models use the same tokenizer (proprietary BPE, ~65K vocabulary), so token counts are consistent across models for the same input — but you must still specify the model, because tool overhead injection varies.

---

## What Gets Counted

The count includes everything the model would see as input tokens:

- **System prompt** — your `system` field content
- **Messages** — all user, assistant, tool_use, and tool_result content
- **Tool definitions** — names, descriptions, and JSON schemas
- **Formatting overhead** — role labels, message structure, internal formatting
- **System-injected tokens** — see next section
- **Images** — tokenized at a fixed rate based on dimensions
- **PDFs** — converted to tokens internally
- **Extended thinking** — thinking blocks from *previous* assistant turns are **ignored** (not counted). Only the current assistant turn's thinking counts.

### Hidden System Prompt for Tools

When tools are provided, Anthropic injects a hidden system prompt that instructs the model on tool usage. This prompt is not visible in the request or response, but its tokens are included in the count.

Observed overhead: **~313-346 tokens** as a base cost when any tools are defined. This is a fixed cost independent of the number of tools — it is the instruction block, not the tool schemas themselves (those are counted separately).

This overhead is reflected in the `countTokens` response but **you are not billed for system-added tokens**. Billing reflects only your content. However, these tokens do consume context window space.

Example: A message with tools that counts as 403 input tokens (as in the official docs example) includes ~346 tokens of hidden overhead plus ~57 tokens of actual user content and tool schema.

---

## Caching Considerations

**Token counting does not use prompt caching.** You may include `cache_control` blocks in the request, but:

- The count reflects the total tokens as if nothing were cached
- Prompt caching only occurs during actual `messages.create` calls
- There is no way to get a "cached vs. uncached" breakdown from `countTokens`

For sliding window budget purposes, this is the correct behavior — we care about context window consumption, not billing rate. Caching affects cost, not context size.

---

## Example: curl Request and Response

### Basic message

```bash
curl https://api.anthropic.com/v1/messages/count_tokens \
    --header "x-api-key: $ANTHROPIC_API_KEY" \
    --header "content-type: application/json" \
    --header "anthropic-version: 2023-06-01" \
    --data '{
      "model": "claude-sonnet-4-6",
      "system": "You are a scientist",
      "messages": [{
        "role": "user",
        "content": "Hello, Claude"
      }]
    }'
```

Response:

```json
{"input_tokens": 14}
```

### With tools

```bash
curl https://api.anthropic.com/v1/messages/count_tokens \
    --header "x-api-key: $ANTHROPIC_API_KEY" \
    --header "content-type: application/json" \
    --header "anthropic-version: 2023-06-01" \
    --data '{
      "model": "claude-sonnet-4-6",
      "tools": [{
        "name": "get_weather",
        "description": "Get the current weather in a given location",
        "input_schema": {
          "type": "object",
          "properties": {
            "location": {
              "type": "string",
              "description": "The city and state, e.g. San Francisco, CA"
            }
          },
          "required": ["location"]
        }
      }],
      "messages": [{
        "role": "user",
        "content": "What'\''s the weather like in San Francisco?"
      }]
    }'
```

Response:

```json
{"input_tokens": 403}
```

Note the jump from ~14 to ~403 tokens — that delta includes both the tool schema and the ~346 hidden system prompt for tool usage.

---

## Accuracy

The count is described as an **estimate** by the official documentation:

> The token count should be considered an estimate. In some cases, the actual number of input tokens used when creating a message may differ by a small amount.

In practice, the count is very close to exact — the variance is small and typically on the order of a few tokens. For sliding window budget decisions, this is more than sufficient.

### vs. Bytes-Based Estimation (~4 bytes/token)

| Property | `countTokens` API | Bytes estimate |
|----------|-------------------|----------------|
| Accuracy | Near-exact (off by a few tokens) | Rough (~10-30% variance) |
| Latency | Network round-trip (50-200ms typical) | In-process, instant |
| Cost | Free (rate-limited) | Free (no limits) |
| Offline | No (requires API access) | Yes |
| Tool overhead | Captures hidden system prompt | Misses it entirely |
| Image/PDF tokens | Accurate | Cannot estimate |

The bytes-based estimate (`apps/ikigai/token_count.c`) serves as a fallback when the API is unavailable, but undercounts tool overhead and cannot handle images or PDFs. The API is the primary path for token counting in the sliding context window.

---

## Quirks, Limitations, and Implementation Notes

### For a C implementation

1. **Simple HTTP POST.** The request is the same JSON body you already build for the Messages API, minus `max_tokens` and `stream`. The response is a single integer in a JSON object. No streaming, no SSE, no complexity.

2. **Reuse the request builder.** The serialized request body for `messages.create` can be adapted for `countTokens` by stripping output-related fields. The message and tool schemas are identical.

3. **Server tool caveat.** Server tool token counts (e.g., `code_execution`) only apply to the first sampling call. For subsequent calls, the count may differ. ikigai does not currently use server tools, so this is not relevant.

4. **Extended thinking quirk.** Thinking blocks from previous assistant turns are dropped from the count (they don't consume input tokens). This means you cannot simply count all blocks in the message history — the API handles this automatically, but a local estimator would need to replicate this behavior.

5. **100,000 message limit.** The API accepts up to 100,000 messages in a single request. For ikigai's sliding window, this is effectively unlimited — if we have 100K messages, we have bigger problems.

6. **Rate limit headroom.** At Tier 2 (2,000 RPM), calling `countTokens` after every turn is easily sustainable. Even at Tier 1 (100 RPM), it would only be a concern for automated testing that sends many turns per minute.

7. **No offline fallback.** Anthropic does not publish their tokenizer vocabulary or merge rules. There is no way to count tokens without making an API call. The bytes-based fallback in `token_count.c` remains necessary for offline scenarios and as a circuit breaker if the API is unavailable.

8. **No breakdown.** The API returns only a total count. If we need per-component counts (e.g., "how many tokens is the system prompt alone?"), we would need to make multiple API calls with different subsets, which is wasteful. The bytes-based estimate may be more practical for component-level analysis.

9. **Error responses.** On error, the API returns standard Anthropic error JSON with `type`, `error.type`, and `error.message`. Rate limit errors (429) include a `retry-after` header. The C implementation should handle these gracefully and fall back to bytes-based estimation.

---

## References

- [Token Counting Guide](https://platform.claude.com/docs/en/docs/build-with-claude/token-counting) — official user guide
- [Count Tokens API Reference](https://platform.claude.com/docs/en/api/messages-count-tokens) — API reference
- [Rate Limits](https://platform.claude.com/docs/en/api/rate-limits) — tier-based rate limits
- `project/sliding-context-window.md` — design document
- `apps/ikigai/providers/anthropic/count_tokens.c` — implementation
- `project/tokens/anthropic-tokens.md` — existing tokenizer research
