# Fix: Anthropic Thinking Block Signatures

## Problem

HTTP 400 error occurs when continuing a conversation after a tool call when thinking is enabled.

## Reproduction

```
Switched to anthropic claude-sonnet-4-5
  Thinking: low (22016 tokens)
what is 1 + 1

1 + 1 = 2

what tools where you offered just now?

I have access to the following tools: [...]

can you provide me with a summary of the ./README.md file

-> file_read: path="./README.md"

<- file_read: {"success":true,"data":{"output":"# ikigai\n..."

Error: HTTP 400
```

## Root Cause

Anthropic's API returns a cryptographic `signature` with each thinking block. When sending thinking blocks back in subsequent requests (e.g., after a tool call), this signature **must** be included. Without it, Anthropic returns HTTP 400.

The current implementation:
1. Parses thinking text but ignores the signature
2. Stores thinking blocks without signature field
3. Serializes thinking blocks back to Anthropic without signature
4. Anthropic rejects the malformed request

## Affected Files

### 1. `src/providers/provider.h:155-158`

The `thinking` struct has no `signature` field:

```c
/* IK_CONTENT_THINKING */
struct {
    char *text; /* Thinking summary text */
} thinking;
```

**Fix:** Add `char *signature` field.

### 2. `src/providers/anthropic/streaming_events.c:86-88`

`content_block_start` for thinking blocks doesn't capture anything useful:

```c
} else if (strcmp(type_str, "thinking") == 0) {
    sctx->current_block_type = IK_CONTENT_THINKING;
    // No event emission for thinking blocks
}
```

**Fix:** May need to store thinking block state for signature capture.

### 3. `src/providers/anthropic/streaming_events.c` - Missing `signature_delta` handler

The signature arrives via `content_block_delta` with `delta.type = "signature_delta"`, NOT in `content_block_stop`.

Need to add handling for:
```json
{"type": "content_block_delta", "index": 0, "delta": {"type": "signature_delta", "signature": "EqQBCgIYAhIM..."}}
```

**Fix:** Add `signature_delta` case in `content_block_delta` handler to capture and store signature.

### 4. `src/providers/anthropic/request_serialize.c:36-39`

Thinking block serialization omits the signature:

```c
case IK_CONTENT_THINKING:
    if (!yyjson_mut_obj_add_str_(doc, obj, "type", "thinking")) return false;
    if (!yyjson_mut_obj_add_str_(doc, obj, "thinking", block->data.thinking.text)) return false;
    break;
```

**Fix:** Add signature serialization:

```c
case IK_CONTENT_THINKING:
    if (!yyjson_mut_obj_add_str_(doc, obj, "type", "thinking")) return false;
    if (!yyjson_mut_obj_add_str_(doc, obj, "thinking", block->data.thinking.text)) return false;
    if (!yyjson_mut_obj_add_str_(doc, obj, "signature", block->data.thinking.signature)) return false;
    break;
```

### 5. `src/providers/request_tools.c:209-212`

Deep copy doesn't copy signature:

```c
case IK_CONTENT_THINKING:
    dst->data.thinking.text = talloc_strdup(copy, src->data.thinking.text);
    if (dst->data.thinking.text == NULL) PANIC("Out of memory");
    break;
```

**Fix:** Add signature copy.

### 6. `src/providers/request.c:82-93`

`ik_content_block_thinking` only takes text:

```c
ik_content_block_t *ik_content_block_thinking(TALLOC_CTX *ctx, const char *text) {
    // ...
    block->type = IK_CONTENT_THINKING;
    block->data.thinking.text = talloc_strdup(block, text);
    // ...
}
```

**Fix:** Add signature parameter or create separate function.

### 7. `src/providers/request.h:57-64`

Function declaration needs signature parameter:

```c
ik_content_block_t *ik_content_block_thinking(TALLOC_CTX *ctx, const char *text);
```

## Implementation Plan

1. **Update struct** (`provider.h`)
   - Add `char *signature` to thinking struct

2. **Update streaming parser** (`streaming_events.c`)
   - Add `current_thinking_signature` to stream context
   - Handle `signature_delta` in `content_block_delta` (NOT in `content_block_stop`)
   - Store accumulated thinking text + signature

3. **Update response builder** (`streaming.c`)
   - Include thinking blocks (with signature) in response content

4. **Update serialization** (`request_serialize.c`)
   - Add signature field when serializing thinking blocks

5. **Update builders/copiers** (`request.c`, `request_tools.c`)
   - Add signature parameter to `ik_content_block_thinking`
   - Copy signature in deep copy function

6. **Handle `redacted_thinking` blocks**
   - Add `IK_CONTENT_REDACTED_THINKING` type with `data` field
   - Parse `redacted_thinking` in streaming events
   - Serialize back with `type: "redacted_thinking"` and `data` field

7. **Update tests**
   - Add test cases for thinking block round-trip
   - Add test for redacted_thinking serialization

## Anthropic API Reference

Source: [Anthropic Extended Thinking Docs](https://platform.claude.com/docs/en/build-with-claude/extended-thinking)

### Response Format

Thinking blocks in non-streaming responses:
```json
{
  "type": "thinking",
  "thinking": "Let me work through this...",
  "signature": "WaUjzkypQ2mUEVM36O2TxuC06..."
}
```

### Streaming Events

The signature arrives via a **separate `signature_delta` event** before `content_block_stop`:

```
event: content_block_start
data: {"type": "content_block_start", "index": 0, "content_block": {"type": "thinking", "thinking": ""}}

event: content_block_delta
data: {"type": "content_block_delta", "index": 0, "delta": {"type": "thinking_delta", "thinking": "Let me solve..."}}

event: content_block_delta
data: {"type": "content_block_delta", "index": 0, "delta": {"type": "signature_delta", "signature": "EqQBCgIYAhIM..."}}

event: content_block_stop
data: {"type": "content_block_stop", "index": 0}
```

Key insight: signature comes as `signature_delta` in a `content_block_delta` event, NOT in `content_block_stop`.

### Sending Thinking Blocks Back

When continuing after tool use, thinking blocks must be passed back unmodified:

```json
{
  "role": "assistant",
  "content": [
    {
      "type": "thinking",
      "thinking": "The user wants to know...",
      "signature": "BDaL4VrbR2Oj0hO4XpJxT28J..."
    },
    {
      "type": "tool_use",
      "id": "toolu_01...",
      "name": "get_weather",
      "input": {"location": "Paris"}
    }
  ]
}
```

### Redacted Thinking Blocks

Safety-flagged reasoning returns as `redacted_thinking` (encrypted, opaque):

```json
{
  "type": "redacted_thinking",
  "data": "EmwKAhgBEgy3va3pzix/LafPsn4..."
}
```

These must also be passed back unmodified when continuing conversations.
