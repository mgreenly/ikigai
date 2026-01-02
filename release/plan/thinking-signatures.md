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

## 9. Tool Call Message Integration (PHASE 2)

**Problem:** When a response contains thinking + tool_use, only the tool_call is stored in the assistant message. The thinking block (with signature) is lost.

### Agent Context Changes

**File:** `src/agent.h`

Add fields to `ik_agent_ctx_t` to store pending thinking blocks:

```c
// Pending thinking blocks from response (for tool call messages)
char *pending_thinking_text;
char *pending_thinking_signature;
char *pending_redacted_data;
```

### Response Processing

**File:** `src/repl_callbacks.c`

In `extract_tool_calls` (or new function), also extract thinking blocks:

```c
for (size_t i = 0; i < response->content_count; i++) {
    ik_content_block_t *block = &response->content_blocks[i];
    if (block->type == IK_CONTENT_THINKING) {
        agent->pending_thinking_text = talloc_strdup(agent, block->data.thinking.text);
        agent->pending_thinking_signature = talloc_strdup(agent, block->data.thinking.signature);
    } else if (block->type == IK_CONTENT_REDACTED_THINKING) {
        agent->pending_redacted_data = talloc_strdup(agent, block->data.redacted_thinking.data);
    }
}
```

### Tool Call Message Creation

**File:** `src/repl_tool.c`

Replace `ik_message_create_tool_call` with a function that creates a message with all content blocks:

```c
// Count blocks
size_t block_count = 1; // tool_call
if (agent->pending_thinking_text) block_count++;
if (agent->pending_redacted_data) block_count++;

// Create message with multiple content blocks
ik_message_t *msg = talloc_zero(agent, ik_message_t);
msg->role = IK_ROLE_ASSISTANT;
msg->content_blocks = talloc_array(msg, ik_content_block_t, block_count);
msg->content_count = block_count;

size_t idx = 0;
// Add thinking block first (if present)
if (agent->pending_thinking_text) {
    msg->content_blocks[idx].type = IK_CONTENT_THINKING;
    msg->content_blocks[idx].data.thinking.text = talloc_strdup(...);
    msg->content_blocks[idx].data.thinking.signature = talloc_strdup(...);
    idx++;
}
// Add redacted thinking (if present)
if (agent->pending_redacted_data) {
    msg->content_blocks[idx].type = IK_CONTENT_REDACTED_THINKING;
    msg->content_blocks[idx].data.redacted_thinking.data = talloc_strdup(...);
    idx++;
}
// Add tool_call
msg->content_blocks[idx].type = IK_CONTENT_TOOL_CALL;
// ... fill tool_call fields
```

### Cleanup

Clear pending thinking fields after use:

```c
talloc_free(agent->pending_thinking_text);
agent->pending_thinking_text = NULL;
talloc_free(agent->pending_thinking_signature);
agent->pending_thinking_signature = NULL;
talloc_free(agent->pending_redacted_data);
agent->pending_redacted_data = NULL;
```

## 10. Database Persistence (PHASE 2)

**Problem:** Thinking blocks must survive process restart.

### Storage Strategy

Store thinking data in the `data_json` field of tool_call messages:

```json
{
  "tool_call_id": "toolu_01...",
  "tool_name": "bash",
  "tool_args": "{...}",
  "thinking": {
    "text": "Let me solve...",
    "signature": "EqQBCgIYAhIM..."
  }
}
```

Or for redacted thinking:

```json
{
  "tool_call_id": "toolu_01...",
  "tool_name": "bash",
  "tool_args": "{...}",
  "redacted_thinking": {
    "data": "EmwKAhgBEgy..."
  }
}
```

### Database Insert

**File:** `src/repl_tool.c`

Update `ik_db_message_insert_` call to include thinking data in JSON:

```c
char *data_json = build_tool_call_data_json(agent, tc);
ik_db_message_insert_(..., "tool_call", formatted_call, data_json);
```

### Message Restoration

**File:** `src/message.c`

In `ik_message_from_db_msg`, parse `data_json` for tool_call messages and reconstruct thinking blocks if present.

## Task Dependencies (Updated)

```
PHASE 1 (COMPLETE):
types ─────────────────┬──> content-block-api ──> deep-copy
                       │
streaming-capture ─────┴──> response-builder
                       │
                       └──> serialization

PHASE 2 (NEW):
agent-context ──> extract-thinking ──> tool-call-message
                                    │
                                    └──> db-persistence ──> db-restore
```

Phase 1 tasks are complete. Phase 2 tasks address the in-memory and database integration.
