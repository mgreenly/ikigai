# Task: Google AI API Contract Validation

**Model:** sonnet/thinking
**Depends on:** google-send-impl.md, google-streaming.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/google/google.h` - Google provider
- `src/providers/google/adapter.c` - Google adapter implementation
- `src/providers/provider.h` - Vtable interface
- `tests/unit/providers/google/` - Existing Google tests

**Fixtures:**
- `tests/fixtures/vcr/google/` - VCR cassettes for Google

## Objective

Create a contract validation test suite that verifies the Google AI (Gemini) provider implementation correctly handles real API behavior patterns captured in VCR cassettes. This validates that our implementation matches the Google Generative AI API contract expectations for request format, response parsing, streaming events, error handling, and provider-specific features (thinking config, grounding).

## Rationale

The Google provider must correctly implement the Gemini API contract. VCR cassettes capture real API responses, and contract tests validate that our code correctly:
- Serializes requests in the expected Gemini REST API format
- Parses all response types (text, tool calls, thinking)
- Handles NDJSON streaming format (not SSE like others)
- Maps error responses to correct error categories
- Extracts thinking configuration and grounding metadata
- Generates UUID tool call IDs (Google doesn't provide them)

Contract validation ensures our implementation stays aligned with the real API as it evolves.

## Interface

### Test Functions to Implement

| Function | Purpose |
|----------|---------|
| `test_contract_simple_text` | Validate basic text response parsing |
| `test_contract_tool_call` | Validate tool call request/response handling |
| `test_contract_tool_call_id_generation` | Validate UUID generation for tool calls (Google-specific) |
| `test_contract_thinking_config` | Validate thinking config parameter serialization |
| `test_contract_thinking_response` | Validate thinking content extraction from thoughts field |
| `test_contract_streaming_text` | Validate NDJSON streaming text chunks |
| `test_contract_streaming_thinking` | Validate NDJSON streaming thinking chunks |
| `test_contract_streaming_tool_call` | Validate NDJSON streaming tool call chunks |
| `test_contract_error_auth` | Validate 403 authentication error mapping |
| `test_contract_error_rate_limit` | Validate 429 rate limit error mapping |
| `test_contract_error_invalid_request` | Validate 400 invalid request error mapping |
| `test_contract_multi_turn` | Validate multi-turn conversation handling |
| `test_contract_token_usage` | Validate token usage reporting (promptTokenCount/candidatesTokenCount) |
| `test_contract_grounding` | Validate grounding metadata extraction (if applicable) |

### Helper Functions

| Function | Purpose |
|----------|---------|
| `create_google_provider(ctx, api_key)` | Create Google provider with test credentials |
| `verify_response_structure(resp)` | Verify ik_response_t has expected fields populated |
| `verify_thinking_data(resp)` | Verify thinking content extracted from thoughts field |
| `verify_tool_call_format(tool_call)` | Verify tool call has valid UUID and structure |
| `verify_uuid_format(id)` | Verify generated UUID is 22-char base64url |
| `verify_stream_event_sequence(events)` | Verify stream events follow Google NDJSON protocol |
| `verify_error_mapping(result, expected_category)` | Verify error mapped to correct category |

## Behaviors

### VCR Cassette Validation

**Response Validation:**
- Content parts: correct count, text matches cassette
- Tool calls: functionCall extracted, UUID generated for id
- Thinking content: extracted from thoughts field when present
- Finish reason: maps from Google enum (STOP, MAX_TOKENS, etc.)
- Token usage: promptTokenCount, candidatesTokenCount, totalTokenCount
- Model echo: returns model from request (Google doesn't echo)
- Safety ratings: extracted when present

**NDJSON Stream Validation:**
- Format: Each line is complete JSON object (not SSE)
- Chunks: `{"candidates":[{"content":{"parts":[...]}}]}`
- Text deltas: parts array contains text deltas
- Thinking chunks: thoughts field contains thinking deltas
- Tool call chunks: functionCall field contains tool data
- Usage data: usageMetadata in final chunk
- Finish reason: finishReason in final candidate

**Error Response Validation:**
- HTTP 403: maps to IK_ERR_CAT_AUTH with API key error message
- HTTP 429: maps to IK_ERR_CAT_RATE_LIMIT with retry info
- HTTP 400: maps to IK_ERR_CAT_INVALID_ARG with validation error details
- HTTP 500: maps to IK_ERR_CAT_SERVER with server error details
- Error details: preserve Google error code and message

### Test Request Fixtures

Create standard request fixtures for contract tests:

```c
// Simple text request
ik_request_t *fixture_google_simple_text(TALLOC_CTX *ctx);
// Returns request with: "What is 2+2?" for gemini-3.0-flash

// Tool call request
ik_request_t *fixture_google_tool_call(TALLOC_CTX *ctx);
// Returns request with get_weather function declaration

// Thinking request (medium budget)
ik_request_t *fixture_google_thinking_med(TALLOC_CTX *ctx);
// Returns request with thinking level = IK_THINKING_MED

