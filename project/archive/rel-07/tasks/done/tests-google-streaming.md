# Task: Create Google Provider Streaming Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** google-streaming.md, tests-google-basic.md, vcr-core.md, vcr-mock-integration.md, vcr-fixtures-setup.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. Tests must verify async behavior via the fdset/perform/info_read pattern, not blocking calls.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] VCR mock infrastructure exists (from vcr-core.md, vcr-mock-integration.md)
- [ ] Google streaming implementation exists (from google-streaming.md)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load makefile` - Build targets and test execution

**Source:**
- `src/providers/google/streaming.c` - Streaming implementation to test
- `src/openai/client_multi.c` - Reference for async curl_multi pattern
- `tests/unit/providers/google/` - Basic tests for patterns
- `tests/helpers/mock_curl_multi.h` - Mock curl_multi infrastructure

**Plan:**
- `scratch/plan/testing-strategy.md` - "SSE Streaming Mock" section for async test flow
- `scratch/plan/streaming.md` - "Async Streaming Architecture" section for event loop integration
- `scratch/plan/provider-interface.md` - Vtable specification with fdset/perform/info_read

## Objective

Create tests for Google Gemini async streaming response handling. Tests must verify the fdset/perform/info_read cycle using mock curl_multi, ensuring stream events are delivered via callbacks during `perform()` calls. Validates JSON chunk parsing, thought part detection, and event normalization in an async context.

## Recording Fixtures

Before tests can run in playback mode, fixtures must be recorded from real API responses:

1. **Ensure valid credentials** - `GOOGLE_API_KEY` environment variable must be set
2. **Run in record mode**:
   ```bash
   VCR_RECORD=1 make build/tests/unit/providers/google/test_google_streaming
   VCR_RECORD=1 ./build/tests/unit/providers/google/test_google_streaming
   ```
3. **Verify fixtures created** - Check `tests/fixtures/vcr/google/*.jsonl` files exist
4. **Verify no credentials leaked** - `grep -r "AIza" tests/fixtures/vcr/google/` returns nothing
5. **Commit fixtures** - Fixtures are committed to git for deterministic CI runs

**Note:** Fixtures only need re-recording when API behavior changes. Normal test runs use playback mode (VCR_RECORD unset).

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/google/test_google_streaming.c` | Async streaming with mock curl_multi |

**Fixture files (VCR JSONL format):**

| File | Purpose |
|------|---------|
| `tests/fixtures/vcr/google/stream_basic.jsonl` | Streaming response for basic completion |
| `tests/fixtures/vcr/google/stream_thinking.jsonl` | Streaming response with thought parts |
| `tests/fixtures/vcr/google/stream_tool_call.jsonl` | Streaming response with function call |

## Behaviors

**Async Streaming Architecture:**

Google streaming integrates with select()-based event loop:

1. `start_stream()` returns immediately (non-blocking)
2. Test drives event loop: fdset → select → perform → info_read
3. During `perform()`, mock delivers chunks to write callback
4. Write callback invokes stream_cb with normalized events
5. `info_read()` invokes completion_cb when transfer completes

**Streaming Format:**

Gemini uses newline-delimited JSON (not SSE):
```json
{"candidates":[{"content":{"parts":[{"text":"Hello"}]}}]}
{"candidates":[{"content":{"parts":[{"text":" world"}]}}]}
{"candidates":[{"finishReason":"STOP","content":{"parts":[{"text":"!"}]}}]}
```

**With Thinking:**
```json
{"candidates":[{"content":{"parts":[{"text":"Let me think...","thought":true}]}}]}
{"candidates":[{"content":{"parts":[{"text":"The answer is 42."}]}}]}
```

**Part Types to Handle:**

| Part Type | Normalized Event | Detection |
|-----------|------------------|-----------|
| `text` (no thought) | `IK_STREAM_TEXT_DELTA` | `thought` field absent or false |
| `text` (thought=true) | `IK_STREAM_THINKING_DELTA` | `thought: true` in part |
| `functionCall` | `IK_STREAM_TOOL_CALL_START` + `IK_STREAM_TOOL_CALL_DONE` | `functionCall` field present |
| finish | `IK_STREAM_DONE` | `finishReason` field present |

**Mock curl_multi Pattern:**

Tests use MOCKABLE() wrappers to intercept curl_multi calls:

```c
// Mock functions to intercept
curl_multi_fdset_()   - Returns mock FDs (can return -1 for no FDs)
curl_multi_perform_() - Simulates progress, delivers data to write callbacks
curl_multi_info_read_() - Returns completion messages with mock responses
```

## Test Scenarios

**Async Event Loop Tests (4 tests) - REQUIRED:**
- start_stream returns immediately (non-blocking)
- fdset populates fd_sets with provider FDs
- perform delivers chunks to stream callback
- info_read invokes completion callback when done

**Basic Streaming (4 tests):**
- Parse single text part chunk via async loop
- Parse multiple text parts in one chunk
- Parse finishReason chunk (triggers IK_STREAM_DONE)
- Accumulate text across multiple perform() calls

**Thought Part Detection (4 tests):**
- Parse part with thought=true flag
- Parse part without thought flag (defaults to false)
- Distinguish thought content from regular content
- Interleaved thinking and content parts

**Function Call Streaming (3 tests):**
- Parse functionCall part (emits START + DONE together)
- Generate 22-char base64url UUID for function call ID
- Parse function arguments from functionCall.args

**Event Normalization (3 tests):**
- Normalize text part to IK_STREAM_TEXT_DELTA
- Normalize thought part to IK_STREAM_THINKING_DELTA
- Normalize finishReason to IK_STREAM_DONE with usage

**Error Handling (3 tests):**
- Handle malformed JSON chunk (emits IK_STREAM_ERROR)
- Handle stream interruption mid-transfer
- Completion callback receives error on network failure

**Async Test Flow Pattern:**

```c
// Test captures events via callback
static ik_stream_event_t *captured_events;
static size_t captured_count;
static bool completed;

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx) {
    captured_events[captured_count++] = *event;
    return OK(NULL);
}

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    completed = true;
    return OK(NULL);
}

START_TEST(test_async_streaming_basic)
{
    // Setup: Load fixture, configure mock
    const char *chunks = load_fixture("google/stream_basic.txt");
    mock_set_streaming_response(200, chunks);

    // Start stream (returns immediately)
    res_t r = provider->vt->start_stream(provider->ctx, req,
                                          test_stream_cb, NULL,
                                          test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    int running = 1;
    while (running > 0 && !completed) {
        fd_set read_fds, write_fds, exc_fds;
        FD_ZERO(&read_fds); FD_ZERO(&write_fds); FD_ZERO(&exc_fds);
        int max_fd = 0;

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        // In tests, mock returns immediately without real select()

        provider->vt->perform(provider->ctx, &running);
        // ↑ This triggers mock write callbacks, delivers chunks to stream_cb

        provider->vt->info_read(provider->ctx, logger);
        // ↑ When transfer completes, invokes completion_cb
    }

    // Verify captured events
    ck_assert(completed);
    ck_assert_int_ge(captured_count, 2);  // At least text + done
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
    ck_assert_int_eq(captured_events[captured_count-1].type, IK_STREAM_DONE);
}
END_TEST
```

## Postconditions

- [ ] 1 test file with 21+ tests covering async behavior
- [ ] 3 fixture files recorded with VCR_RECORD=1 (JSONL format)
- [ ] No API keys in fixtures (verify: `grep -r "AIza" tests/fixtures/vcr/google/` returns empty)
- [ ] Async event loop tests verify fdset/perform/info_read cycle
- [ ] Stream callbacks invoked during perform() verified
- [ ] Thought part detection verified
- [ ] Event normalization verified
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-google-streaming.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).