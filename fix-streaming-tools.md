# Implement: Streaming Tool Call Support

## Problem

The new multi-provider interface is missing tool call support for streaming mode. All providers (OpenAI, Anthropic, Google) stream responses correctly for text, but tool calls are not propagated to the REPL.

**Symptoms:**
- Token usage is displayed (e.g., "Tokens: 298 in + 24 out = 322")
- No assistant response shown
- No tool execution occurs
- Affects all providers (OpenAI, Anthropic, Google)

**Note:** This is initial implementation work. The multi-provider interface is new and streaming tool calls have never worked.

## Root Cause

In `src/repl_callbacks.c`, the stream callback ignores tool call events:

```c
// Lines 117-121
case IK_STREAM_TOOL_CALL_START:
case IK_STREAM_TOOL_CALL_DELTA:
case IK_STREAM_TOOL_CALL_DONE:
    // Tool call handling not implemented yet
    break;
```

The completion callback has two paths:

```c
// Lines 273-276: Streaming path (response == NULL)
if (completion->success && completion->response == NULL) {
    render_usage_event(agent);
    // extract_tool_calls() NOT called - tool calls lost!
}

// Lines 291-294: Non-streaming path (response != NULL)
if (completion->success && completion->response != NULL) {
    store_response_metadata(agent, completion->response);
    render_usage_event(agent);
    extract_tool_calls(agent, completion->response);  // Only here!
}
```

For streaming requests, `completion->response` is NULL, so `extract_tool_calls()` is never called and `agent->pending_tool_call` remains NULL.

In `src/repl_event_handlers.c:229-231`, tool execution depends on this field:

```c
if (agent->pending_tool_call != NULL) {
    ik_agent_start_tool_execution_(agent);
    return;
}
```

## Data Flow

Streaming events provide all necessary data:

| Event | Data Available |
|-------|----------------|
| `IK_STREAM_TOOL_CALL_START` | `id`, `name` |
| `IK_STREAM_TOOL_CALL_DELTA` | `arguments` (JSON fragment) |
| `IK_STREAM_TOOL_CALL_DONE` | (signals completion) |
| `IK_STREAM_DONE` | `finish_reason` (IK_FINISH_TOOL_USE) |

The provider streaming code (e.g., `src/providers/openai/streaming_chat_delta.c`) already accumulates tool call data internally, but this data is not captured at the REPL level.

## Proposed Fix

### Option A: Duplicate Accumulation at REPL Level (Quick Fix)

Add streaming tool call accumulation fields to `ik_agent_ctx_t` and handle the streaming events in `repl_callbacks.c` to re-accumulate the same data the provider already accumulated.

**Pros:**
- Quick to implement
- Only touches REPL code

**Cons:**
- Duplicates work - provider already accumulates this data
- Two places maintaining the same accumulation logic
- Error-prone - logic must stay in sync
- Leaky abstraction - REPL knows too much about streaming internals

### Option B: Provider Builds Response from Accumulated Data (Correct Fix)

The provider streaming contexts already accumulate all tool call data. For example, `ik_openai_chat_stream_ctx_t` has:

```c
char *current_tool_id;             /* Current tool call ID */
char *current_tool_name;           /* Current tool call name */
char *current_tool_args;           /* Accumulated tool arguments */
ik_finish_reason_t finish_reason;  /* Finish reason from choice */
ik_usage_t usage;                  /* Accumulated usage statistics */
```

Currently, when streaming completes, the provider discards this and sets `completion->response = NULL`:

```c
// src/providers/openai/openai_handlers.c:237-244
// Success - stream events were already delivered during perform()
ik_provider_completion_t provider_completion = {0};
provider_completion.success = true;
provider_completion.response = NULL;  // <-- Data discarded!
```

**The fix:** Before calling the completion callback, the provider should build an `ik_response_t` from its accumulated streaming data:

```c
// Build response from accumulated streaming data
ik_response_t *response = build_response_from_stream_ctx(sctx);
provider_completion.response = response;  // Now REPL has the data
```

**Pros:**
- Single source of truth - provider accumulates, provider builds response
- REPL code stays simple - always uses `completion->response`
- No duplicate accumulation logic
- Clean abstraction - streaming is an implementation detail
- Unified code path for streaming and non-streaming

**Cons:**
- Requires changes to all three providers (OpenAI, Anthropic, Google)
- Slightly more upfront work

### Recommendation: Option B

Option B is the correct long-term solution. The provider layer's job is to abstract away API differences and present a uniform interface. Whether the response came from streaming or a single JSON response should be invisible to the REPL.

## Implementation Gaps

### Tool Argument Accumulation

The streaming contexts were designed to emit events but not all accumulate the data needed to build a final response. Current state:

| Provider | Has `current_tool_id` | Has `current_tool_name` | Has `current_tool_args` |
|----------|----------------------|------------------------|------------------------|
| OpenAI   | Yes                  | Yes                    | **Yes**                |
| Anthropic| Yes                  | Yes                    | **No**                 |
| Google   | Yes                  | Yes                    | **No**                 |

Anthropic and Google emit `IK_STREAM_TOOL_CALL_DELTA` events but don't accumulate the arguments.
This must be fixed before we can build responses from streaming data.

## Task Breakdown

### Phase 1: Complete Anthropic Tool Argument Accumulation

**Task 1.1: Add accumulation to Anthropic streaming context**

