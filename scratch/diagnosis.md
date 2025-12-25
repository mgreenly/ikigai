# Orchestration Failure: Root Cause Analysis

## Executive Summary

The orchestration stopped on `tests-openai-streaming.md` due to an **incomplete implementation** of the prerequisite task `openai-streaming-chat.md`. The task specification required full async vtable integration, but only the SSE parser was implemented, leaving the `openai_start_stream()` vtable method as a stub.

**Impact:** Test task cannot proceed because it depends on async HTTP functionality that was never implemented.

---

## The Dependency Chain

```
openai-streaming-chat.md (completed ✓, but incomplete implementation ✗)
  ↓
  Created: SSE parsing logic (streaming_chat.c)
  Missing: Vtable integration (openai_start_stream stub)
  ↓
tests-openai-streaming.md (BLOCKED ✗)
  ↓
  Requires: Async fdset/perform/info_read pattern
  Cannot test: vtable methods that don't exist
```

---

## What Was Implemented

**Commit:** 51133c3 - "task: openai-streaming-chat.md - Implement async streaming for OpenAI Chat Completions API"

**Files created:**
1. `src/providers/openai/streaming.h` (96 lines)
   - Declares `ik_openai_chat_stream_ctx_t` type
   - Declares `ik_openai_chat_stream_ctx_create()` factory
   - Declares `ik_openai_chat_stream_process_data()` - SSE data processor
   - Declares getters for usage and finish_reason

2. `src/providers/openai/streaming_chat.c` (413 lines)
   - Implements SSE parser that processes data events
   - Emits normalized `ik_stream_event_t` events
   - Handles tool call state machine
   - Extracts usage and finish_reason metadata
   - **Self-contained parser - no HTTP layer integration**

3. `Makefile` - Added streaming_chat.c to build

**What works:**
- ✓ SSE parsing logic (data-only format)
- ✓ Event normalization (TEXT_DELTA, TOOL_CALL_*, DONE, ERROR)
- ✓ Tool call state tracking
- ✓ Usage extraction
- ✓ Can be called directly with SSE data strings (unit testable)

---

## What Was NOT Implemented

**File:** `src/providers/openai/openai.c`

**Current state (lines 384-406):**
```c
static res_t openai_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb,
                                  void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);     // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    // Stub: Will be implemented in openai-streaming.md
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_stream not yet implemented");
    talloc_free(tmp);
    return result;
}
```

**Missing functionality:**
1. ✗ HTTP request serialization for streaming (with `"stream": true` in JSON)
2. ✗ Curl multi handle integration (`ik_http_multi_add_request`)
3. ✗ SSE write callback setup
4. ✗ Stream context lifecycle management
5. ✗ Async request lifecycle (start_stream → perform → info_read → completion_cb)

**Comment in stub:** "Will be implemented in openai-streaming.md"
- This is a **different task** than what was actually implemented
- Suggests the work was intentionally split but never completed

---

## What the Task Specification Required

**File:** `scratch/tasks/done/openai-streaming-chat.md`

**Section "Async Flow (Vtable Integration)" (lines 93-108):**

> The streaming interface follows the vtable async pattern:
>
> ```c
> res_t (*start_stream)(void *ctx, const ik_request_t *req,
>                       ik_stream_cb_t stream_cb, void *stream_ctx,
>                       ik_provider_completion_cb_t completion_cb, void *completion_ctx);
> ```
>
> **Callback invocation points:**
> - `stream_cb` - invoked from curl write callback during `perform()` as SSE events arrive
> - `completion_cb` - invoked from `info_read()` when HTTP transfer completes

**Section "Async Event Flow" (lines 165-176):**

> 1. Caller invokes `start_stream()` (non-blocking)
> 2. Request added to curl_multi handle
> 3. `start_stream()` returns immediately
> 4. REPL event loop runs: fdset() -> select() -> perform() -> info_read()
> 5. During `perform()`, curl invokes write callback as data arrives
> 6. Write callback feeds data to SSE parser
> 7. SSE parser invokes `ik_openai_chat_stream_process_data()` for each event
> 8. `process_data()` invokes user's `stream_cb` with normalized `ik_stream_event_t`
> 9. When transfer completes, `info_read()` invokes `completion_cb`

**Postcondition (line 342):**
> - [ ] Events delivered to user callback during `perform()` (not blocking)

**The spec clearly required vtable integration, NOT just a parser.**

---

## Why Tests Can't Proceed

**Test file:** `tests/unit/providers/openai/openai_streaming_test.c` (exists, 18 tests)

**Current state:**
- ✓ Tests the parser directly via `ik_openai_chat_stream_process_data()`
- ✓ All 18 tests pass
- ✓ Covers SSE parsing scenarios (text deltas, tool calls, errors, usage)

**What test task requires:** `scratch/tasks/tests-openai-streaming.md`

