# Plan: OpenAI Client Module Implementation

## Overview

Implement the OpenAI client module (`src/openai.c/h`) for streaming HTTP requests to OpenAI Chat Completions API. Uses libcurl multi interface for immediate abort support. Depends on config and logger modules.

## Prerequisites

- Logger module completed (plan-logger.md)
- Config module completed (plan-config.md)
- Review docs/phase-1.md and docs/phase-1-details.md for OpenAI client specifications
- Review docs/memory.md for talloc patterns
- Review docs/error_handling.md for Result type usage
- Understand curl multi interface for abort support

## Task List

### Task 1: Review and Verify Prerequisites

**Description**: Review previous work decisions and verify that dependencies are complete and no architectural changes affect OpenAI client module.

**Actions**:
- Verify logger module is complete and tested
- Verify config module is complete and tested
- Read through docs/decisions.md for OpenAI-related decisions
- Verify docs/phase-1-details.md OpenAI section is current
- Review curl multi interface documentation
- Confirm API endpoint and authentication method
- Verify abort flag semantics are clear

**Acceptance**:
- Logger and config modules available
- No conflicts with previous decisions
- OpenAI specification is clear and unchanged
- Understanding of curl multi for shutdown support
- Ready to proceed with implementation

---

### Task 2: Create OpenAI Header and Types (Red Phase)

