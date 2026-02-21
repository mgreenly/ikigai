# Task: Anthropic API Contract Validation

**Model:** sonnet/thinking
**Depends on:** anthropic-send-impl.md, anthropic-streaming.md

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
- `src/providers/anthropic/anthropic.h` - Anthropic provider
- `src/providers/anthropic/adapter.c` - Anthropic adapter implementation
- `src/providers/provider.h` - Vtable interface
- `tests/unit/providers/anthropic/` - Existing Anthropic tests

**Fixtures:**
- `tests/fixtures/vcr/anthropic/` - VCR cassettes for Anthropic

## Objective

Create a contract validation test suite that verifies the Anthropic provider implementation correctly handles real API behavior patterns captured in VCR cassettes. This validates that our implementation matches the Anthropic API contract expectations for request format, response parsing, streaming events, error handling, and provider-specific features (thinking tokens, extended thinking).

## Rationale

The Anthropic provider must correctly implement the Claude API contract. VCR cassettes capture real API responses, and contract tests validate that our code correctly:
- Serializes requests in the expected format
- Parses all response types (text, tool calls, thinking)
- Handles SSE streaming events properly
- Maps error responses to correct error categories
- Extracts thinking tokens and budgets correctly

Contract validation ensures our implementation stays aligned with the real API as it evolves.

## Interface

### Test Functions to Implement

| Function | Purpose |
|----------|---------|
| `test_contract_simple_text` | Validate basic text response parsing |
| `test_contract_tool_call` | Validate tool call request/response handling |
| `test_contract_thinking_tokens` | Validate thinking token extraction and reporting |
| `test_contract_extended_thinking` | Validate extended thinking (high budget) responses |
| `test_contract_streaming_text` | Validate SSE streaming text deltas |
| `test_contract_streaming_thinking` | Validate SSE streaming thinking events |
| `test_contract_streaming_tool_call` | Validate SSE streaming tool call events |
| `test_contract_error_auth` | Validate 401 authentication error mapping |
| `test_contract_error_rate_limit` | Validate 429 rate limit error mapping |
| `test_contract_error_invalid_request` | Validate 400 invalid request error mapping |
| `test_contract_multi_turn` | Validate multi-turn conversation handling |
| `test_contract_token_usage` | Validate token usage reporting (input/output/cache tokens) |

### Helper Functions

| Function | Purpose |
|----------|---------|
| `create_anthropic_provider(ctx, api_key)` | Create Anthropic provider with test credentials |
| `verify_response_structure(resp)` | Verify ik_response_t has expected fields populated |
| `verify_thinking_data(resp)` | Verify thinking tokens extracted correctly |
| `verify_tool_call_format(tool_call)` | Verify tool call has valid structure |
| `verify_stream_event_sequence(events)` | Verify stream events follow Anthropic SSE protocol |
| `verify_error_mapping(result, expected_category)` | Verify error mapped to correct category |

## Behaviors

### VCR Cassette Validation

**Response Validation:**
- Content blocks: correct count, correct types, text matches cassette
- Tool calls: name, id, arguments match cassette structure
- Thinking blocks: extracted correctly when present
- Finish reason: matches cassette stop_reason
- Token usage: input_tokens, output_tokens, cache_creation_tokens, cache_read_tokens
- Model echo: returns model from response

**SSE Stream Validation:**
- Event sequence: message_start → content_block_start → deltas → content_block_stop → message_delta → message_stop
- Text deltas: properly accumulated into final content
- Thinking events: content_block_start/delta/stop for thinking blocks
- Tool call events: content_block_start/delta/stop for tool_use blocks
- Usage data: extracted from message_delta and message_stop events
- Finish reason: extracted from message_delta event

**Error Response Validation:**
- HTTP 401: maps to IK_ERR_CAT_AUTH with api key error message
- HTTP 429: maps to IK_ERR_CAT_RATE_LIMIT with retry_after extracted from headers
- HTTP 400: maps to IK_ERR_CAT_INVALID_ARG with validation error details
- HTTP 500: maps to IK_ERR_CAT_SERVER with server error details
- Error details: preserve provider error type and message

