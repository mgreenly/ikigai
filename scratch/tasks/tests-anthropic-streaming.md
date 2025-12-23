# Task: Create Anthropic Provider Streaming Tests (Async)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** anthropic-streaming.md, tests-anthropic-basic.md, vcr-core.md, vcr-mock-integration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**CRITICAL ARCHITECTURE CONSTRAINT:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. Tests must verify async behavior through the fdset/perform/info_read pattern, NOT blocking calls.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] Anthropic streaming implementation exists (`src/providers/anthropic/streaming.c`)
- [ ] Mock curl_multi infrastructure exists (`tests/helpers/mock_curl_multi.h`)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load makefile` - Build system patterns

**Source:**
- `src/providers/anthropic/streaming.c` - Streaming implementation to test
- `src/providers/anthropic/adapter.c` - Vtable implementation with fdset/perform/info_read
- `src/providers/common/http_multi.h` - Async HTTP interface
- `src/providers/common/sse_parser.h` - SSE parsing interface
- `src/openai/client_multi.c` - Reference implementation (existing async pattern)
- `tests/unit/providers/anthropic/` - Basic tests for patterns
- `tests/helpers/mock_curl_multi.h` - Mock curl_multi infrastructure

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/testing-strategy.md` - SSE streaming mock pattern, async test flow
- `scratch/plan/streaming.md` - Normalized stream event types and async data flow
- `scratch/plan/architecture.md` - Event loop integration details

## Objective

Create tests for Anthropic async streaming using mock curl_multi. Tests verify:
1. The async fdset/perform/info_read pattern works correctly
2. SSE events are parsed and normalized during perform() calls
3. Stream callbacks are invoked incrementally as data arrives
4. Completion callback is invoked when transfer finishes

**This is NOT unit testing of SSE parsing in isolation.** This tests the full async streaming flow through the provider vtable interface with mocked network I/O.

## Async Test Pattern

**CRITICAL:** Tests must exercise the async event loop pattern, not call blocking functions.

```c
// Test captures events via stream callback
static ik_stream_event_t captured_events[32];
static size_t captured_count = 0;

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx) {
    captured_events[captured_count++] = *event;
    return OK(NULL);
}

static res_t captured_result;
static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    captured_result = completion->result;
    return OK(NULL);
}

START_TEST(test_basic_streaming)
{
    captured_count = 0;

    // Setup: Load SSE fixture and configure mock
    const char *sse_data = load_fixture("anthropic/stream_basic.txt");
    mock_curl_multi_set_streaming_response(200, sse_data);

    // Create provider
    res_t r = ik_provider_get_or_create(ctx, "anthropic", &provider);
    ck_assert(is_ok(&r));

    // Start async stream (returns immediately)
    r = provider->vt->start_stream(provider->ctx, req,
                                   test_stream_cb, NULL,
                                   test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    int running = 1;
    while (running > 0) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        // Note: In test, mock makes select() return immediately

        provider->vt->perform(provider->ctx, &running);
        // ↑ Mock curl_multi_perform delivers SSE chunks
        // ↑ SSE parser invokes stream_cb during this call
    }

    provider->vt->info_read(provider->ctx, logger);
    // ↑ Invokes completion_cb when transfer marked complete

    // Assert on captured events
    ck_assert(is_ok(&captured_result));
    ck_assert(captured_count > 0);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_int_eq(captured_events[captured_count-1].type, IK_STREAM_DONE);
}
END_TEST
```

## Recording Fixtures

Before tests can run in playback mode, fixtures must be recorded from real API responses:

1. **Ensure valid credentials** - `ANTHROPIC_API_KEY` environment variable must be set
2. **Run in record mode**:
   ```bash
   VCR_RECORD=1 make build/tests/unit/providers/anthropic/test_anthropic_streaming
   VCR_RECORD=1 ./build/tests/unit/providers/anthropic/test_anthropic_streaming
   ```
3. **Verify fixtures created** - Check `tests/fixtures/vcr/anthropic/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "sk-ant-" tests/fixtures/vcr/anthropic/` returns nothing
5. **Commit fixtures** - Fixtures are committed to git for deterministic CI runs

