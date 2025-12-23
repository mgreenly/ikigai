# Task: Create Integration Flow Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/high
**Depends on:** tests-integration-switching.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load database` - Database schema and query patterns
- `/load errors` - Error handling patterns

**Source:**
- `tests/integration/` - Existing integration test patterns
- `tests/integration/openai/http_handler_tool_calls_test.c` - Tool call test pattern
- `src/tool_dispatcher.c` - Tool execution
- `src/repl_init.c` - Session restoration
- `src/openai/client_multi.c` - Reference async pattern (fdset/perform/info_read)

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop
- `scratch/plan/testing-strategy.md` - Integration tests, mock curl_multi pattern
- `scratch/plan/provider-interface.md` - Async vtable interface
- `scratch/plan/streaming.md` - Async streaming data flow

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Mock `curl_multi_fdset_()`, `curl_multi_perform_()`, `curl_multi_info_read_()`
- Tests must exercise the async fdset/perform/info_read cycle
- NEVER use blocking mocks

Reference: `src/openai/client_multi.c` for the correct async pattern.

## Objective

Create integration tests for cross-cutting flows: tool calling across providers, error handling and recovery, and session restoration. Tests verify end-to-end async behavior with mocked curl_multi functions. All tests must exercise the non-blocking event loop pattern.

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/integration/test_tool_calls_e2e.c` | Tool calling with different providers |
| `tests/integration/test_error_handling_e2e.c` | Error handling and recovery |
| `tests/integration/test_session_restore_e2e.c` | Session restoration |

## Test Scenarios

### Tool Calls E2E (6 tests) - Async Pattern

All tool call tests MUST use the async mock pattern. Each test:
- Calls `start_stream()` which returns immediately
- Drives the mock event loop (fdset/perform/info_read cycle)
- Receives tool_call events via stream callback during `perform()`
- Receives completion via callback from `info_read()`

1. **Anthropic tool call format (async)**
   - Configure mock curl_multi with Anthropic SSE fixture
   - Define weather tool, call `start_stream()`
   - Drive event loop - stream_cb receives IK_STREAM_TOOL_CALL_START/DELTA/DONE
   - Verify tool call parsed from accumulated deltas
   - Call `start_stream()` with tool_result, drive event loop again
   - Verify final response via completion callback

2. **OpenAI tool call format (async)**
   - Configure mock curl_multi with OpenAI SSE fixture
   - Mock returns tool_calls array with JSON string arguments
   - Drive event loop, verify stream events received
   - Verify JSON arguments parsed correctly from accumulated deltas

3. **Google tool call format (async)**
   - Configure mock curl_multi with Google SSE fixture
   - Mock returns functionCall parts (complete in one chunk)
   - Drive event loop, verify TOOL_CALL_START + TOOL_CALL_DONE emitted together
   - Verify UUID generated for tool call id

4. **Tool result format per provider (async)**
   - For each provider: start_stream() with tool_result
   - Drive event loop
   - Verify result formatted correctly via captured request body

5. **Multiple tool calls in one response (async)**
   - Mock returns multiple tool calls in SSE stream
   - Drive event loop, verify all TOOL_CALL events indexed correctly
   - Verify all tools executed and results returned via callbacks

6. **Tool error handling (async)**
   - Tool returns error in tool_result message
   - Call `start_stream()` with error result
   - Drive event loop
   - Verify error propagated to provider correctly via completion callback

### Error Handling E2E (6 tests) - Async Pattern

Error tests verify that errors are delivered via completion callback, NOT as return values from `start_request()`/`start_stream()`. Each test:
- Calls `start_request()` or `start_stream()` which returns OK (non-blocking)
- Drives the mock event loop
- Receives error via completion callback from `info_read()`
- Verifies error category, retryable flag, and message

1. **Rate limit from Anthropic (async)**
   - Configure mock curl_multi to return HTTP 429
   - Call `start_request()` - returns OK immediately
   - Drive event loop until completion callback fires
   - Verify completion->result has IK_ERR_CAT_RATE_LIMIT category
   - Verify completion->retryable is true
   - Verify completion->retry_after_ms extracted from header

2. **Rate limit from OpenAI (async)**
   - Configure mock curl_multi with OpenAI 429 response
   - Drive event loop
   - Verify same IK_ERR_CAT_RATE_LIMIT category via callback

3. **Auth error from OpenAI (async)**
   - Configure mock curl_multi to return HTTP 401
   - Call `start_request()` - returns OK immediately
   - Drive event loop
   - Verify completion callback receives IK_ERR_CAT_AUTH category
   - Verify completion->retryable is false

4. **Overloaded error from Anthropic (async)**
   - Configure mock curl_multi to return HTTP 529
   - Drive event loop
   - Verify IK_ERR_CAT_SERVER category via callback
   - Verify retryable flag true

5. **Context length error (async)**
   - Configure mock curl_multi with 400 + context_length_exceeded body
   - Drive event loop
   - Verify IK_ERR_CAT_INVALID_ARG category via callback

6. **Network error (async)**
   - Configure mock curl_multi_info_read to return CURLE_COULDNT_CONNECT
   - Drive event loop
   - Verify IK_ERR_CAT_NETWORK category via callback
   - Verify retryable flag true

### Session Restoration E2E (5 tests)

Session restoration tests database operations (synchronous) followed by async provider interactions. After restoration, subsequent message sends use the async vtable pattern.

1. **Restore provider setting**
   - Create session with openai/gpt-5
   - Save to database
   - Simulate restart (new REPL context)
   - Load from database
   - Verify provider is OpenAI
   - Send test message via async start_stream()
   - Drive event loop, verify OpenAI request format used