### Test Request Fixtures

Create standard request fixtures for contract tests:

```c
// Simple text request
ik_request_t *fixture_anthropic_simple_text(TALLOC_CTX *ctx);
// Returns request with: "What is 2+2?" for claude-sonnet-4-5

// Tool call request
ik_request_t *fixture_anthropic_tool_call(TALLOC_CTX *ctx);
// Returns request with get_weather tool definition

// Thinking request (medium budget)
ik_request_t *fixture_anthropic_thinking_med(TALLOC_CTX *ctx);
// Returns request with thinking level = IK_THINKING_MED

// Extended thinking request (high budget)
ik_request_t *fixture_anthropic_thinking_high(TALLOC_CTX *ctx);
// Returns request with thinking level = IK_THINKING_HIGH

// Multi-turn conversation
ik_request_t *fixture_anthropic_multi_turn(TALLOC_CTX *ctx);
// Returns request with user/assistant/user message sequence

// Invalid model request
ik_request_t *fixture_anthropic_invalid_model(TALLOC_CTX *ctx);
// Returns request with non-existent model name
```

### VCR Cassette Requirements

Tests MUST use VCR cassettes for deterministic validation:
- Each test has matching VCR cassette in `tests/fixtures/vcr/anthropic/`
- Cassettes contain real API responses (captured with VCR_RECORD=1)
- Request validation ensures our code generates API-compliant requests
- Response parsing validates against real API response structure

Cassette naming convention: `test_contract_{scenario}.jsonl`

### Anthropic-Specific Behaviors

**Thinking Tokens:**
- `thinking` parameter in request maps to token budget
- IK_THINKING_MIN → 1,024 tokens (minimum)
- IK_THINKING_LOW → 22,016 tokens (1/3 range)
- IK_THINKING_MED → 43,008 tokens (2/3 range)
- IK_THINKING_HIGH → 64,000 tokens (maximum)

**Content Block Types:**
- `text` blocks → IK_CONTENT_TEXT
- `thinking` blocks → IK_CONTENT_THINKING
- `tool_use` blocks → IK_CONTENT_TOOL_CALL

**Stop Reasons:**
- `end_turn` → IK_FINISH_STOP
- `max_tokens` → IK_FINISH_LENGTH
- `tool_use` → IK_FINISH_TOOL_USE
- `stop_sequence` → IK_FINISH_STOP

