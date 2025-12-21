# Task: Create Google Provider Tests

**Layer:** 4
**Model:** sonnet/none
**Depends on:** google-core.md, google-request.md, google-response.md, google-streaming.md, tests-provider-common.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns

**Source:**
- `tests/unit/providers/` - Common provider test patterns
- `src/providers/google/` - Google/Gemini provider implementation

**Plan:**
- `scratch/plan/testing-strategy.md` - Provider-specific test organization

## Objective

Create comprehensive test suite for Google Gemini provider including request serialization with parts structure, response parsing with thought flags, UUID generation for tool calls, version-specific thinking parameters (Gemini 2.5 thinkingBudget vs Gemini 3 thinkingLevel), and thought signature handling.

## Interface

Test files to create:

| File | Purpose |
|------|---------|
| `tests/unit/providers/google/test_google_adapter.c` | Tests for provider vtable implementation |
| `tests/unit/providers/google/test_google_client.c` | Tests for request serialization to Gemini API |
| `tests/unit/providers/google/test_google_streaming.c` | Tests for streaming response handling |
| `tests/unit/providers/google/test_google_errors.c` | Tests for error response parsing and mapping |

Fixture files to create:

| File | Purpose |
|------|---------|
| `tests/fixtures/google/response_basic.json` | Standard completion response |
| `tests/fixtures/google/response_thinking.json` | Response with thought=true parts |
| `tests/fixtures/google/response_function_call.json` | Response with functionCall |
| `tests/fixtures/google/stream_basic.txt` | Streaming response for basic completion |
| `tests/fixtures/google/stream_thinking.txt` | Streaming response with thought parts |
| `tests/fixtures/google/error_auth.json` | 401/403 authentication error |
| `tests/fixtures/google/error_rate_limit.json` | 429 rate limit error |

## Behaviors

**Adapter Tests:**
- Provider vtable correctly implements all interface functions
- `create()` initializes Google-specific structures
- `send()` calls Gemini request builder and HTTP client
- `destroy()` cleans up resources properly

**Request Serialization Tests:**
- Build Gemini API request with `contents` array
- Serialize system message as first user part with role metadata
- Serialize user/assistant messages as parts with text
- Include thinking parameter based on model version:
  - Gemini 2.5: use `thinkingBudget` (token count)
  - Gemini 3.0+: use `thinkingLevel` (low/medium/high)
- Include tool declarations in `tools` array
- Set appropriate headers (API key in URL, content-type)

**Response Parsing Tests:**
- Parse standard completion response
- Extract text from parts without `thought` flag
- Extract thinking from parts with `thought: true`
- Extract function calls from `functionCall` parts
- Generate UUIDs for tool calls (Gemini doesn't provide IDs)
- Handle multiple parts in order
- Detect thought signatures in text content

**Streaming Tests:**
- Parse streaming JSON chunks
- Extract parts from each chunk
- Identify thought parts by flag
- Normalize events to common format
- Handle stream termination

**Error Handling Tests:**
- Parse error response JSON structure
- Extract error code and message
- Map to error categories correctly
- Identify retryable errors

## Test Scenarios

**Adapter:**
- Create and destroy adapter
- Send request returns OK with valid response
- Send request returns ERR on failure

**Request Serialization:**
- Build request with system and user messages
- Build request for Gemini 2.5 with thinkingBudget
- Build request for Gemini 3.0 with thinkingLevel
- Build request with tool declarations
- Build request without optional fields
- Verify JSON structure matches API spec

**Response Parsing:**
- Parse response with single text part
- Parse response with thought part followed by text part
- Parse response with functionCall part
- Parse response with multiple parts
- Generate UUID for tool call
- Detect thought signature in text content

**Streaming:**
- Stream basic completion with text parts
- Stream completion with thought parts
- Stream completion with function call
- Handle stream termination gracefully
- Normalize all chunk types to common format

**Error Handling:**
- Parse authentication error (401/403)
- Parse rate limit error (429)
- Parse quota exceeded error (429 with specific message)
- Parse validation error (400)
- Map errors to correct categories

## Postconditions

- [ ] 100% line coverage on Google adapter code
- [ ] All fixtures validate against real API structure
- [ ] Tests cover both Gemini 2.5 and 3.0 thinking parameters
- [ ] UUID generation for tool calls tested
- [ ] Thought signature detection tested
- [ ] Error mapping covers all HTTP status codes
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
