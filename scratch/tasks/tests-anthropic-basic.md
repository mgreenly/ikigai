# Task: Create Anthropic Provider Basic Tests

**Layer:** 4
**Model:** sonnet/thinking
**Depends on:** anthropic-core.md, anthropic-request.md, anthropic-response.md, tests-mock-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/anthropic/` - Anthropic provider implementation
- `tests/helpers/mock_http.h` - Mock infrastructure

## Objective

Create tests for Anthropic provider adapter, request serialization, response parsing, and error handling. Uses mock HTTP infrastructure for isolated testing.

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/unit/providers/anthropic/test_anthropic_adapter.c` | Provider vtable implementation |
| `tests/unit/providers/anthropic/test_anthropic_client.c` | Request serialization to Messages API |
| `tests/unit/providers/anthropic/test_anthropic_errors.c` | Error response parsing and mapping |

**Fixture files to create:**

| File | Purpose |
|------|---------|
| `tests/fixtures/anthropic/response_basic.json` | Standard completion response |
| `tests/fixtures/anthropic/response_thinking.json` | Response with thinking content block |
| `tests/fixtures/anthropic/response_tool_call.json` | Response with tool use content block |
| `tests/fixtures/anthropic/error_auth.json` | 401 authentication error |
| `tests/fixtures/anthropic/error_rate_limit.json` | 429 rate limit error |

## Test Scenarios

**Adapter Tests (5 tests):**
- Create adapter with valid credentials
- Destroy adapter cleans up resources
- Send request returns OK with valid response
- Send request returns ERR on HTTP failure
- Vtable functions are non-NULL

**Request Serialization Tests (6 tests):**
- Build request with system and user messages
- Build request with thinking budget (extended thinking)
- Build request with tool definitions
- Build request without optional fields
- Verify correct headers (API key, version, content-type)
- Verify JSON structure matches Messages API spec

**Response Parsing Tests (5 tests):**
- Parse response with single text block
- Parse response with thinking block followed by text block
- Parse response with tool use block
- Parse response with multiple content blocks
- Extract usage metadata (tokens)

**Error Handling Tests (5 tests):**
- Parse authentication error (401)
- Parse rate limit error (429)
- Parse overloaded error (529)
- Parse validation error (400)
- Map errors to correct categories

## Postconditions

- [ ] 3 test files created with 21+ tests total
- [ ] 5 fixture files created with valid JSON
- [ ] All tests use mock HTTP (no real network)
- [ ] Error mapping covers key HTTP status codes
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
