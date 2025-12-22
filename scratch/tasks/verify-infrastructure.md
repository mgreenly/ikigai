# Task: Verify Infrastructure Stage

**VERIFICATION TASK:** This task does not create code. It verifies previous tasks completed correctly.

**Model:** sonnet/thinking
**Depends on:** verify-foundation.md, tests-mock-infrastructure.md, tests-common-utilities.md, tests-provider-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)

This verification gate ensures the test infrastructure and mock systems work correctly before OpenAI shim tasks begin. The mock infrastructure is critical - if it doesn't work, all provider tests will fail in confusing ways.

If any verification step fails, return `{"ok": false, "blocked_by": "<specific failure>"}`.

## Pre-Read

Read these files to understand what was supposed to be created:
- `scratch/tasks/tests-mock-infrastructure.md` - Mock curl_multi pattern
- `scratch/tasks/tests-common-utilities.md` - HTTP and SSE test utilities
- `scratch/tasks/tests-provider-core.md` - Provider factory and vtable tests

## Verification Protocol

### Step 1: Test Infrastructure Files Exist

**Mock infrastructure:**
- [ ] `tests/mocks/curl_multi_mock.h` exists
- [ ] `tests/mocks/curl_multi_mock.c` exists
- [ ] Mock state machine types are defined

**Test fixtures:**
- [ ] `tests/fixtures/responses/` directory exists
- [ ] `tests/fixtures/errors/` directory exists
- [ ] At least one fixture file exists in each directory

**Test utilities:**
- [ ] HTTP client test file exists in `tests/unit/providers/common/`
- [ ] SSE parser test file exists in `tests/unit/providers/common/`
- [ ] Provider core test file exists in `tests/unit/providers/`

### Step 2: Mock Infrastructure Compiles

```bash
make build/tests/mocks/curl_multi_mock.o 2>&1
```

- [ ] Compilation succeeds
- [ ] No warnings

Or if mocks are compiled as part of test binaries:
```bash
ls build/tests/unit/providers/*.o 2>/dev/null | head -5
```
- [ ] Test object files exist

### Step 3: Mock API Verification

Read mock header and verify these functions/macros exist:

**Mock control functions:**
- [ ] Function to set mock response data
- [ ] Function to set mock HTTP status
- [ ] Function to simulate streaming chunks
- [ ] Function to reset mock state between tests

**Mock state tracking:**
- [ ] Can track number of requests made
- [ ] Can track callbacks invoked
- [ ] Can verify fdset/perform/info_read cycle was followed

### Step 4: Async Pattern Mock Verification

Verify the mock supports the async pattern:

Read mock implementation and verify:
- [ ] Mock `curl_multi_fdset` populates fd_sets correctly
- [ ] Mock `curl_multi_perform` drives state machine forward
- [ ] Mock `curl_multi_info_read` returns completion info
- [ ] Mock supports multiple sequential perform() calls before completion
- [ ] Mock can simulate streaming (partial data delivery)

### Step 5: Test Fixture Format Verification

Read a sample response fixture and verify format:

```bash
head -20 tests/fixtures/responses/*.json | head -40
```

- [ ] Fixtures are valid JSON
- [ ] Fixtures match expected provider response format
- [ ] Error fixtures include HTTP status and error body

### Step 6: HTTP Client Tests Pass

Run HTTP client utility tests:
```bash
make build/tests/unit/providers/common/http_multi_test && ./build/tests/unit/providers/common/http_multi_test
```

- [ ] All tests pass
- [ ] Tests cover: create, fdset, perform, timeout, info_read, add_request

Verify test coverage of async pattern:
```bash
grep -n "fdset\|perform\|info_read" tests/unit/providers/common/http_multi_test.c | wc -l
```
- [ ] Multiple test cases exercise the async cycle

### Step 7: SSE Parser Tests Pass

Run SSE parser tests:
```bash
make build/tests/unit/providers/common/sse_parser_test && ./build/tests/unit/providers/common/sse_parser_test
```

- [ ] All tests pass
- [ ] Tests cover: create, feed, reset, partial chunks, multi-line data

Verify callback invocation tests:
```bash
grep -n "callback" tests/unit/providers/common/sse_parser_test.c | wc -l
```
- [ ] Tests verify callbacks are invoked with correct event data

### Step 8: Provider Core Tests Pass

Run provider core tests:
```bash
make build/tests/unit/providers/provider_core_test && ./build/tests/unit/providers/provider_core_test
```

- [ ] All tests pass

Verify tests cover all providers:
```bash
grep -E "(openai|anthropic|google)" tests/unit/providers/provider_core_test.c
```
- [ ] All three provider names appear in tests

