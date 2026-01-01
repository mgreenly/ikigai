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

---

## Post-Implementation Fix: Tool Data Cleared Too Early

### Problem Discovered

After implementing all 6 phases, manual testing revealed that streaming tool calls still weren't working. Debug output showed:
- `TOOL_CALL_START`, `TOOL_CALL_DELTA`, `TOOL_CALL_DONE` events were being emitted correctly
- But completion showed `content_count=0, pending_tool=NO`
- `finish_reason` was correctly set to `IK_FINISH_TOOL_USE` (2)

### Root Cause

The streaming contexts were correctly accumulating tool call data (`current_tool_id`, `current_tool_name`, `current_tool_args`), but then **clearing that data** when the tool call block ended - **before** the response builder ran.

**Anthropic** (`src/providers/anthropic/streaming_events.c`):
```c
// In ik_anthropic_process_content_block_stop() - lines 235-241
// This ran when content_block_stop event arrived, BEFORE response.completed
talloc_free(sctx->current_tool_id);
sctx->current_tool_id = NULL;
talloc_free(sctx->current_tool_name);
sctx->current_tool_name = NULL;
talloc_free(sctx->current_tool_args);
sctx->current_tool_args = NULL;
```

**OpenAI Responses API** (`src/providers/openai/streaming_responses_events.c`):
```c
// In response.output_item.done handler - lines 281-282
stream_ctx->current_tool_id = NULL;
stream_ctx->current_tool_name = NULL;
```

### Timeline

1. Tool call starts → data accumulated in streaming context ✓
2. Tool call completes → `TOOL_CALL_DONE` emitted, **data cleared** ✗
3. Stream ends → response builder called, finds NULL data ✗
4. Completion callback → `content_count=0`, no tool execution

### Fix Applied

Removed the premature clearing of tool data. The data persists in the streaming context until the response is built and the context is freed.

**Files modified:**
- `src/providers/anthropic/streaming_events.c` - Removed lines 235-241
- `src/providers/openai/streaming_responses_events.c` - Removed lines 281-282

**Tests updated:**
- `tests/unit/providers/anthropic/streaming_tool_accum_test.c` - Changed `test_args_cleared_on_tool_stop` to expect data NOT cleared

### Lesson Learned

When implementing streaming response building, the accumulated data must persist until the final response is constructed. Don't clear intermediate state in "done" event handlers - let the context cleanup handle it.

---

## Post-Implementation Fix #2: OpenAI Responses API Missing Infrastructure

### Problem Discovered

After fixing the premature clearing issue, OpenAI models (gpt-5, etc.) still weren't executing tool calls. Testing showed that Anthropic (Sonnet) worked correctly after the first fix, but OpenAI streaming was completely broken.

### Root Cause

OpenAI has **two different streaming APIs**:
1. **Chat Completions API** (`/v1/chat/completions`) - older API, simpler SSE format
2. **Responses API** (`/v1/responses`) - newer API for models with thinking, different SSE format

The original Phase 1-6 implementation only added tool accumulation and response building for the **Chat Completions API**. Models using the Responses API (detected via `ik_openai_prefer_responses_api()`) were:

1. **Using the wrong context type**: `openai.c:start_stream()` always created `ik_openai_chat_stream_ctx_t`, even for Responses API
2. **Using the wrong parser**: `ik_openai_stream_write_callback()` always called `ik_openai_chat_stream_process_data()`, which couldn't parse Responses API SSE format
3. **Missing argument accumulation**: `ik_openai_responses_stream_ctx_t` had no `current_tool_args` field
4. **Missing response builder**: No `ik_openai_responses_stream_build_response()` function existed

### Fix Applied

**1. Added `current_tool_args` to Responses API context:**
- `streaming_responses_internal.h`: Added `char *current_tool_args;` field
- `streaming_responses.c`: Initialize to NULL in context creation

**2. Added argument accumulation in delta handler:**
- `streaming_responses_events.c`: Accumulate in `response.function_call_arguments.delta` handler

**3. Added response builder for Responses API:**
- `streaming_responses.c`: Added `ik_openai_responses_stream_build_response()`
- `streaming.h`: Declared the new function

**4. Fixed handler struct to support both contexts:**
- `openai_handlers.h`: Changed `parser_ctx` from `ik_openai_chat_stream_ctx_t*` to `void*`

**5. Fixed context creation in start_stream:**
- `openai.c`: Check `use_responses_api` and create correct context type

**6. Fixed write callback to route correctly:**
- `openai_handlers.c`: Check `use_responses_api` and delegate to `ik_openai_responses_stream_write_callback()` for Responses API

**7. Fixed completion handler to use correct response builder:**
- `openai_handlers.c`: Check `use_responses_api` and call `ik_openai_responses_stream_build_response()` for Responses API

### Files Modified

| File | Change |
|------|--------|
| `streaming_responses_internal.h` | Added `current_tool_args` field |
| `streaming_responses.c` | Initialize field, added response builder |
| `streaming_responses_events.c` | Accumulate arguments in delta handler |
| `streaming.h` | Declared response builder |
| `openai_handlers.h` | Changed `parser_ctx` to `void*` |
| `openai.c` | Create correct context based on API type |
| `openai_handlers.c` | Route write callback and response builder by API type |

### Lesson Learned