2. **Restore model setting**
   - Create session with specific model
   - Restore
   - Verify model preserved in agent context
   - Send message, verify model in request body

3. **Restore thinking level**
   - Create session with thinking level high
   - Restore
   - Verify thinking level preserved
   - Send message via async start_stream()
   - Verify thinking parameters in request body

4. **Restore conversation history**
   - Create session with messages
   - Restore using ik_agent_replay_history()
   - Verify messages loaded in order
   - Call start_stream() with new message
   - Verify request body includes full history

5. **Restore forked agent**
   - Create parent and forked child
   - End session
   - Restore
   - Verify parent-child relationship preserved
   - Verify child has correct provider/model
   - Send message to child via async pattern
   - Verify correct provider format used

## Behaviors

### Async Test Pattern (CRITICAL)

Tests MUST use the async mock pattern, NOT blocking mocks. Here is the required pattern:

```c
// Capture response via callback
static ik_response_t *captured_response;
static res_t captured_result;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    captured_result = completion->success ? OK(NULL) : ERR(completion->error_message);
    captured_response = completion->response;
    return OK(NULL);
}

START_TEST(test_async_request)
{
    captured_response = NULL;

    // Setup: Load fixture and configure mock curl_multi
    const char *response_json = load_fixture("provider/response.json");
    mock_curl_multi_set_response(200, response_json);

    // Create provider (uses async vtable, credentials loaded internally)
    res_t r = ik_provider_create(ctx, "anthropic", &provider);
    ck_assert(is_ok(&r));

    // Start request (returns immediately - non-blocking)
    r = provider->vt->start_request(provider->ctx, req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    while (captured_response == NULL) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);

        // Get FDs from provider
        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

        // select() would go here in real code - mock just proceeds
        // struct timeval tv = {0, 10000};
        // select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &tv);

        // Process I/O (non-blocking)
        int running;
        provider->vt->perform(provider->ctx, &running);

        // Check for completed transfers
        provider->vt->info_read(provider->ctx, NULL);
    }

    // Assert on captured response
    ck_assert(is_ok(&captured_result));
    ck_assert_str_eq(captured_response->content, "expected");
}
END_TEST
```

### Mock curl_multi Functions

The test harness must mock these MOCKABLE wrapper functions:

```c
// Mock curl_multi_fdset - returns mock FDs (can return -1 for no FDs)
CURLMcode curl_multi_fdset_(CURLM *multi_handle, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd);

// Mock curl_multi_perform - simulates progress, invokes write callbacks
CURLMcode curl_multi_perform_(CURLM *multi_handle, int *running_handles);

// Mock curl_multi_info_read - returns completion messages
CURLMsg *curl_multi_info_read_(CURLM *multi_handle, int *msgs_in_queue);
```

**Mock behavior:**
- `curl_multi_fdset_()` - Returns mock FDs or -1 when no activity needed
- `curl_multi_perform_()` - Decrements running_handles, feeds data to write callbacks
- `curl_multi_info_read_()` - Returns CURLMSG_DONE when transfer complete

### Tool Call Flow (Async)

```
1. start_stream() called - returns immediately (non-blocking)
2. Event loop: fdset() -> select() -> perform() -> info_read()
3. During perform(): SSE data arrives, stream_cb invoked with tool_call events
4. Tool call accumulated: TOOL_CALL_START -> TOOL_CALL_DELTA... -> TOOL_CALL_DONE
5. REPL executes tool during info_read() completion callback
6. start_stream() called again with tool_result - returns immediately
7. Event loop drives second request
8. Final response received via completion callback
```

### Error Category Mapping

| HTTP Status | Category | Retryable |
|-------------|----------|-----------|
| 401, 403 | IK_ERR_CAT_AUTH | false |
| 429 | IK_ERR_CAT_RATE_LIMIT | true |
| 400 | IK_ERR_CAT_INVALID_ARG | false |
| 500, 502, 503 | IK_ERR_CAT_SERVER | true |
| 529 (Anthropic) | IK_ERR_CAT_SERVER | true |
| Connection fail | IK_ERR_CAT_NETWORK | true |

**Errors are delivered via completion callback**, not as return values from start_request/start_stream. Tests must verify error handling through the callback.

### Session Restoration Flow

```
1. REPL starts
2. Query database for active session
3. Load agents from agents table
4. For each agent, replay messages using ik_agent_replay_history()
5. Reconstruct mark stack
6. Resume with provider/model/thinking from agent record
```

Session restoration is synchronous (database queries), but subsequent provider interactions are async.

## Postconditions

**Async Architecture Requirements:**
- [ ] All tests use async mock pattern (fdset/perform/info_read cycle)
- [ ] No blocking HTTP mocks - only curl_multi mocks
- [ ] Responses received via completion callbacks, not return values
- [ ] Stream events received via stream callbacks during perform()

**Functional Requirements:**
- [ ] 3 test files with 17 tests total
- [ ] Tool calling works with all 3 providers via async pattern
- [ ] Tool result formatting verified
- [ ] Error categories mapped correctly (delivered via callback)
- [ ] Retryable flag verified for each error type
- [ ] Session restoration preserves all settings
- [ ] Restored sessions can send messages via async vtable

**Quality Requirements:**
- [ ] All tests run in `make check`
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] No real API calls made (all mocked via curl_multi)

**Git Requirements:**
- [ ] Changes committed to git with message: `task: tests-integration-flows.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).