**Description**: Define callback type and API in header file.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Create `tests/unit/openai_test.c`
- Write test that attempts to use callback type
- Write test that attempts to call `ik_openai_stream_req()`
- Test should fail to compile (types/functions don't exist)
- Run `make check` to verify test fails

**GREEN (Implement Minimum Code)**:
- Create `include/openai.h`
- Add callback type definition:
  ```c
  typedef void (*ik_openai_stream_cb_t)(const char *json_chunk, void *user_data);
  ```
- Add function declaration:
  ```c
  ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                   const ik_cfg_t *cfg,
                                   json_t *req_payload,
                                   ik_openai_stream_cb_t cb,
                                   void *cb_data,
                                   volatile sig_atomic_t *abort_flag);
  ```
- Add include guards and necessary includes
- Update Makefile if needed (libcurl should already be linked)
- Run `make check` to verify test now compiles

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add openai.h types and function declarations"

---

### Task 3: Implement SSE Parser Structure (Red Phase)

**Description**: Create internal SSE parser state structure.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_sse_parser_init()` that:
  - Tests internal SSE parser initialization
  - Verifies buffer allocation
  - Verifies initial state
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Create `src/openai.c`
- Define internal SSE parser structure:
  ```c
  typedef struct {
    TALLOC_CTX *ctx;
    char *buffer;
    size_t buffer_len;
    size_t buffer_capacity;
    ik_openai_stream_cb_t callback;
    void *cb_data;
    bool done;
    ik_result_t error;
  } sse_parser_t;
  ```
- Implement `static sse_parser_t* sse_parser_init()`:
  - Allocate on context
  - Initialize buffer (4096 bytes)
  - Set initial state
  - Return parser
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement SSE parser structure and initialization"

---

### Task 4: Implement SSE Buffer Growth (Red Phase)

**Description**: Implement buffer growth with 1MB limit.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_sse_buffer_growth()` that:
  - Creates SSE parser
  - Simulates incoming data larger than 4KB
  - Verifies buffer grows (doubles capacity)
  - Continues adding data
  - Verifies growth works correctly
- Add test `test_openai_sse_buffer_limit()` that:
  - Creates SSE parser
  - Attempts to grow buffer beyond 1MB
  - Verifies returns error (IK_ERR_PARSE)
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Implement buffer growth logic in SSE write callback:
  - Check if `buffer_len + new_bytes > buffer_capacity`
  - Calculate `new_cap = buffer_capacity * 2`
  - Check if `new_cap > 1048576` (1MB limit)
  - If exceeds limit, set error and return 0 (abort transfer)
  - Use `talloc_realloc()` to grow buffer
  - Handle OOM, set error and return 0
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement SSE buffer growth with 1MB limit"

---

### Task 5: Implement SSE Message Parsing (Red Phase)

**Description**: Parse SSE format and extract data chunks.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_sse_parse_single_chunk()` that:
  - Simulates receiving "data: {\"test\":\"value\"}\n\n"
  - Parses SSE message
  - Verifies callback called with JSON chunk
- Add test `test_openai_sse_parse_multiple_chunks()` that:
  - Simulates receiving multiple SSE messages
  - Verifies each triggers callback
- Add test `test_openai_sse_parse_done()` that:
  - Simulates receiving "data: [DONE]\n\n"
  - Verifies done flag set
  - Verifies callback NOT called for [DONE]
- Add test `test_openai_sse_invalid_format()` that:
  - Sends malformed SSE (missing "data: ")
  - Verifies returns error
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Implement `static size_t sse_write_callback()`:
  - Append new data to buffer
  - While not done:
    - Search for "\n\n" delimiter
    - If found, extract message
    - Check for "data: " prefix (6 chars)
    - If invalid, set error and abort
    - Extract content after "data: "
    - Check if content is "[DONE]"
    - If [DONE], set done flag and break
    - Otherwise, validate JSON and call callback
    - Remove processed message from buffer
  - Return bytes received (or 0 on error)
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement SSE message parsing and callback"

---

### Task 6: Implement JSON Validation (Red Phase)

**Description**: Validate JSON chunks before passing to callback.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_sse_invalid_json()` that:
  - Sends SSE with invalid JSON chunk
  - Verifies returns error (IK_ERR_PARSE)
  - Verifies callback NOT called
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Add JSON validation in SSE write callback:
  - Before calling callback, parse JSON with `json_loads()`
  - Check for errors
  - If invalid, set parser error and return 0
  - Call `json_decref()` immediately (just validating)
  - Then call callback with original string
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add JSON validation for SSE chunks"

---

### Task 7: Implement Curl Request Setup (Red Phase)

**Description**: Set up curl easy handle with headers and options.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_curl_setup()` that:
  - Tests curl handle initialization
  - Verifies headers are set correctly
  - Verifies URL is correct
  - Verifies options are correct
- This may need mocking or inspection of curl handle
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Implement curl setup in `ik_openai_stream_req()`:
  - Serialize request payload with `json_dumps()`
  - Create curl easy handle
  - Build Authorization header with API key
  - Add Content-Type header
  - Set URL: "https://api.openai.com/v1/chat/completions"
  - Set POST fields to serialized JSON
  - Set write callback and user data
  - Handle OOM and initialization failures
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement curl request setup and headers"

---

### Task 8: Implement Curl Multi Interface (Red Phase)

**Description**: Use curl multi for event loop with abort support.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_request_with_abort()` that:
  - Starts request
  - Sets abort flag during request
  - Verifies request aborts promptly (< 200ms)
  - Verifies returns OK (abort is not error)
- This may require real API call or mock server
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Implement curl multi event loop:
  - Create multi handle with `curl_multi_init()`
  - Add easy handle with `curl_multi_add_handle()`
  - Loop while running and `!*abort_flag`:
    - Call `curl_multi_perform()`
    - Call `curl_multi_poll()` with 50ms timeout
  - If aborted, remove handle immediately
  - Check HTTP status code
  - Clean up handles
  - Return result based on status and errors
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement curl multi interface with abort support"

---

### Task 9: Implement HTTP Error Handling (Red Phase)

**Description**: Handle non-2xx HTTP responses and authentication errors.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_auth_error()` that:
  - Makes request with invalid API key
  - Verifies returns ERR with IK_ERR_AUTH
  - Verifies error message mentions authentication
- Add test `test_openai_http_error()` that:
  - Simulates 500 server error
  - Verifies returns ERR with IK_ERR_NETWORK
- Add test `test_openai_network_error()` that:
  - Simulates connection failure
  - Verifies returns ERR with IK_ERR_NETWORK
- These may require mock server or real API
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Add error handling after curl execution:
  - Get HTTP status code with `curl_easy_getinfo()`
  - Check if aborted (return OK)
  - Check if parser error occurred (return parser error)
  - Check HTTP status:
    - 401 → return `ERR(ctx, AUTH, "Authentication failed")`
    - Non-2xx → return `ERR(ctx, NETWORK, "HTTP error %ld", code)`
  - Check curl errors (connection failures)
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement HTTP error handling and authentication errors"

---

### Task 10: Test Memory Management (Red Phase)

**Description**: Verify proper cleanup of all resources.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_memory_cleanup()` that:
  - Makes successful request
  - Verifies all talloc allocations on correct context
  - Verifies jansson objects freed
  - Verifies curl handles cleaned up
  - Verifies no memory leaks
- Add test `test_openai_error_path_cleanup()` that:
  - Tests various error paths
  - Verifies cleanup happens in all error cases
  - No leaks on error paths
- Run `make check` to verify tests pass

**GREEN (Fix Any Issues)**:
- Ensure all cleanup paths are correct:
  - Temporary context freed before return
  - jansson strings freed after copying
  - curl easy and multi handles cleaned up
  - curl headers freed
  - Parser buffer on temporary context (auto-freed)
- Add cleanup labels if needed for error paths
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add memory management tests for OpenAI client"

---

### Task 11: Integration Tests with Real API (Red Phase)

**Description**: Test complete flow with real OpenAI API.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Create `tests/integration/openai_integration_test.c`
- Add test `test_openai_real_api_stream()` that:
  - Uses real API key from config or environment
  - Makes streaming request to gpt-4o-mini
  - Captures all chunks via callback
  - Verifies chunks are valid JSON
  - Verifies [DONE] received
  - Verifies complete response makes sense
- Add test `test_openai_real_api_abort()` that:
  - Starts streaming request
  - Sets abort flag after first chunk
  - Verifies request aborts promptly
- Skip if no API key available
- Run `make check` to verify test runs

**GREEN (Fix Any Issues)**:
- Fix any issues discovered with real API
- Capture response examples for documentation
- Run `make check` to verify all tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add OpenAI integration tests with real API"

---

### Task 12: Test Shutdown Response Time (Red Phase)

**Description**: Verify abort happens within shutdown requirement (< 200ms).

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_openai_shutdown_timing()` that:
  - Starts streaming request
  - Records time
  - Sets abort flag
  - Waits for function to return
  - Verifies elapsed time < 200ms
- Run `make check` to verify test passes

**GREEN (Verify Implementation)**:
- Implementation should already meet requirement (50ms poll interval)
- If test fails, adjust poll timeout in curl_multi_poll()
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add shutdown timing verification test"

---

### Task 13: Manual Verification and Documentation Review

**Description**: Human verification of OpenAI client functionality and code quality.

**Manual Tests**:
1. Run test suite: `make check`
2. Make real API call with valid key
3. Make API call with invalid key (verify 401 error)
4. Test abort during streaming (verify prompt termination)
5. Test with large responses (verify buffer growth)
6. Verify SSE parsing handles edge cases
7. Check shutdown timing meets < 200ms requirement

**Code Inspection**:
- Review openai.c implementation for:
  - Proper talloc memory management
  - All allocations on provided context
  - jansson cleanup (json_decref, free)
  - curl handle cleanup (easy and multi)
  - curl headers cleanup
  - Error handling on all paths
  - Logger usage for errors
  - No memory leaks in error paths
  - Abort flag checking correctness
  - SSE parsing robustness

**Documentation**:
- Verify openai.h has appropriate comments
- Ensure implementation matches specification exactly
- Check callback semantics are clear
- Verify error codes are appropriate
- Check abort behavior is documented

**Acceptance**:
- All manual tests pass
- Code quality is high
- Implementation matches specification
- Streaming works correctly
- Abort support works properly
- Error handling is robust
- Ready to proceed to next module (handler)

---

## Success Criteria

- All tests pass (`make check`)
- Code complexity passes (`make lint`)
- Coverage is 100% for Lines, Functions, and Branches
- All commits follow format: "Brief description of change"
- No architectural changes made without explicit approval
- OpenAI client ready for use by handler module
- Streaming SSE parsing works correctly
- Curl multi interface provides abort support
- HTTP error handling is comprehensive
- Shutdown response time < 200ms
- Memory management is clean and leak-free

## Notes

- OpenAI client depends on config and logger modules
- Use talloc for all memory management
- Use Result types for error handling
- Must use curl multi interface (not curl_easy_perform) for abort support
- SSE buffer has 1MB growth limit
- Callback receives complete JSON chunks (no "data: " prefix)
- Callback NOT called for [DONE] marker
- Authentication errors return IK_ERR_AUTH
- Network errors return IK_ERR_NETWORK
- Abort is not an error - returns OK(NULL)
- Tests should use real API during development
- Consider capturing responses for mocked tests later
