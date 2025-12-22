# Task: Create Provider Switching Integration Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/extended
**Depends on:** repl-provider-routing.md, fork-model-override.md, tests-openai-basic.md, tests-mock-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. Tests must verify async behavior via the fdset/perform/info_read pattern, not blocking calls.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] Mock infrastructure from tests-mock-infrastructure.md is complete

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load database` - Database schema and query patterns

**Source:**
- `tests/integration/` - Existing integration test patterns
- `src/commands_basic.c` - Model switching command
- `src/commands_fork.c` - Fork command with model override
- `src/openai/client_multi.c` - Reference async pattern (fdset/perform/info_read)
- `tests/helpers/mock_http.h` - Mock HTTP infrastructure

**Plan:**
- `scratch/plan/testing-strategy.md` - Mock HTTP Pattern section with async test flow
- `scratch/plan/architecture.md` - Event Loop Integration section
- `scratch/plan/provider-interface.md` - Vtable async methods
- `scratch/plan/streaming.md` - Async streaming data flow

## Objective

Create integration tests for provider switching and fork inheritance using the async vtable interface. Tests verify that:
1. Switching providers preserves message history using async callbacks
2. Thinking levels translate correctly across providers
3. Forked agents correctly inherit or override parent settings
4. The select()-based event loop pattern works correctly with multiple providers

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/integration/test_provider_switching.c` | Provider switching and fork inheritance with async patterns |

**Mock utilities to use:**

| Utility | Purpose |
|---------|---------|
| `tests/helpers/mock_http.h` | Mock curl_multi functions for async testing |
| `tests/test_utils.h` | Test database setup |

**Async vtable methods to test:**

| Method | Purpose |
|--------|---------|
| `provider->vt->fdset()` | Get FDs for select() |
| `provider->vt->perform()` | Non-blocking I/O processing |
| `provider->vt->timeout()` | Get recommended timeout |
| `provider->vt->info_read()` | Check for completed transfers |
| `provider->vt->start_stream()` | Initiate async streaming request |

## Test Scenarios

**Provider Switching with Async Event Loop (5 tests):**

1. **Start with Anthropic, switch to OpenAI (async flow)**
   - Start session with Anthropic claude-sonnet-4-5
   - Call `start_stream()` (returns immediately)
   - Drive event loop with fdset/select/perform cycle
   - Verify stream_cb receives IK_STREAM_TEXT_DELTA events
   - Verify completion_cb receives final response via info_read()
   - Switch to OpenAI gpt-4o via /model command
   - Repeat async streaming cycle
   - Verify both messages in history with correct formatting

2. **Start with OpenAI, switch to Google (async flow)**
   - Similar async flow with different providers
   - Verify event loop correctly handles provider FD changes after switch
   - Verify message history preserved

3. **Multiple switches with concurrent mock responses**
   - Anthropic → OpenAI → Google → Anthropic
   - Each switch tests clean fdset transition
   - Verify perform() processes correct provider's data
   - Verify all messages preserved

4. **Switch preserves system prompt (async verification)**
   - Set system prompt with Anthropic
   - Start async request, verify system prompt in serialized request
   - Switch to OpenAI
   - Start async request, verify system prompt included

5. **Switch updates agent database record after async completion**
   - Switch provider
   - Start async request
   - After completion_cb fires via info_read(), query database
   - Verify agent record updated with new provider

**Thinking Level Translation with Async Streaming (4 tests):**

1. **Anthropic thinking to OpenAI reasoning (async)**
   - Start with Anthropic, set thinking to "medium"
   - Start async stream, mock returns IK_STREAM_THINKING_DELTA events
   - Verify thinkingBudget in serialized request via mock capture
   - Switch to OpenAI o1 model
   - Start async stream
   - Verify reasoning_effort: "medium" in serialized request

2. **OpenAI reasoning to Google thinking (async)**
   - Start with OpenAI o1, set thinking to "high"
   - Mock returns IK_STREAM_THINKING_DELTA events during perform()
   - Switch to Google gemini-2.5
   - Start async stream
   - Verify thinkingBudget in serialized request

3. **Thinking level preserved across async switch**
   - Set thinking level
   - Start async request, complete via event loop
   - Switch provider
   - Start async request
   - Verify level preserved in new format via mock request capture

4. **Thinking level change after switch (async verification)**
   - Switch provider
   - Start async request
   - Change thinking level
   - Start new async request
   - Verify new level in serialized request

**Fork Inheritance with Async Providers (5 tests):**

1. **Fork inherits parent provider (async verification)**
   - Parent: claude-sonnet-4-5/medium
   - /fork without override
   - Start async stream on child
   - Verify child's fdset() returns Anthropic provider FDs
   - Verify child has claude-sonnet-4-5/medium settings

2. **Fork with model override (different provider async)**
   - Parent: claude-sonnet-4-5/medium
   - /fork --model gpt-4o
   - Start async stream on child
   - Verify child's fdset() returns OpenAI provider FDs (not Anthropic)
   - Verify child uses gpt-4o (implicit provider switch)

