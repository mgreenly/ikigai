# Task: Create OpenAI Provider Basic Tests

**Layer:** 5
**Model:** sonnet/thinking
**Depends on:** openai-core.md, openai-request-chat.md, openai-response-chat.md, tests-mock-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/openai/` - OpenAI provider implementation
- `tests/helpers/mock_http.h` - Mock infrastructure

## Objective

Create tests for OpenAI provider adapter, request serialization, response parsing, and error handling. Covers both standard GPT models and o-series reasoning models.

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
- Send request returns OK with valid response
- Send request returns ERR on HTTP failure
- Vtable functions are non-NULL

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

- [ ] 3 test files created with 23+ tests total
- [ ] 5 fixture files created with valid JSON
- [ ] Both GPT and o-series models tested
- [ ] Reasoning effort mapping tested
- [ ] Tool call argument parsing tested
- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
