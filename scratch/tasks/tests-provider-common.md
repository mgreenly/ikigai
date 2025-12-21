# Task: Create Common Provider Tests

**Layer:** 2
**Model:** sonnet/none
**Depends on:** provider-types.md, http-client.md, sse-parser.md, credentials-core.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/` - Existing test file patterns
- `src/providers/common/error.c` - Error handling implementation
- `src/credentials.c` - Credentials API

**Plan:**
- `scratch/plan/testing-strategy.md` - Test organization and mock patterns

## Objective

Create test suite for shared provider infrastructure including HTTP client, SSE parser, error handling, credentials lookup, and request/response builders. Establish mock patterns and fixture organization for reuse across provider-specific tests.

## Interface

Test files to create:

| File | Purpose |
|------|---------|
| `tests/unit/providers/common/test_http_client.c` | Tests for HTTP client wrapper |
| `tests/unit/providers/common/test_sse_parser.c` | Tests for SSE parsing logic |
| `tests/unit/providers/test_provider_common.c` | Tests for provider creation and credentials lookup |
| `tests/unit/providers/test_request_builders.c` | Tests for request/response building utilities |
| `tests/unit/providers/test_error_handling.c` | Tests for error category mapping and retryable checks |

Fixture directories to create:

| Directory | Purpose |
|-----------|---------|
| `tests/fixtures/responses/` | Common response samples |
| `tests/fixtures/errors/` | Error response samples per provider |

## Behaviors

**HTTP Client Tests:**
- Mock `curl_easy_perform()` wrapper to return controlled responses
- Verify request headers are set correctly
- Test timeout and error handling
- Support streaming and non-streaming mocks

**SSE Parser Tests:**
- Parse standard SSE event format (event:, data:, id:)
- Handle multi-line data fields
- Detect stream termination
- Handle malformed events gracefully

**Error Handling Tests:**
- `ik_error_category_name()` returns correct string for each category
- `ik_error_is_retryable()` identifies retryable categories correctly
- HTTP status code to error category mapping for each provider
- Provider-specific error response parsing

**Credentials Tests:**
- Lookup by provider name returns correct API key
- Missing credentials return appropriate error
- Environment variable fallback works
- Config file credentials work

**Request Builder Tests:**
- Build JSON request bodies with correct structure
- Include all required fields per provider
- Handle optional fields correctly
- Serialize message history

## Test Scenarios

**HTTP Client:**
- Successful request returns 200 and body
- Network error returns appropriate error category
- Timeout returns ERR_TIMEOUT
- Mock supports both streaming and non-streaming

**SSE Parser:**
- Parse complete event with all fields
- Parse event with only data field
- Handle multi-line data with multiple `data:` lines
- Detect `[DONE]` marker for stream end
- Ignore malformed lines without crashing

**Error Handling:**
- Map 401 to ERR_AUTH for all providers
- Map 429 to ERR_RATE_LIMIT for all providers
- Map 500 to ERR_SERVICE for all providers
- `ik_error_is_retryable()` returns true for rate limit and service errors
- Provider-specific error messages extracted from JSON

**Credentials:**
- Lookup "anthropic" returns ANTHROPIC_API_KEY
- Lookup "openai" returns OPENAI_API_KEY
- Lookup "google" returns GOOGLE_API_KEY
- Missing provider returns ERR_NOT_FOUND

**Request Builders:**
- Build chat completion request with system and user messages
- Include thinking budget when specified
- Include tool definitions when provided
- Omit optional fields when not specified

## Postconditions

- [ ] All common utilities have unit tests
- [ ] Mock pattern established for HTTP client
- [ ] Fixtures organized in `tests/fixtures/`
- [ ] Error handling tests cover all error categories
- [ ] Credentials API tested with all providers
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