3. **Fork with thinking override (async)**
   - Parent: gpt-4o/low
   - /fork --thinking high
   - Start async stream on child
   - Verify reasoning_effort: "high" in serialized request
   - Verify child uses gpt-4o/high

4. **Fork with full override (cross-provider async)**
   - Parent: claude-sonnet-4-5/medium
   - /fork --model gemini-2.5-flash/high
   - Start async stream on child
   - Verify child's provider is Google (via fdset/perform cycle)
   - Verify child uses gemini-2.5-flash/high

5. **Database records fork hierarchy after async completion**
   - Create parent and start async request, complete via event loop
   - Fork child and start async request on child
   - After child's completion_cb fires via info_read()
   - Query database
   - Verify parent_uuid and fork_message_id correct

## Behaviors

**Async Mock Setup (Critical):**
- Mock `curl_multi_fdset_()` to return mock FDs (can return -1 for no FDs)
- Mock `curl_multi_perform_()` to simulate progress and deliver data to write callbacks
- Mock `curl_multi_info_read_()` to return completion messages with mock responses
- Set environment variables for test API keys
- Use isolated test database

**Event Loop Test Pattern:**
```c
// Test captures response via completion callback
static ik_response_t *captured_response;
static res_t captured_result;

static res_t test_completion_cb(const ik_provider_completion_t *completion, void *ctx) {
    captured_result = completion->success ? OK(NULL) : ERR(completion->error_message);
    captured_response = completion->response;
    return OK(NULL);
}

// Test captures streaming events
static ik_stream_event_t *captured_events;
static size_t event_count;

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx) {
    captured_events[event_count++] = *event;
    return OK(NULL);
}

START_TEST(test_provider_switch_async)
{
    captured_response = NULL;
    event_count = 0;

    // Setup: Load fixture data for Anthropic
    mock_set_streaming_response(200, load_fixture("anthropic/stream_basic.txt"));

    // Start async stream (returns immediately)
    res_t r = provider->vt->start_stream(provider->ctx, req,
                                          test_stream_cb, NULL,
                                          test_completion_cb, NULL);
    ck_assert(is_ok(&r));

    // Drive event loop until complete
    int running = 1;
    while (running > 0) {
        fd_set read_fds, write_fds, exc_fds;
        int max_fd = 0;
        FD_ZERO(&read_fds); FD_ZERO(&write_fds); FD_ZERO(&exc_fds);

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);
        // Mock: select() not needed in tests, perform() delivers data immediately
        provider->vt->perform(provider->ctx, &running);
    }
    provider->vt->info_read(provider->ctx, logger);

    // Verify stream events were captured
    ck_assert(event_count > 0);
    ck_assert(captured_events[0].type == IK_STREAM_START);
    ck_assert(captured_events[event_count-1].type == IK_STREAM_DONE);

    // Switch provider and repeat...
}
END_TEST
```

**Provider Detection:**
- Model names map to providers:
  - `claude-*` → anthropic
  - `gpt-*`, `o1-*` → openai
  - `gemini-*` → google

**Thinking Level Mapping:**
| Level | Anthropic | OpenAI | Google 2.5 | Google 3.0 |
|-------|-----------|--------|------------|------------|
| none | - | - | - | - |
| low | 1024 tokens | "low" | 1024 tokens | "low" |
| medium | 4096 tokens | "medium" | 4096 tokens | "medium" |
| high | 16384 tokens | "high" | 16384 tokens | "high" |

**Async Streaming Verification:**
- Mock `perform()` feeds SSE chunks to curl write callback incrementally
- Each `perform()` call may deliver zero or more stream events via `stream_cb`
- `info_read()` invokes `completion_cb` when transfer completes
- Stream events are normalized to `ik_stream_event_t` types:
  - `IK_STREAM_START` - Stream started
  - `IK_STREAM_TEXT_DELTA` - Text content delta
  - `IK_STREAM_THINKING_DELTA` - Thinking/reasoning delta
  - `IK_STREAM_TOOL_CALL_START/DELTA/DONE` - Tool call events
  - `IK_STREAM_DONE` - Stream completed
  - `IK_STREAM_ERROR` - Error occurred

## Postconditions

- [ ] 1 test file with 14 tests using async vtable pattern
- [ ] All tests use fdset/perform/info_read cycle (no blocking calls)
- [ ] Provider switching preserves message history via async callbacks
- [ ] Thinking levels translate correctly across providers (verified via mock request capture)
- [ ] Fork inheritance and override tested with async provider verification
- [ ] Database records updated correctly after async completion
- [ ] Mock curl_multi functions used (not curl_easy)
- [ ] Stream callbacks receive normalized ik_stream_event_t events
- [ ] Completion callbacks invoked from info_read()
- [ ] All tests run in `make check`
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] No real API calls made
- [ ] Changes committed to git with message: `task: tests-integration-switching.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).