**Lines 69-98 - "Async Test Pattern":**
```c
// Start async stream (returns immediately)
r = provider->vt->start_stream(provider->ctx, req, stream_cb, &events,
                                completion_cb, completion_ctx);

// Simulate event loop - each perform() delivers some SSE events
while (running > 0) {
    provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    provider->vt->perform(provider->ctx, &running);
}
provider->vt->info_read(provider->ctx, logger);
```

**Lines 177-188 - Test scenarios required:**
```
**Async Event Loop Tests (3 tests):**
- start_stream() returns immediately without blocking
- fdset() returns mock FDs correctly
- perform() + info_read() cycle completes stream
```

**The gap:**
- Test task expects to call `provider->vt->start_stream()`
- Current stub returns `ERR(NOT_IMPLEMENTED)`
- Cannot test async integration without the integration existing

---

## Why Task Was Marked Complete

**Evidence from git log:**
```
commit 51133c3
Author: ai4mgreenly <ai4mgreenly@logic-refinery.com>
Date:   Wed Dec 24 14:06:59 2025 -0600

    task: openai-streaming-chat.md - Implement async streaming for OpenAI Chat Completions API

    Add OpenAI Chat Completions streaming support with async pattern that integrates
    with select()-based event loop. Implements data-only SSE parsing with normalized
    stream events.

    ...

    make check passed - all tests pass
```