### Step 9: Mock Integration Test

Verify mock can drive a complete request cycle:

Look for a test that does the full cycle:
```bash
grep -A 20 "test.*async\|test.*cycle\|test.*complete" tests/unit/providers/common/http_multi_test.c
```

The test should:
- [ ] Call add_request (non-blocking start)
- [ ] Call fdset to get file descriptors
- [ ] Call perform to drive I/O
- [ ] Call info_read to check completion
- [ ] Verify callback was invoked with response

### Step 10: Streaming Mock Test

Verify mock can simulate streaming:

Look for streaming tests:
```bash
grep -n "stream\|chunk\|delta" tests/unit/providers/common/http_multi_test.c
```

- [ ] Tests exist for streaming scenarios
- [ ] Tests verify multiple chunks are delivered
- [ ] Tests verify stream completion callback

### Step 11: Error Simulation Test

Verify mock can simulate errors:

```bash
grep -n "error\|fail\|401\|429\|500" tests/unit/providers/common/http_multi_test.c
```

- [ ] Tests exist for error scenarios
- [ ] Tests cover: auth failure (401), rate limit (429), server error (500)
- [ ] Tests verify error is delivered via completion callback (not return value)

### Step 12: Provider Factory Tests

Verify factory creates all providers:

```bash
grep -A 10 "test.*create\|test.*factory" tests/unit/providers/provider_core_test.c
```

- [ ] Test creates OpenAI provider
- [ ] Test creates Anthropic provider
- [ ] Test creates Google provider
- [ ] All created providers have non-NULL vtable
- [ ] All vtable methods are non-NULL

### Step 13: Vtable Method Tests

Verify each vtable method is tested:

```bash
grep -c "vt->fdset\|vt->perform\|vt->timeout\|vt->info_read\|vt->start_request\|vt->start_stream" tests/unit/providers/provider_core_test.c
```

- [ ] At least 6 matches (one per method)
- [ ] Each method is called in at least one test

### Step 14: Environment Variable Tests

Verify credential tests use environment variables:

```bash
grep -n "setenv\|getenv\|OPENAI_API_KEY\|ANTHROPIC_API_KEY\|GOOGLE_API_KEY" tests/unit/providers/provider_core_test.c
```

- [ ] Tests set credentials via environment variables
- [ ] Tests clean up environment after each test (unsetenv or restore)

### Step 15: Full Test Suite Runs

Run all infrastructure tests together:
```bash
make check-unit 2>&1 | grep -E "(PASS|FAIL|providers|mock)"
```

- [ ] All provider tests pass
- [ ] All mock tests pass
- [ ] No failures in infrastructure code

### Step 16: Cross-Task Contract Verification

Verify mock infrastructure meets downstream expectations:

**From `openai-shim-compat-tests.md`:**
- Expects mock to simulate OpenAI responses → verify fixture exists
- Expects mock to support streaming → verify streaming test passes

**From `tests-anthropic-basic.md`:**
- Expects mock to simulate Anthropic responses → verify fixture format is compatible
- Expects mock HTTP status codes to map to error categories

**From `tests-google-basic.md`:**
- Expects mock to simulate Google responses → verify fixture format
- Expects mock to handle JSON lines (not SSE) for Google streaming

## Postconditions

- [ ] All 16 verification steps pass
- [ ] Mock infrastructure compiles without warnings
- [ ] Mock supports complete async cycle (fdset/perform/info_read)
- [ ] Mock supports streaming simulation
- [ ] Mock supports error simulation
- [ ] All provider factory tests pass
- [ ] All HTTP client tests pass
- [ ] All SSE parser tests pass

## Success Criteria

Return `{"ok": true}` if ALL verification steps pass.

Return `{"ok": false, "blocked_by": "<specific failure>"}` if any step fails.

**Example failures:**

```json
{
  "ok": false,
  "blocked_by": "Step 9: Mock Integration Test - No test found that exercises complete fdset/perform/info_read cycle. Tests only call add_request without driving event loop.",
  "affected_downstream": ["openai-shim-compat-tests.md", "tests-anthropic-basic.md", "tests-google-basic.md"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 6: HTTP Client Tests - Test failed: test_perform_drives_transfer. Expected callback invocation after perform(), but callback was never called.",
  "affected_downstream": ["All provider streaming tests"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 14: Environment Variable Tests - Tests do not clean up environment. Found setenv() without matching unsetenv(). This will cause test pollution in parallel execution.",
  "affected_downstream": ["All credential-dependent tests"]
}
```
