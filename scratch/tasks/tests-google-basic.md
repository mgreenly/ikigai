# Task: Create Google Provider Basic Tests

**Model:** sonnet/thinking
**Depends on:** google-core.md, google-request.md, google-response.md, tests-mock-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/google/` - Google/Gemini provider implementation
- `tests/helpers/mock_http.h` - Mock infrastructure

## Objective

Create tests for Google Gemini provider adapter, request serialization, response parsing, and error handling. Covers both Gemini 2.5 (thinkingBudget) and Gemini 3.0 (thinkingLevel) thinking parameters.

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/google/test_google_adapter.c` | Provider vtable implementation |
| `tests/unit/providers/google/test_google_client.c` | Request serialization to Gemini API |
| `tests/unit/providers/google/test_google_errors.c` | Error response parsing and mapping |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/google/response_basic.json` | Standard completion response |
| `tests/fixtures/google/response_thinking.json` | Response with thought=true parts |
| `tests/fixtures/google/response_function_call.json` | Response with functionCall |
| `tests/fixtures/google/error_auth.json` | 401/403 authentication error |
| `tests/fixtures/google/error_rate_limit.json` | 429 rate limit error |

## Test Scenarios

**Adapter Tests (5 tests):**
- Create adapter with valid credentials
- Destroy adapter cleans up resources
- Send request returns OK with valid response
- Send request returns ERR on HTTP failure
- Vtable functions are non-NULL

**Request Serialization Tests (7 tests):**
- Build request with system and user messages
- Build request for Gemini 2.5 with thinkingBudget
- Build request for Gemini 3.0 with thinkingLevel
- Build request with tool declarations
- Build request without optional fields
- Verify API key in URL (not header)
- Verify JSON structure matches Gemini API spec

**Response Parsing Tests (6 tests):**
- Parse response with single text part
- Parse response with thought=true part followed by text
- Parse response with functionCall part
- Parse response with multiple parts
- Generate UUID for tool call (Gemini doesn't provide IDs)
- Detect thought signature in text content

**Error Handling Tests (5 tests):**
- Parse authentication error (401/403)
- Parse rate limit error (429)
- Parse quota exceeded error
- Parse validation error (400)
- Map errors to correct categories

## Postconditions

- [ ] 3 test files created with 23+ tests total
- [ ] 5 fixture files created with valid JSON
- [ ] Both Gemini 2.5 and 3.0 thinking params tested
- [ ] UUID generation for tool calls tested
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: tests-google-basic.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).