**Cache Tokens:**
- Anthropic supports prompt caching
- Responses include `cache_creation_tokens` and `cache_read_tokens`
- Validate these are extracted and stored in usage metadata

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
├── anthropic_contract_test.c      - Main contract test suite
├── anthropic_contract_fixtures.c  - Shared request fixtures
├── anthropic_contract_fixtures.h  - Fixture declarations
```

## Test Scenarios

### Simple Text Contract
1. Load VCR cassette: `test_contract_simple_text.jsonl`
2. Create simple text request fixture
3. Send through Anthropic provider (VCR replays response)
4. Verify response: content matches cassette, usage correct
5. Assert: all fields extracted correctly

### Tool Call Contract
1. Load VCR cassette: `test_contract_tool_call.jsonl`
2. Create tool call request with definition
3. Send through provider
4. Verify: tool_use block parsed, name/id/arguments extracted
5. Assert: tool call structure matches API contract

### Thinking Tokens Contract
1. Load VCR cassette: `test_contract_thinking_med.jsonl`
2. Create request with IK_THINKING_MED
3. Send through provider
4. Verify: thinking block extracted, token count reported
5. Assert: thinking parameter serialized correctly

### Extended Thinking Contract
1. Load VCR cassette: `test_contract_thinking_high.jsonl`
2. Create request with IK_THINKING_HIGH (64k tokens)
3. Send through provider
4. Verify: large thinking output handled, usage tracking correct
5. Assert: extended thinking responses parsed correctly

### Streaming Text Contract
1. Load VCR cassette: `test_contract_streaming_text.jsonl`
2. Create streaming request
3. Collect all stream events
4. Verify: event sequence matches Anthropic SSE protocol
5. Assert: text deltas accumulate to final content

### Streaming Thinking Contract
1. Load VCR cassette: `test_contract_streaming_thinking.jsonl`
2. Create streaming request with thinking enabled
3. Collect stream events
4. Verify: thinking content_block events parsed
5. Assert: thinking deltas accumulated separately from text

### Streaming Tool Call Contract
1. Load VCR cassette: `test_contract_streaming_tool_call.jsonl`
2. Create streaming request that triggers tool use
3. Collect events
4. Verify: tool_use content_block events parsed
5. Assert: tool arguments accumulated from deltas

### Auth Error Contract
1. Load VCR cassette: `test_contract_error_auth.jsonl` (401 response)
2. Create request with invalid API key
3. Send through provider
4. Verify: result is IK_ERR_CAT_AUTH, error message preserved
5. Assert: authentication errors mapped correctly

### Rate Limit Error Contract
1. Load VCR cassette: `test_contract_error_rate_limit.jsonl` (429 response)
2. Create request that triggers rate limit
3. Send through provider
4. Verify: result is IK_ERR_CAT_RATE_LIMIT, retry_after extracted
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
4. Verify: response handles conversation context
5. Assert: multi-message requests handled per API contract

### Token Usage Contract
1. Load VCR cassette: `test_contract_token_usage.jsonl`
2. Create request, verify response
3. Extract usage: input_tokens, output_tokens, cache tokens
4. Verify: all usage fields extracted from response
5. Assert: usage reporting matches API contract

## Postconditions

- [ ] `tests/contract_validations/anthropic_contract_test.c` exists
- [ ] `tests/contract_validations/anthropic_contract_fixtures.c` exists
- [ ] `tests/contract_validations/anthropic_contract_fixtures.h` exists
- [ ] All contract tests use VCR cassettes (no live API)
- [ ] VCR cassettes exist for all test scenarios in `tests/fixtures/vcr/anthropic/`
- [ ] Simple text contract test passes
- [ ] Tool call contract test passes
- [ ] Thinking tokens contract test passes
- [ ] Extended thinking contract test passes
- [ ] Streaming text contract test passes
- [ ] Streaming thinking contract test passes
- [ ] Streaming tool call contract test passes
- [ ] Auth error contract test passes
- [ ] Rate limit error contract test passes
- [ ] Invalid request error contract test passes
- [ ] Multi-turn contract test passes
- [ ] Token usage contract test passes
- [ ] `make build/tests/contract_validations/anthropic_contract_test` succeeds
- [ ] `./build/tests/contract_validations/anthropic_contract_test` passes
- [ ] Makefile updated with contract test sources
- [ ] `make check` passes
- [ ] Skip escape hatch documented and logs warning

- [ ] Changes committed to git with message: `task: contract-anthropic.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Build contract tests
make build/tests/contract_validations/anthropic_contract_test

# Run contract validation
./build/tests/contract_validations/anthropic_contract_test

# Verify all tests pass
echo $?  # Should be 0

# Verify escape hatch works (should warn)
IK_SKIP_CONTRACT_VALIDATION=1 ./build/tests/contract_validations/anthropic_contract_test
# Should log warning about skipped validation

# Verify cassettes exist
ls tests/fixtures/vcr/anthropic/test_contract_*.jsonl
```

## Integration with VCR System

Contract tests leverage the VCR system designed in `vcr-cassettes.md`:

1. **Playback Mode (default):** Tests load cassettes, VCR replays responses
2. **Capture Mode:** `VCR_RECORD=1` makes real API calls to update cassettes
3. **Request Verification:** VCR verifies our requests match recorded requests
4. **Credential Redaction:** API keys automatically redacted in cassettes

To update cassettes with fresh API responses:
```bash
# Requires ANTHROPIC_API_KEY in environment or credentials.json
VCR_RECORD=1 ./build/tests/contract_validations/anthropic_contract_test
```

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