**Note:** Fixtures only need re-recording when API behavior changes. Normal test runs use playback mode (VCR_RECORD unset).

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/anthropic/test_anthropic_streaming.c` | Async streaming through provider vtable |

**Fixture files (VCR JSONL format):**

| File | Purpose |
|------|---------|
| `tests/fixtures/vcr/anthropic/stream_basic.jsonl` | SSE stream for basic completion |
| `tests/fixtures/vcr/anthropic/stream_thinking.jsonl` | SSE stream with thinking deltas |
| `tests/fixtures/vcr/anthropic/stream_tool_call.jsonl` | SSE stream with tool use |

## Mock curl_multi Requirements

The mock infrastructure must simulate curl_multi behavior:

| Mock Function | Behavior |
|---------------|----------|
| `curl_multi_fdset_()` | Returns mock FDs (can return -1 for no-wait) |
| `curl_multi_perform_()` | Delivers SSE chunks to write callback, updates running count |
| `curl_multi_info_read_()` | Returns CURLMSG_DONE when fixture exhausted |
| `curl_multi_timeout_()` | Returns 0 (immediate timeout for tests) |

**Chunk delivery pattern:** Mock perform() delivers fixture data in chunks to simulate incremental arrival. Each perform() call delivers one SSE event (event: + data: pair) to exercise the streaming callback path.

## Behaviors

**Event Types to Test:**

| Anthropic Event | Normalized Event | Callback |
|-----------------|------------------|----------|
| `message_start` | `IK_STREAM_START` | stream_cb |
| `content_block_delta` (text_delta) | `IK_STREAM_TEXT_DELTA` | stream_cb |
| `content_block_delta` (thinking_delta) | `IK_STREAM_THINKING_DELTA` | stream_cb |
| `content_block_start` (tool_use) | `IK_STREAM_TOOL_CALL_START` | stream_cb |
| `content_block_delta` (input_json_delta) | `IK_STREAM_TOOL_CALL_DELTA` | stream_cb |
| `content_block_stop` (tool_use) | `IK_STREAM_TOOL_CALL_DONE` | stream_cb |
| `message_stop` | `IK_STREAM_DONE` | stream_cb, then completion_cb |

**SSE Format (stream_basic.txt):**
```
event: message_start
data: {"type":"message_start","message":{"id":"msg_123","model":"claude-sonnet-4-5-20250929"}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"!"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":2}}

event: message_stop
data: {"type":"message_stop"}
```

## Test Scenarios

**Async Event Loop (3 tests):**
- `test_start_stream_returns_immediately` - start_stream() does not block
- `test_fdset_returns_mock_fds` - fdset() populates FD sets correctly
- `test_perform_delivers_events_incrementally` - Each perform() delivers events to stream_cb

**Basic Streaming (4 tests):**
- `test_stream_start_event` - First event is IK_STREAM_START with model
- `test_text_delta_events` - IK_STREAM_TEXT_DELTA events contain text fragments
- `test_stream_done_event` - Final event is IK_STREAM_DONE with usage
- `test_completion_callback_invoked` - completion_cb called after info_read()

**Content Accumulation (3 tests):**
- `test_multiple_text_deltas` - Multiple deltas delivered in sequence
- `test_delta_content_preserved` - Delta text matches fixture data
- `test_event_order_preserved` - Events arrive in SSE order

**Thinking Content (3 tests):**
- `test_thinking_delta_event_type` - Thinking deltas map to IK_STREAM_THINKING_DELTA
- `test_thinking_delta_content` - Thinking text extracted correctly
- `test_usage_includes_thinking_tokens` - Done event includes thinking token count

**Tool Call Streaming (4 tests):**
- `test_tool_call_start_event` - IK_STREAM_TOOL_CALL_START with id and name
- `test_tool_call_delta_events` - IK_STREAM_TOOL_CALL_DELTA with JSON fragments
- `test_tool_call_done_event` - IK_STREAM_TOOL_CALL_DONE after content_block_stop
- `test_tool_call_arguments_accumulated` - JSON fragments form valid object

**Error Handling (3 tests):**
- `test_http_error_calls_completion_cb` - HTTP 500 invokes completion_cb with error
- `test_malformed_sse_handled` - Invalid SSE data produces IK_STREAM_ERROR
- `test_incomplete_stream_detected` - Missing message_stop produces error

## Implementation Notes

1. **Use existing Check framework** - Follow `tests/unit/providers/anthropic/` patterns
2. **Mock setup in suite_setup** - Configure mock_curl_multi before tests
3. **Fixture loading** - Use `tests/fixtures/vcr/` loading pattern from existing tests
4. **Memory management** - Use talloc, free in teardown
5. **Event capture array** - Static array to capture stream events for assertions
6. **Running handle tracking** - Mock updates running count during perform()

## Postconditions

- [ ] 1 test file with 20+ tests
- [ ] 3 fixture files recorded with VCR_RECORD=1 (JSONL format)
- [ ] No API keys in fixtures (verify: `grep -r "sk-ant-" tests/fixtures/vcr/anthropic/` returns empty)
- [ ] All async patterns tested (fdset, perform, info_read)
- [ ] All stream event types tested
- [ ] Event sequence verified (START -> deltas -> DONE)
- [ ] Completion callback verification
- [ ] Error handling paths tested
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-anthropic-streaming.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).