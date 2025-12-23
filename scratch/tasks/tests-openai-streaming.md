# Task: Create OpenAI Provider Async Streaming Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-streaming-chat.md, tests-openai-basic.md, vcr-core.md, vcr-mock-integration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:
- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Tests MUST verify the async fdset/perform/info_read pattern, not blocking behavior.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] VCR mock infrastructure complete (vcr-core.md, vcr-mock-integration.md)
- [ ] `openai-streaming-chat.md` complete (streaming implementation exists)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/openai/streaming.c` - Async streaming implementation
- `src/providers/common/http_multi.c` - curl_multi wrapper
- `tests/unit/providers/openai/` - Basic tests for patterns
- `tests/helpers/mock_http_multi.h` - Mock curl_multi infrastructure

**Plan:**
- `scratch/plan/streaming.md` - Normalized event types and async flow
- `scratch/plan/testing-strategy.md` - SSE streaming mock pattern

## Objective

Create tests for OpenAI async streaming event handling. Tests MUST use mock curl_multi and exercise the fdset/perform/info_read cycle. Verifies SSE parsing, delta accumulation, tool call argument streaming, and event normalization through the async pattern.

## Recording Fixtures

Before tests can run in playback mode, fixtures must be recorded from real API responses:

1. **Ensure valid credentials** - `OPENAI_API_KEY` environment variable must be set
2. **Run in record mode**:
   ```bash
   VCR_RECORD=1 make build/tests/unit/providers/openai/test_openai_streaming
   VCR_RECORD=1 ./build/tests/unit/providers/openai/test_openai_streaming
   ```
3. **Verify fixtures created** - Check `tests/fixtures/vcr/openai/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "sk-" tests/fixtures/vcr/openai/` returns nothing
5. **Commit fixtures** - Fixtures are committed to git for deterministic CI runs

**Note:** Fixtures only need re-recording when API behavior changes. Normal test runs use playback mode (VCR_RECORD unset).

## Async Test Pattern

Tests follow this flow (from `scratch/plan/testing-strategy.md`):

```c
// Setup: Load SSE fixture
const char *sse_data = load_fixture("openai/stream_basic.txt");
mock_set_streaming_response(200, sse_data);

// Start async stream (returns immediately)
r = provider->vt->start_stream(provider->ctx, req, stream_cb, &events,
                                completion_cb, completion_ctx);
assert(is_ok(&r));

// Simulate event loop - each perform() delivers some SSE events
int running = 1;
while (running > 0) {
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = 0;
    provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
    // No real select() needed in tests - mock controls timing
    provider->vt->perform(provider->ctx, &running);
    // stream_cb is invoked during perform() as data arrives
}
provider->vt->info_read(provider->ctx, logger);
// completion_cb invoked from info_read() when transfer completes

// Verify stream events were captured
assert(events.count == expected_event_count);
assert(events.items[0].type == IK_STREAM_START);
assert(events.items[events.count-1].type == IK_STREAM_DONE);
```

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/openai/test_openai_streaming.c` | Async SSE streaming response handling |

**Fixture files (VCR JSONL format):**

| File | Purpose |
|------|---------|
| `tests/fixtures/vcr/openai/stream_basic.jsonl` | SSE stream for basic completion |
| `tests/fixtures/vcr/openai/stream_tool_call.jsonl` | SSE stream with tool call |
| `tests/fixtures/vcr/openai/stream_reasoning.jsonl` | SSE stream from reasoning model |

## Normalized Event Types

Use correct event types from `scratch/plan/streaming.md`:

| Event Type | Description |
|------------|-------------|
| `IK_STREAM_START` | Stream started (model extracted) |
| `IK_STREAM_TEXT_DELTA` | Text content delta |
| `IK_STREAM_THINKING_DELTA` | Reasoning/thinking delta |
| `IK_STREAM_TOOL_CALL_START` | Tool call started (id, name) |
| `IK_STREAM_TOOL_CALL_DELTA` | Tool call arguments delta |
| `IK_STREAM_TOOL_CALL_DONE` | Tool call completed |
| `IK_STREAM_DONE` | Stream completed (finish_reason, usage) |
| `IK_STREAM_ERROR` | Error occurred |