When a provider supports multiple API variants, ensure that:
1. Each variant has its own streaming context with all required fields
2. Each variant has its own response builder
3. Routing logic correctly dispatches to the right implementation based on which API is in use
4. Test both API paths, not just the one you're primarily developing against

---

## Post-Implementation Fix #3: Tool Loop Continuation String Mismatch

### Problem Discovered

After fixes #1 and #2, manual testing showed that tool calls were being extracted correctly, but the model's response after tool execution wasn't being displayed. The tool loop wasn't continuing.

### Root Cause

String mismatch between stored finish reason and what the continuation logic checked for:

**In `repl_callbacks.c:191` (store_response_metadata):**
```c
case IK_FINISH_TOOL_USE: finish_reason_str = "tool_use"; break;
```

**In `repl.c:196` (ik_agent_should_continue_tool_loop):**
```c
if (strcmp(agent->response_finish_reason, "tool_calls") != 0) {
    return false;  // Always returned false!
}
```

The stored value was `"tool_use"` (our normalized internal value) but the check looked for `"tool_calls"` (OpenAI's raw API value). This caused the tool loop to never continue after tool execution.

### Fix Applied

Changed `repl.c:196` to check for `"tool_use"` instead of `"tool_calls"`.

**Files modified:**
- `src/repl.c:196` - Changed comparison from `"tool_calls"` to `"tool_use"`

**Tests updated:**
- `tests/unit/repl/repl_tool_completion_test.c` - Updated test to use `"tool_use"`
- `tests/unit/repl/repl_tool_loop_continuation_test.c` - Updated test to use `"tool_use"`
- `tests/unit/repl/tool_loop_limit_test.c` - Updated all occurrences to use `"tool_use"`

### Lesson Learned

When normalizing API values (like finish_reason), ensure all code paths use the normalized value consistently. The normalization happens in the provider layer, so REPL-level code should always reference the normalized values, not raw API values.

---

## Post-Implementation Fix #4: Response Builders Not Setting TOOL_USE Finish Reason

### Problem Discovered

After fix #3, the tool loop still wasn't continuing because `ik_agent_should_continue_tool_loop` was checking for `finish_reason == "tool_use"`, but the actual finish_reason was `"stop"`.

### Root Cause

Different APIs handle finish reasons differently when there's a tool call:

| Provider | API | Status When Tool Call | Mapped To |
|----------|-----|----------------------|-----------|
| OpenAI | Chat Completions | `"tool_calls"` | `IK_FINISH_TOOL_USE` ✓ |
| OpenAI | Responses API | `"completed"` | `IK_FINISH_STOP` ✗ |
| Anthropic | Messages | `"tool_use"` | `IK_FINISH_TOOL_USE` ✓ |
| Google | Gemini | `"STOP"` | `IK_FINISH_STOP` ✗ |

The OpenAI Responses API and Google Gemini don't have a dedicated "tool use" status - they return "completed"/"STOP" even when the response contains tool calls. The `ik_openai_map_responses_status()` and `ik_google_map_finish_reason()` functions had no way to know a tool call was present.

### Fix Applied

Modified the streaming response builders to override finish_reason to `IK_FINISH_TOOL_USE` when the response contains a tool call:

```c
// In response builder, when tool call is present:
if (sctx->current_tool_id != NULL && sctx->current_tool_name != NULL) {
    resp->finish_reason = IK_FINISH_TOOL_USE;  // Override!
    // ... build tool call block
}
```

**Files modified:**
- `src/providers/openai/streaming_responses.c` - Override finish_reason for Responses API
- `src/providers/google/streaming.c` - Override finish_reason for Google

**Tests updated:**
- `tests/unit/providers/google/streaming_response_builder_test.c` - Updated to expect `IK_FINISH_TOOL_USE`

### Lesson Learned

When building a normalized response from provider-specific data, the response builder is the right place to enforce semantic consistency. If a tool call is present, the finish_reason should be `IK_FINISH_TOOL_USE` regardless of what the raw API returned. This keeps the REPL logic simple - it only needs to check the normalized finish_reason.

---

## Status: COMPLETE

All streaming tool call support is now working for all three providers:

| Provider | API | Status |
|----------|-----|--------|
| OpenAI | Chat Completions | ✓ Working |
| OpenAI | Responses API | ✓ Working |
| Anthropic | Messages | ✓ Working |
| Google | Gemini | ✓ Working |

### Summary of All Fixes

1. **Fix #1** (Phase 1-6): Implemented streaming tool call accumulation and response building for all providers
2. **Fix #2**: Removed premature clearing of tool data in streaming event handlers
3. **Fix #3**: Fixed string mismatch in `ik_agent_should_continue_tool_loop` (`"tool_calls"` → `"tool_use"`)
4. **Fix #4**: Added finish_reason override in response builders for OpenAI Responses API and Google (which return "completed"/"STOP" even for tool calls)

### Files Modified (Final State)

**Provider streaming response builders:**
- `src/providers/openai/streaming_responses.c` - Override finish_reason for tool calls
- `src/providers/google/streaming.c` - Override finish_reason for tool calls

**REPL tool loop logic:**
- `src/repl.c` - Check for `"tool_use"` (normalized value)

**Tests updated:**
- `tests/unit/repl/repl_tool_completion_test.c`
- `tests/unit/repl/repl_tool_loop_continuation_test.c`
- `tests/unit/repl/tool_loop_limit_test.c`
- `tests/unit/providers/google/streaming_response_builder_test.c`
