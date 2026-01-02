# Plan: Anthropic Thinking Block Signatures

## Overview

This plan defines the interface changes needed to capture, store, and serialize Anthropic thinking block signatures. All tasks must use these exact definitions for consistency.

## 1. Content Type Enum Extension

**File:** `src/providers/provider.h`

Add new enum value after `IK_CONTENT_THINKING`:

```c
typedef enum {
    IK_CONTENT_TEXT = 0,
    IK_CONTENT_TOOL_CALL = 1,
    IK_CONTENT_TOOL_RESULT = 2,
    IK_CONTENT_THINKING = 3,
    IK_CONTENT_REDACTED_THINKING = 4  // NEW
} ik_content_type_t;
```

## 2. Content Block Struct Changes

**File:** `src/providers/provider.h`

### Thinking Block (existing, modified)

Add `signature` field:

```c
/* IK_CONTENT_THINKING */
struct {
    char *text;      /* Thinking summary text */
    char *signature; /* Cryptographic signature (required for round-trip) */
} thinking;
```

### Redacted Thinking Block (new)

Add new union member:

```c
/* IK_CONTENT_REDACTED_THINKING */
struct {
    char *data; /* Encrypted opaque data (base64) */
} redacted_thinking;
```

## 3. Stream Context Changes

**File:** `src/providers/anthropic/streaming.h`

Add thinking accumulation fields to `ik_anthropic_stream_ctx_t`:

| Field | Type | Purpose |
|-------|------|---------|
| `current_thinking_text` | `char *` | Accumulated thinking text from thinking_delta events |
| `current_thinking_signature` | `char *` | Signature from signature_delta event |
| `current_redacted_data` | `char *` | Data from redacted_thinking block |

## 4. Content Block Builder Functions

**File:** `src/providers/request.h`

### Modified Function

```c
/**
 * Create thinking content block with signature
 *
 * @param ctx       Talloc parent context
 * @param text      Thinking text (will be copied)
 * @param signature Cryptographic signature (will be copied)
 * @return          Allocated content block
 */
ik_content_block_t *ik_content_block_thinking(TALLOC_CTX *ctx,
                                               const char *text,
                                               const char *signature);
```

### New Function

```c
/**
 * Create redacted thinking content block
 *
 * @param ctx  Talloc parent context
 * @param data Encrypted opaque data (will be copied)
 * @return     Allocated content block
 */
ik_content_block_t *ik_content_block_redacted_thinking(TALLOC_CTX *ctx,
                                                        const char *data);
```

## 5. Streaming Event Handling

**File:** `src/providers/anthropic/streaming_events.c`

### New Delta Type: `signature_delta`

Handle in `ik_anthropic_process_content_block_delta`:

```json
{"type": "content_block_delta", "index": 0, "delta": {"type": "signature_delta", "signature": "EqQBCgIYAhIM..."}}
```

Extract `signature` field, store in `sctx->current_thinking_signature`.

### New Block Type: `redacted_thinking`

Handle in `ik_anthropic_process_content_block_start`:

```json
{"type": "content_block_start", "index": 1, "content_block": {"type": "redacted_thinking", "data": "EmwKAhgBEgy..."}}
```

Set `current_block_type = IK_CONTENT_REDACTED_THINKING`, store data in `sctx->current_redacted_data`.

## 6. Response Builder Changes

**File:** `src/providers/anthropic/streaming.c`

Function `ik_anthropic_stream_build_response` must include thinking blocks in response:

**Current behavior:** Only includes tool_call if present, otherwise empty content.

**New behavior:** Build content_blocks array containing:
1. Thinking block (if `current_thinking_text` is set) with signature
2. Redacted thinking block (if `current_redacted_data` is set)
3. Tool call (if `current_tool_id` is set)

Order matters: thinking blocks come before tool_use in Anthropic responses.

## 7. Serialization Format

**File:** `src/providers/anthropic/request_serialize.c`

### Thinking Block

```json
{
  "type": "thinking",
  "thinking": "<text>",
  "signature": "<signature>"
}
```

### Redacted Thinking Block

```json
{
  "type": "redacted_thinking",
  "data": "<encrypted_data>"
}
```

## 8. Deep Copy Updates

All content block copy functions must handle:

1. `IK_CONTENT_THINKING`: Copy both `text` and `signature`
2. `IK_CONTENT_REDACTED_THINKING`: Copy `data`

**Affected locations:**
- `src/providers/request_tools.c` - `ik_request_add_message_direct`
- `src/agent_messages.c` - `clone_content_block`

## Task Dependencies

```
types ─────────────────┬──> content-block-api ──> deep-copy
                       │
streaming-capture ─────┴──> response-builder
                       │
                       └──> serialization
```

All tasks can run after `types` completes. No other strict ordering required.