## SSE Format Examples

**Basic streaming (Chat Completions API):**
```
data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{"content":" world"},"finish_reason":null}]}

data: {"id":"chatcmpl-...","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]
```

**Tool call streaming:**
```
data: {"choices":[{"delta":{"tool_calls":[{"index":0,"id":"call_abc","type":"function","function":{"name":"get_weather","arguments":""}}]}}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"{\"lo"}}]}}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"cation\":"}}]}}]}

data: {"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"arguments":"\"NYC\"}"}}]}}]}

data: {"choices":[{"delta":{},"finish_reason":"tool_calls"}]}

data: [DONE]
```

## Delta Types to Handle

| Delta Content | Normalized Event |
|---------------|------------------|
| `content` string | `IK_STREAM_TEXT_DELTA` |
| `tool_calls` with id/name | `IK_STREAM_TOOL_CALL_START` |
| `tool_calls` with arguments | `IK_STREAM_TOOL_CALL_DELTA` |
| `finish_reason` set | `IK_STREAM_DONE` |
| `[DONE]` marker | Stream end signal (no event) |

## Test Scenarios

**Async Event Loop Tests (3 tests):**
- `start_stream()` returns immediately without blocking
- `fdset()` returns mock FDs correctly
- `perform()` + `info_read()` cycle completes stream

**Basic Streaming (4 tests):**
- Parse initial role delta to `IK_STREAM_START`
- Parse content delta to `IK_STREAM_TEXT_DELTA`
- Parse finish_reason delta to `IK_STREAM_DONE`
- Handle `[DONE]` marker gracefully

**Content Accumulation (3 tests):**
- Accumulate multiple text deltas across perform() calls
- Handle empty content deltas
- Preserve text order across event loop iterations

**Tool Call Streaming (5 tests):**
- Parse tool_calls delta with id and name to `IK_STREAM_TOOL_CALL_START`
- Parse tool_calls delta with partial arguments to `IK_STREAM_TOOL_CALL_DELTA`
- Accumulate arguments across multiple deltas/perform() calls
- Handle multiple tool calls (by index)
- Emit `IK_STREAM_TOOL_CALL_DONE` when complete

**Event Normalization (3 tests):**
- Normalize content delta to `IK_STREAM_TEXT_DELTA`
- Normalize tool_calls delta to `IK_STREAM_TOOL_CALL_DELTA`
- Normalize finish_reason to `IK_STREAM_DONE`

**Error Handling (3 tests):**
- Handle malformed SSE data via `IK_STREAM_ERROR`
- Handle stream interruption (incomplete stream)
- Handle HTTP error during stream

## Test Helper Structure

```c
// Event collection for verification
typedef struct {
    ik_stream_event_t *items;
    size_t count;
    size_t capacity;
} event_array_t;

// Stream callback captures events
static res_t stream_cb(const ik_stream_event_t *event, void *ctx) {
    event_array_t *events = ctx;
    // Copy event to array for later verification
    return OK(NULL);
}

// Completion callback signals done
static bool g_stream_complete;
static res_t g_stream_result;

static res_t completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    g_stream_complete = true;
    g_stream_result = completion->result;
    return OK(NULL);
}
```

## Postconditions

- [ ] 1 test file with 21+ tests
- [ ] 3 fixture files recorded with VCR_RECORD=1 (JSONL format)
- [ ] No API keys in fixtures (verify: `grep -r "sk-" tests/fixtures/vcr/openai/` returns empty)
- [ ] All tests use async fdset/perform/info_read pattern
- [ ] Tool call argument accumulation verified
- [ ] Event normalization verified
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-openai-streaming.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