// Extended thinking request (high budget)
ik_request_t *fixture_google_thinking_high(TALLOC_CTX *ctx);
// Returns request with thinking level = IK_THINKING_HIGH

// Multi-turn conversation
ik_request_t *fixture_google_multi_turn(TALLOC_CTX *ctx);
// Returns request with user/model/user message sequence (Google uses "model" not "assistant")

// Invalid model request
ik_request_t *fixture_google_invalid_model(TALLOC_CTX *ctx);
// Returns request with non-existent model name
```

### VCR Cassette Requirements

Tests MUST use VCR cassettes for deterministic validation:
- Each test has matching VCR cassette in `tests/fixtures/vcr/google/`
- Cassettes contain real API responses (captured with VCR_RECORD=1)
- Request validation ensures our code generates API-compliant requests
- Response parsing validates against real API response structure

Cassette naming convention: `test_contract_{scenario}.jsonl`

### Google-Specific Behaviors

**Thinking Configuration:**
- `thinkingConfig` parameter in request maps to token budget
- IK_THINKING_NONE → thinkingBudget 128 (minimum, cannot disable)
- IK_THINKING_LOW → thinkingBudget 11,008 tokens
- IK_THINKING_MED → thinkingBudget 21,888 tokens
- IK_THINKING_HIGH → thinkingBudget 32,768 tokens

**Tool Call ID Generation:**
- Google API does NOT provide tool call IDs
- Provider MUST generate UUID for each tool call
- Format: 22-character base64url-encoded UUID
- Consistent with provider ID generation pattern

**Content Parts Structure:**
- Text: `{"text": "content"}`
- Tool call: `{"functionCall": {"name": "...", "args": {...}}}`
- Thinking: extracted from `thoughts` field (separate from parts)

**Role Mapping:**
- user → user
- assistant → model (Google uses "model" not "assistant")
- tool → function (tool responses use functionResponse)

**Finish Reasons:**
- `STOP` → IK_FINISH_STOP
- `MAX_TOKENS` → IK_FINISH_LENGTH
- `SAFETY` → IK_FINISH_CONTENT_FILTER (Google-specific)
- `RECITATION` → IK_FINISH_CONTENT_FILTER (Google-specific)
- `OTHER` → IK_FINISH_STOP

**Token Counting:**
- Google provides: promptTokenCount, candidatesTokenCount, totalTokenCount
- Map to: input_tokens, output_tokens, total_tokens

**Streaming Format:**
- NDJSON (newline-delimited JSON), NOT Server-Sent Events
- Each line is complete JSON object
- No `event:` or `data:` prefixes like SSE

### Failure Handling

On contract validation failure:
- Log which field/event failed validation
- Include expected value from cassette
- Include actual value from our parsing
- Include full request/response for debugging
- Fail test with descriptive message

### Environment Variable

Add `IK_SKIP_CONTRACT_VALIDATION=1` escape hatch for emergency situations. Log a prominent warning when this is used.

## Directory Structure

```
tests/contract_validations/
├── google_contract_test.c      - Main contract test suite
├── google_contract_fixtures.c  - Shared request fixtures
├── google_contract_fixtures.h  - Fixture declarations
```

## Test Scenarios

### Simple Text Contract
1. Load VCR cassette: `test_contract_simple_text.jsonl`
2. Create simple text request fixture
3. Send through Google provider (VCR replays response)
4. Verify response: content matches cassette, usage correct
5. Assert: all fields extracted correctly

### Tool Call Contract
1. Load VCR cassette: `test_contract_tool_call.jsonl`
2. Create tool call request with function declaration
3. Send through provider
4. Verify: functionCall parsed, name/args extracted
5. Assert: tool call structure matches API contract

### Tool Call ID Generation Contract
1. Load VCR cassette: `test_contract_tool_call_id_generation.jsonl`
2. Create request that triggers tool call
3. Send through provider
4. Verify: tool call ID is 22-char base64url UUID
5. Assert: UUID generation works correctly (Google doesn't provide IDs)

### Thinking Config Contract
1. Load VCR cassette: `test_contract_thinking_config.jsonl`
2. Create request with IK_THINKING_MED
3. Verify request: thinkingConfig parameter serialized
4. Send through provider
5. Assert: thinking configuration parameter formatted correctly

### Thinking Response Contract
1. Load VCR cassette: `test_contract_thinking_response.jsonl`
2. Create request with thinking enabled
3. Send through provider
4. Verify: thoughts field extracted, thinking content populated
5. Assert: thinking extraction matches API contract

### Streaming Text Contract
1. Load VCR cassette: `test_contract_streaming_text.jsonl`
2. Create streaming request
3. Collect all stream events
4. Verify: NDJSON chunks parsed, event sequence correct
5. Assert: text parts accumulate to final content

### Streaming Thinking Contract
1. Load VCR cassette: `test_contract_streaming_thinking.jsonl`
2. Create streaming request with thinking enabled
3. Collect stream events
4. Verify: thoughts field chunks parsed
5. Assert: thinking deltas accumulated correctly

### Streaming Tool Call Contract
1. Load VCR cassette: `test_contract_streaming_tool_call.jsonl`
2. Create streaming request that triggers function call
3. Collect events
4. Verify: functionCall chunks parsed, args accumulated
5. Assert: tool call streaming matches API contract

### Auth Error Contract
1. Load VCR cassette: `test_contract_error_auth.jsonl` (403 response)
2. Create request with invalid API key
3. Send through provider
4. Verify: result is IK_ERR_CAT_AUTH, error message preserved
5. Assert: authentication errors mapped correctly

### Rate Limit Error Contract
1. Load VCR cassette: `test_contract_error_rate_limit.jsonl` (429 response)
2. Create request that triggers rate limit
3. Send through provider
4. Verify: result is IK_ERR_CAT_RATE_LIMIT, retry info extracted
5. Assert: rate limit handling matches API contract

### Invalid Request Error Contract
1. Load VCR cassette: `test_contract_error_invalid_request.jsonl` (400 response)
2. Create request with invalid parameters
3. Send through provider
4. Verify: result is IK_ERR_CAT_INVALID_ARG, validation error details preserved
5. Assert: client errors mapped correctly

### Multi-Turn Contract
1. Load VCR cassette: `test_contract_multi_turn.jsonl`
2. Create multi-turn conversation request
3. Send through provider
4. Verify: role mapping (user/model) correct, context preserved
5. Assert: multi-message requests handled per API contract

### Token Usage Contract
1. Load VCR cassette: `test_contract_token_usage.jsonl`
2. Create request, verify response
3. Extract usage: promptTokenCount, candidatesTokenCount, totalTokenCount
4. Verify: all usage fields extracted from usageMetadata
5. Assert: usage reporting matches API contract

### Grounding Contract (Optional)
1. Load VCR cassette: `test_contract_grounding.jsonl`
2. Create request with grounding enabled (if applicable)
3. Send through provider
4. Verify: grounding metadata extracted
5. Assert: grounding support matches API contract

## Postconditions

- [ ] `tests/contract_validations/google_contract_test.c` exists
- [ ] `tests/contract_validations/google_contract_fixtures.c` exists
- [ ] `tests/contract_validations/google_contract_fixtures.h` exists
- [ ] All contract tests use VCR cassettes (no live API)
- [ ] VCR cassettes exist for all test scenarios in `tests/fixtures/vcr/google/`
- [ ] Simple text contract test passes
- [ ] Tool call contract test passes
- [ ] Tool call ID generation contract test passes
- [ ] Thinking config contract test passes
- [ ] Thinking response contract test passes
- [ ] Streaming text contract test passes
- [ ] Streaming thinking contract test passes
- [ ] Streaming tool call contract test passes
- [ ] Auth error contract test passes
- [ ] Rate limit error contract test passes
- [ ] Invalid request error contract test passes
- [ ] Multi-turn contract test passes
- [ ] Token usage contract test passes
- [ ] `make build/tests/contract_validations/google_contract_test` succeeds
- [ ] `./build/tests/contract_validations/google_contract_test` passes
- [ ] Makefile updated with contract test sources
- [ ] `make check` passes
- [ ] Skip escape hatch documented and logs warning

- [ ] Changes committed to git with message: `task: contract-google.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Build contract tests
make build/tests/contract_validations/google_contract_test