Pre-read:
- `src/providers/anthropic/streaming.h` - context structure
- `src/providers/anthropic/streaming_events.c` - where DELTA is emitted (line 192-198)
- `src/providers/openai/streaming_chat_delta.c:166-173` - reference for accumulation pattern

Changes:
- Add `char *current_tool_args;` field to `ik_anthropic_stream_ctx_t`
- Initialize to NULL in `ik_anthropic_stream_ctx_create`
- Accumulate in DELTA handling (like OpenAI does)
- Clear on tool call end

**Task 1.2: Add unit tests for Anthropic argument accumulation**

Reference:
- `tests/unit/providers/anthropic/` - existing test patterns

---

### Phase 2: Complete Google Tool Argument Accumulation

**Task 2.1: Add accumulation to Google streaming context**

Pre-read:
- `src/providers/google/streaming.c:17-29` - context structure
- `src/providers/google/streaming.c:159-168` - where DELTA is emitted
- `src/providers/openai/streaming_chat_delta.c:166-173` - reference for accumulation pattern

Changes:
- Add `char *current_tool_args;` field to `ik_google_stream_ctx`
- Initialize to empty string "" when tool call starts (line 129)
- Accumulate in DELTA handling (line 161)
- Already cleared on tool call end (line 49-50 clears id/name, add args)

**Task 2.2: Add unit tests for Google argument accumulation**

Reference:
- `tests/unit/providers/google/` - existing test patterns

---

### Phase 3: OpenAI Response Building

**Task 3.1: Add response builder function**

Pre-read:
- `src/providers/openai/streaming_chat_internal.h` - streaming context (already has all data)
- `src/providers/openai/response_chat.c` - reference for building `ik_response_t`
- `src/providers/provider.h` - `ik_response_t`, `ik_content_block_t` structures

New function in `src/providers/openai/streaming_chat.c`:
```c
ik_response_t *ik_openai_chat_stream_build_response(TALLOC_CTX *ctx,
                                                      ik_openai_chat_stream_ctx_t *sctx);
```

Must populate:
- `response->model` from `sctx->model`
- `response->finish_reason` from `sctx->finish_reason`
- `response->usage` from `sctx->usage`
- `response->content_blocks` with tool call if present (from `current_tool_*`)

**Task 3.2: Call response builder in completion handler**

Pre-read:
- `src/providers/openai/openai_handlers.c:237-247` - where completion is called

Change line 240 from:
```c
provider_completion.response = NULL;
```
To:
```c
provider_completion.response = ik_openai_chat_stream_build_response(req_ctx, sctx);
```

**Task 3.3: Add unit tests for OpenAI response building**

---

### Phase 4: Anthropic Response Building

**Task 4.1: Add response builder function**

Pre-read:
- `src/providers/anthropic/streaming.h` - streaming context
- `src/providers/anthropic/response.c` - reference for building `ik_response_t`

New function:
```c
ik_response_t *ik_anthropic_stream_build_response(TALLOC_CTX *ctx,
                                                   ik_anthropic_stream_ctx_t *sctx);
```

**Task 4.2: Call response builder in completion handler**

Pre-read:
- `src/providers/anthropic/anthropic.c:240-242` - where completion is called

**Task 4.3: Add unit tests for Anthropic response building**

---

### Phase 5: Google Response Building

**Task 5.1: Add response builder function**

Pre-read:
- `src/providers/google/streaming.c:17-29` - streaming context
- `src/providers/google/response.c` - reference for building `ik_response_t`

New function:
```c
ik_response_t *ik_google_stream_build_response(TALLOC_CTX *ctx,
                                                 ik_google_stream_ctx_t *sctx);
```

**Task 5.2: Call response builder in completion handler**

Pre-read:
- `src/providers/google/google.c:231-233` - where completion is called

**Task 5.3: Add unit tests for Google response building**

---

### Phase 6: REPL Cleanup

**Task 6.1: Remove streaming-specific placeholder code**

Pre-read:
- `src/repl_callbacks.c:273-276` - streaming path (no longer needed)
- `src/repl_callbacks.c:291-295` - response handling (now handles both cases)

After implementation, `completion->response` is always non-NULL for successful requests.

Remove lines 273-276:
```c
// For streaming (response == NULL), render usage from stored token counts
if (completion->success && completion->response == NULL) {
    render_usage_event(agent);
}
```

The remaining code (291-295) handles both streaming and non-streaming correctly.

**Task 6.2: Update/add REPL tests**

Verify tool extraction works for streaming responses.

---

## Code to Remove

After implementation, the following placeholder code can be removed:

| File | Lines | Description |
|------|-------|-------------|
| `src/repl_callbacks.c` | 273-276 | Streaming-specific usage rendering (response will always be populated) |
| `src/repl_callbacks.c` | 117-121 | Tool call event handlers (no-ops, can remain or be removed) |

## Testing Strategy

1. **Unit tests per phase**: Each phase adds tests for the new functionality
2. **Integration test**: End-to-end test with mocked streaming that returns tool calls
3. **Manual verification**: "list files in current directory" should trigger glob tool

## Shared Knowledge for All Subagents

Pre-read these files to understand the architecture:
- `src/providers/provider.h` - Core types: `ik_response_t`, `ik_content_block_t`, `ik_finish_reason_t`
- `src/providers/provider_stream.h` - Stream event types
- `.claude/library/errors/SKILL.md` - Error handling patterns
- `.claude/library/style/SKILL.md` - Code style conventions
