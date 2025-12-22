# Task: Create OpenAI Provider Basic Tests

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** openai-core.md, openai-request-chat.md, openai-response-chat.md, tests-mock-infrastructure.md

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

Tests MUST exercise the async fdset/perform/info_read pattern, not blocking patterns.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/openai/` - OpenAI provider implementation
- `tests/helpers/mock_http.h` - Mock infrastructure (curl_multi mocks)
- `src/openai/client_multi.c` - Reference async implementation

**Plan:**
- `scratch/plan/testing-strategy.md` - Mock HTTP pattern, async test flow
- `scratch/plan/provider-interface.md` - Vtable specification with async methods

## Objective

Create tests for OpenAI provider adapter, request serialization, response parsing, and error handling. Covers both standard GPT models and o-series reasoning models. All tests use the async curl_multi mock pattern.

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/openai/test_openai_adapter.c` | Provider vtable implementation |
| `tests/unit/providers/openai/test_openai_client.c` | Request serialization to Chat Completions API |
| `tests/unit/providers/openai/test_openai_errors.c` | Error response parsing and mapping |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/openai/response_basic.json` | Standard chat completion response |
| `tests/fixtures/openai/response_tool_call.json` | Response with tool_calls array |
| `tests/fixtures/openai/response_reasoning.json` | Response from o-series model |
| `tests/fixtures/openai/error_auth.json` | 401 authentication error |
| `tests/fixtures/openai/error_rate_limit.json` | 429 rate limit error |

## Test Scenarios

**Adapter Tests (5 tests):**
- Create adapter with valid credentials
- Destroy adapter cleans up resources
- Vtable async methods (fdset, perform, timeout, info_read) are non-NULL
- start_request returns OK immediately (non-blocking)
- start_stream returns OK immediately (non-blocking)

**Async Request Tests (4 tests):**

These tests use the async fdset/perform/info_read pattern. Completion is delivered via callback.

```c
// Pattern: start_request + event loop until callback fires
static ik_response_t *captured_response;

static res_t test_completion_cb(const ik_http_completion_t *completion, void *ctx) {
    captured_response = completion->response;
    return OK(NULL);
}

START_TEST(test_non_streaming_request)
{
    captured_response = NULL;
    mock_set_response(200, load_fixture("openai/response_basic.json"));

    res_t r = provider->vt->start_request(provider->ctx, req, test_completion_cb, NULL);
    ck_assert(is_ok(&r));  // Returns immediately

    // Drive event loop until complete
    while (captured_response == NULL) {
        fd_set rfds, wfds, efds;
        int max_fd = 0;
        provider->vt->fdset(provider->ctx, &rfds, &wfds, &efds, &max_fd);
        // select() call here in real code
        provider->vt->perform(provider->ctx, NULL);
        provider->vt->info_read(provider->ctx, NULL);
    }

    ck_assert_str_eq(captured_response->content, "expected");
}
END_TEST
```

- start_request with mock response triggers callback on info_read
- start_request with mock HTTP error triggers callback with error
- Callback receives response via ik_http_completion_t
- Event loop drives fdset/perform/info_read cycle

**Request Serialization Tests (7 tests):**
- Build request with system and user messages
- Build request for o1 model with reasoning_effort
- Build request for gpt-4 model without reasoning_effort
- Build request with tool definitions
- Build request without optional fields
- Verify correct headers (API key, content-type)
- Verify JSON structure matches Chat Completions API

**Response Parsing Tests (6 tests):**
- Parse response with message content
- Parse response with tool_calls array
- Parse response from reasoning model
- Extract tool call ID and arguments
- Parse tool arguments from JSON string
- Extract usage metadata (tokens)

**Error Handling Tests (5 tests):**
- Parse authentication error (401)
- Parse rate limit error (429)
- Parse context length error (400)
- Parse model not found error (404)
- Map errors to correct categories

## Postconditions

- [ ] 3 test files created with 27+ tests total
- [ ] 5 fixture files created with valid JSON
- [ ] Async tests use fdset/perform/info_read pattern (not blocking send)
- [ ] Mock curl_multi functions used (not curl_easy)
- [ ] Callbacks receive responses via ik_http_completion_t
- [ ] Both GPT and o-series models tested
- [ ] Reasoning effort mapping tested
- [ ] Tool call argument parsing tested
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-openai-basic.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).