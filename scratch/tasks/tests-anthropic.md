# Task: Create Anthropic Provider Tests

**Layer:** 4
**Model:** sonnet/none
**Depends on:** anthropic-core.md, anthropic-request.md, anthropic-response.md, anthropic-streaming.md, tests-provider-common.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/anthropic/` - Anthropic provider implementation

**Plan:**
- `scratch/plan/testing-strategy.md` - Provider-specific test organization

## Objective

Create comprehensive test suite for Anthropic provider including request serialization, response parsing, streaming event handling, thinking budget calculation, and error mapping. Tests verify correct implementation of Anthropic Messages API format with all content types.

## Interface

Test files to create:

| File | Purpose |
|------|---------|
| `tests/unit/providers/anthropic/test_anthropic_adapter.c` | Tests for provider vtable implementation |
| `tests/unit/providers/anthropic/test_anthropic_client.c` | Tests for request serialization to Messages API |
| `tests/unit/providers/anthropic/test_anthropic_streaming.c` | Tests for SSE event handling and normalization |
| `tests/unit/providers/anthropic/test_anthropic_errors.c` | Tests for error response parsing and mapping |

Fixture files to create:

| File | Purpose |
|------|---------|
| `tests/fixtures/anthropic/response_basic.json` | Standard completion response |
| `tests/fixtures/anthropic/response_thinking.json` | Response with thinking content block |
| `tests/fixtures/anthropic/response_tool_call.json` | Response with tool use content block |
| `tests/fixtures/anthropic/stream_basic.txt` | SSE stream for basic completion |
| `tests/fixtures/anthropic/stream_thinking.txt` | SSE stream with thinking deltas |
| `tests/fixtures/anthropic/error_auth.json` | 401 authentication error |
| `tests/fixtures/anthropic/error_rate_limit.json` | 429 rate limit error |

## Behaviors

**Adapter Tests:**
- Provider vtable correctly implements all interface functions
- `create()` initializes Anthropic-specific structures
- `send()` calls Anthropic request builder and HTTP client
- `destroy()` cleans up resources properly

**Request Serialization Tests:**
- Build Messages API request with correct structure
- Include `model`, `max_tokens`, `messages` fields
- Serialize system message in `system` field
- Serialize user/assistant messages in `messages` array
- Include thinking budget in extended thinking parameter
- Include tool definitions in `tools` array
- Set appropriate headers (API key, version, content-type)

**Response Parsing Tests:**
- Parse standard completion response
- Extract text from `text` content blocks
- Extract thinking from `thinking` content blocks
- Extract tool calls from `tool_use` content blocks
- Calculate thinking tokens from response metadata
- Handle multiple content blocks in order

**Streaming Tests:**
- Parse `message_start` event
- Parse `content_block_start` event for each type
- Parse `content_block_delta` events
- Parse `message_delta` event with usage
- Parse `message_stop` event
- Normalize events to common format
- Calculate thinking budget from deltas

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
- Build request with thinking budget (extended thinking)
- Build request with tool definitions
- Build request without optional fields
- Verify JSON structure matches API spec

**Response Parsing:**
- Parse response with single text block
- Parse response with thinking block followed by text block
- Parse response with tool use block
- Parse response with multiple content blocks
- Extract thinking token count from usage metadata

**Streaming:**
- Stream basic completion with text deltas
- Stream completion with thinking deltas
- Stream completion with tool use
- Handle stream termination gracefully
- Normalize all event types to common format

**Error Handling:**
- Parse authentication error (401)
- Parse rate limit error (429)
- Parse overloaded error (529)
- Parse validation error (400)
- Map errors to correct categories

## Postconditions

- [ ] 100% line coverage on Anthropic adapter code
- [ ] All fixtures validate against real API structure
- [ ] Tests cover all content block types
- [ ] Thinking budget calculation tested
- [ ] Error mapping covers all HTTP status codes
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
