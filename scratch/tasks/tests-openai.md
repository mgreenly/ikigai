# Task: Create OpenAI Provider Tests

**Layer:** 5
**Model:** sonnet/none
**Depends on:** openai-core.md, openai-request-chat.md, openai-response-chat.md, openai-streaming-chat.md, tests-provider-common.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/openai/` - OpenAI provider implementation

**Plan:**
- `scratch/plan/testing-strategy.md` - Provider-specific test organization

## Objective

Create comprehensive test suite for OpenAI provider including Chat Completions API format, tool calls with JSON string arguments, reasoning effort mapping for o-series models, and error handling. Tests verify correct implementation of both standard and reasoning model formats.

## Interface

Test files to create:

| File | Purpose |
|------|---------|
| `tests/unit/providers/openai/test_openai_adapter.c` | Tests for provider vtable implementation |
| `tests/unit/providers/openai/test_openai_client.c` | Tests for request serialization to Chat Completions API |
| `tests/unit/providers/openai/test_openai_streaming.c` | Tests for SSE streaming response handling |
| `tests/unit/providers/openai/test_openai_errors.c` | Tests for error response parsing and mapping |

Fixture files to create:

| File | Purpose |
|------|---------|
| `tests/fixtures/openai/response_basic.json` | Standard chat completion response |
| `tests/fixtures/openai/response_tool_call.json` | Response with tool_calls array |
| `tests/fixtures/openai/response_reasoning.json` | Response from o-series model with reasoning |
| `tests/fixtures/openai/stream_basic.txt` | SSE stream for basic completion |
| `tests/fixtures/openai/stream_reasoning.txt` | SSE stream from reasoning model |
| `tests/fixtures/openai/error_auth.json` | 401 authentication error |
| `tests/fixtures/openai/error_rate_limit.json` | 429 rate limit error |

## Behaviors

**Adapter Tests:**
- Provider vtable correctly implements all interface functions
- `create()` initializes OpenAI-specific structures
- `send()` calls OpenAI request builder and HTTP client
- `destroy()` cleans up resources properly

**Request Serialization Tests:**
- Build Chat Completions API request with correct structure
- Include `model`, `messages` fields
- Serialize system/user/assistant messages in messages array
- Map thinking level to reasoning effort for o-series models:
  - none/low -> reasoning_effort: "low"
  - medium -> reasoning_effort: "medium"
  - high/extended -> reasoning_effort: "high"
- Include tool definitions in `tools` array
- Set appropriate headers (API key, content-type, organization)

**Response Parsing Tests:**
- Parse standard chat completion response
- Extract text from message content
- Extract tool calls from tool_calls array
- Parse tool call arguments as JSON string
- Handle reasoning content from o-series models
- Calculate token usage from response

**Streaming Tests:**
- Parse SSE events with data: prefix
- Parse delta chunks with content
- Parse delta chunks with tool_calls
- Accumulate tool call arguments across chunks
- Parse [DONE] marker for stream end
- Normalize events to common format

**Error Handling Tests:**
- Parse error response JSON structure
- Extract error type and message
- Map to error categories correctly
- Identify retryable errors

## Test Scenarios

**Adapter:**
- Create and destroy adapter
- Send request returns OK with valid response
- Send request returns ERR on failure

**Request Serialization:**
- Build request with system and user messages
- Build request for o1 model with reasoning effort
- Build request for gpt-4 model without reasoning effort
- Build request with tool definitions
- Build request without optional fields
- Verify JSON structure matches API spec

**Response Parsing:**
- Parse response with message content
- Parse response with tool_calls array
- Parse response from reasoning model
- Extract tool call ID and arguments
- Parse tool arguments from JSON string

**Streaming:**
- Stream basic completion with content deltas
- Stream completion with tool_calls deltas
- Stream from reasoning model
- Accumulate tool call arguments across multiple chunks
- Handle stream termination with [DONE]
- Normalize all delta types to common format

**Error Handling:**
- Parse authentication error (401)
- Parse rate limit error (429)
- Parse context length error (400)
- Parse model not found error (404)
- Map errors to correct categories

## Postconditions

- [ ] 100% line coverage on OpenAI adapter code
- [ ] All fixtures validate against real API structure
- [ ] Tests cover both standard and reasoning models
- [ ] Tool call argument parsing tested (JSON string format)
- [ ] Reasoning effort mapping tested
- [ ] Error mapping covers all HTTP status codes
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