**Why it passed:**
- The task created *parser* code that compiles
- No tests existed yet (test task hadn't run)
- `make check` passed because there were no tests to fail
- Commit message says "async streaming" but only parser was delivered

**The issue:**
- Task was marked complete based on "compiles + no test failures"
- But precondition checking wasn't rigorous enough
- Stub implementation went undetected

---

## Comparison: Similar Successful Task

**For reference:** `openai_start_request()` in same file (lines 285-382)

**Full implementation:**
1. Determines which API to use (chat vs responses)
2. Creates request context to track lifecycle
3. Serializes request to JSON with `ik_openai_serialize_chat_request()`
4. Builds URL with `ik_openai_build_chat_url()`
5. Builds headers with `ik_openai_build_headers()`
6. Constructs `ik_http_request_t` struct
7. Adds to multi handle: `ik_http_multi_add_request(multi, &http_req, NULL, NULL, http_completion_handler, req_ctx)`
8. Returns OK - request started asynchronously

**This is the pattern `openai_start_stream()` should follow.**

---

## The Fix

### Required Implementation

**File:** `src/providers/openai/openai.c`
**Function:** `openai_start_stream()` (lines 384-406)

**Steps to implement (mirroring `openai_start_request` pattern):**

1. **Determine API mode:**
   ```c
   bool use_responses_api = impl_ctx->use_responses_api
       || ik_openai_prefer_responses_api(req->model);
   ```

2. **Create streaming-specific request context:**
   ```c
   ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(impl_ctx, ...);
   req_ctx->provider = impl_ctx;
   req_ctx->use_responses_api = use_responses_api;
   req_ctx->stream_cb = stream_cb;
   req_ctx->stream_ctx = stream_ctx;
   req_ctx->completion_cb = completion_cb;
   req_ctx->completion_ctx = completion_ctx;
   ```

3. **Serialize request with streaming=true:**
   ```c
   char *json_body = NULL;
   res_t serialize_res;

   if (use_responses_api) {
       serialize_res = ik_openai_serialize_responses_request(req_ctx, req, true, &json_body);
   } else {
       serialize_res = ik_openai_serialize_chat_request(req_ctx, req, true, &json_body);
   }
   ```
   Note: Second-to-last parameter is `stream` flag (true for streaming)

4. **Build URL and headers** (same as non-streaming)

5. **Create SSE write callback wrapper:**
   ```c
   // Create streaming context
   ik_openai_chat_stream_ctx_t *stream_ctx_internal =
       ik_openai_chat_stream_ctx_create(req_ctx, stream_cb, stream_ctx);

   // Create write callback that feeds SSE parser
   size_t stream_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
       ik_openai_chat_stream_ctx_t *sctx = userdata;
       // Parse SSE chunks and extract data lines
       // Call ik_openai_chat_stream_process_data() for each data event
       return size * nmemb;
   }
   ```

6. **Add streaming request to multi handle:**
   ```c
   ik_http_request_t http_req = {
       .url = url,
       .method = "POST",
       .headers = headers_const,
       .body = json_body,
       .body_len = strlen(json_body)
   };

   res_t add_res = ik_http_multi_add_request(
       impl_ctx->http_multi,
       &http_req,
       stream_write_callback,      // Write callback for SSE data
       stream_ctx_internal,        // Context for write callback
       stream_completion_handler,  // Completion callback
       req_ctx);                   // Context for completion
   ```

7. **Return immediately** (non-blocking)

### Additional Files Needed

**New file:** `src/providers/openai/streaming_http.c`
- SSE write callback implementation
- Completion callback for streaming requests
- SSE line buffering and parsing

**Or extend:** `src/providers/openai/streaming_chat.c`
- Add HTTP integration functions
- Keep pure parser separate from HTTP layer

### Integration Points

**Existing infrastructure (already works):**
- ✓ `ik_http_multi_t` - Multi-handle manager
- ✓ `ik_http_multi_add_request()` - Add async request
- ✓ `openai_fdset()`, `openai_perform()`, `openai_timeout()`, `openai_info_read()` - Already delegate to http_multi
- ✓ `ik_openai_serialize_chat_request()` - Accepts `stream` parameter

**Just needs wiring:**
- Create stream context
- Create write callback
- Pass to ik_http_multi_add_request()

---

## Proposed Fix Strategy

### Option A: Complete the Original Task (Recommended)

**Action:** Implement `openai_start_stream()` to match the original task specification

**Steps:**
1. Read `src/providers/openai/openai.c` - study `openai_start_request()` pattern
2. Read `src/providers/openai/streaming_chat.c` - understand parser interface
3. Read `src/providers/common/http_multi.h` - understand async request API
4. Implement `openai_start_stream()` following the non-streaming pattern
5. Create SSE write callback that bridges HTTP chunks to parser
6. Test with existing `tests/unit/providers/openai/openai_streaming_test.c`
7. Add 3 async integration tests (start_stream, fdset, perform+info_read)
8. Verify `make check` passes
9. Commit with message: "fix: Complete openai_start_stream vtable integration"

**Estimated complexity:** Medium (100-150 lines of glue code)

**Risk:** Low - follows existing pattern, infrastructure exists

### Option B: Split Into Subtasks

**Action:** Create intermediate task to complete vtable integration

**New task:** `scratch/tasks/openai-vtable-streaming.md`
- Depends on: openai-streaming-chat.md (done)
- Implements: `openai_start_stream()` vtable method
- Wires: SSE parser to HTTP multi layer
- Tests: Basic async smoke test

Then update `tests-openai-streaming.md`:
- Depends on: openai-vtable-streaming.md
- Proceeds with full test suite

**Estimated complexity:** Same work, more orchestration overhead

**Risk:** Medium - adds another dependency that could fail

### Option C: Mark Test Task as "Requires Manual Implementation"

**Action:** Skip the test task, continue with remaining tasks, circle back later

**Steps:**
1. Move tests-openai-streaming.md to `scratch/tasks/deferred/`
2. Continue orchestration with remaining 5 tasks
3. Manually implement vtable integration after orchestration completes
4. Re-run test task

**Estimated complexity:** Low immediate effort, defers the fix

**Risk:** High - Deferred tasks rarely get completed

---

## Recommendation

**Implement Option A immediately.**

**Rationale:**
1. The work is well-scoped (follow existing `openai_start_request` pattern)
2. All infrastructure exists (http_multi, serializers, parser)
3. Blocking 5 downstream tasks that may depend on streaming
4. Demonstrates orchestration can recover from incomplete prerequisites
5. Test file already exists with 18 parser tests - just needs 3 async tests added

**Implementation plan:**
1. Create `streaming_http.c` with write callback and completion handler
2. Implement `openai_start_stream()` in `openai.c`
3. Add to Makefile
4. Add 3 async tests to `openai_streaming_test.c`
5. Run `make check`
6. Commit and continue orchestration

**Time estimate:** 2-3 hours for a developer familiar with the codebase

---

## Lessons Learned

### For Task Authoring

1. **Postconditions must be testable**
   - "Events delivered during perform()" is good
   - But how to verify without downstream tests?
   - Add smoke test requirement in task itself

2. **Stub detection**
   - Task should grep for ERR(NOT_IMPLEMENTED) in relevant files
   - Fail task if expected functions are still stubs
   - Postcondition: "No NOT_IMPLEMENTED errors in `src/providers/openai/openai.c`"

3. **Dependency verification**
   - Test task should check preconditions before executing
   - Verify `openai_start_stream()` doesn't return NOT_IMPLEMENTED
   - Fail fast with clear error message

### For Orchestration

1. **Completion criteria too loose**
   - "make check passes" when no tests exist = false positive
   - Need: "make check passes AND coverage increased"
   - Or: "No function stubs in modified files"

2. **Task splitting requires explicit contracts**
   - If "openai-streaming-chat.md" only implements parser
   - And "openai-streaming.md" implements vtable integration
   - Task specification must clearly state the boundary
   - And test task must depend on BOTH

3. **Escalation didn't help**
   - All 4 escalation levels hit the same blocker
   - No amount of smarter agents can implement missing code
   - Need better precondition checking before task starts

---

## Next Steps

1. **Immediate:** Implement `openai_start_stream()` per Option A
2. **Verify:** Run `make check` - all tests should pass
3. **Resume:** Continue orchestration from tests-openai-streaming.md
4. **Improve:** Update task templates with stub detection postconditions