# Run contract validation
./build/tests/contract_validations/google_contract_test

# Verify all tests pass
echo $?  # Should be 0

# Verify escape hatch works (should warn)
IK_SKIP_CONTRACT_VALIDATION=1 ./build/tests/contract_validations/google_contract_test
# Should log warning about skipped validation

# Verify cassettes exist
ls tests/fixtures/vcr/google/test_contract_*.jsonl
```

## Integration with VCR System

Contract tests leverage the VCR system designed in `vcr-cassettes.md`:

1. **Playback Mode (default):** Tests load cassettes, VCR replays responses
2. **Capture Mode:** `VCR_RECORD=1` makes real API calls to update cassettes
3. **Request Verification:** VCR verifies our requests match recorded requests
4. **Credential Redaction:** API keys automatically redacted in cassettes

To update cassettes with fresh API responses:
```bash
# Requires GOOGLE_API_KEY in environment or credentials.json
VCR_RECORD=1 ./build/tests/contract_validations/google_contract_test
```

## Google API Differences Summary

Key differences from Anthropic/OpenAI that contract tests must validate:

1. **Streaming format:** NDJSON instead of SSE
2. **Role names:** "model" instead of "assistant"
3. **Tool call IDs:** Generated by provider (UUID), not from API
4. **Thinking location:** `thoughts` field, not content blocks
5. **Token field names:** promptTokenCount/candidatesTokenCount
6. **Auth header:** `x-goog-api-key` instead of Authorization
7. **Finish reasons:** Includes SAFETY and RECITATION

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
