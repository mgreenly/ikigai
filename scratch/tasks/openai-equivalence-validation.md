# Task: Validate OpenAI Native vs Shim Equivalence

**Model:** sonnet/thinking
**Depends on:** openai-send-impl.md, openai-streaming-chat.md, openai-streaming-responses.md, openai-shim-streaming.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/openai/openai.h` - Native OpenAI provider
- `src/providers/openai/adapter_shim.c` - Shim adapter wrapping legacy code
- `src/providers/provider.h` - Vtable interface
- `tests/unit/providers/openai/` - Existing OpenAI tests

## Objective

Create a validation test suite that runs identical requests through both the OpenAI shim adapter and native provider, comparing outputs for functional equivalence. This task MUST pass before cleanup tasks delete the legacy code. The validation ensures the native implementation correctly replaces the shim without behavioral regressions.

## Rationale

The shim adapter wraps the legacy `src/openai/` code while the native provider reimplements the same functionality. Cleanup tasks will delete both the legacy code and the shim. If the native provider has subtle bugs or behavioral differences, those issues would only surface after the working code is deleted - making recovery difficult.

This validation creates a safety gate: if outputs differ, cleanup is blocked and the issue must be fixed first.

## Interface

### Test Functions to Implement

| Function | Purpose |
|----------|---------|
| `test_equivalence_simple_text` | Compare text-only responses |
| `test_equivalence_tool_call` | Compare tool call generation |
| `test_equivalence_multi_turn` | Compare multi-turn conversation handling |
| `test_equivalence_streaming_text` | Compare streaming text deltas |
| `test_equivalence_streaming_tool_call` | Compare streaming tool call events |
| `test_equivalence_error_handling` | Compare error responses for invalid requests |
| `test_equivalence_token_usage` | Compare token usage reporting |

### Helper Functions

| Function | Purpose |
|----------|---------|
| `create_shim_provider(ctx, api_key)` | Create shim adapter provider |
| `create_native_provider(ctx, api_key)` | Create native provider |
| `compare_responses(resp_a, resp_b)` | Deep compare two ik_response_t structs |
| `compare_stream_events(events_a, events_b)` | Compare collected stream event sequences |
| `collect_stream_events(provider, request)` | Run streaming request, collect all events into array |

## Behaviors

### Comparison Strategy

**Response Comparison:**
- Content blocks: same count, same types, same text (exact match)
- Tool calls: same id pattern (not exact - providers may generate different IDs), same name, same arguments (JSON-equivalent)
- Finish reason: must match exactly
- Token usage: within 5% tolerance (providers may count slightly differently)
- Model echo: both return same model string

**Stream Event Comparison:**
- Event sequence: same event types in same order
- Text deltas: concatenated text matches final response
- Tool call events: same tool name, same final arguments
- Done event: same finish reason

**Semantic vs Exact Matching:**
- Text content: exact match required
- Tool call IDs: pattern match (both generate valid IDs, may differ)
- Tool arguments: JSON-equivalent (key order may differ)
- Timestamps: ignored
- Provider-specific metadata: ignored

### Test Request Fixtures

Create standard request fixtures used by all equivalence tests:

```c
// Simple text request
ik_request_t *fixture_simple_text(TALLOC_CTX *ctx);
// Returns request with: "What is 2+2?"

// Tool call request
ik_request_t *fixture_tool_call(TALLOC_CTX *ctx);
// Returns request with tool definition and prompt that triggers tool use

// Multi-turn conversation
ik_request_t *fixture_multi_turn(TALLOC_CTX *ctx);
// Returns request with user/assistant/user message sequence

// Error-triggering request
ik_request_t *fixture_invalid_model(TALLOC_CTX *ctx);
// Returns request with non-existent model name
```

### Mock Server Requirements

Tests MUST use the mock HTTP server infrastructure to ensure deterministic responses:
- Same mock responses for both providers
- Controlled latency for streaming tests
- Reproducible error conditions

Do NOT use live API calls - results would be non-deterministic.

### Failure Handling

On comparison failure:
- Log detailed diff showing exactly what differs
- Include request that caused the difference
- Include both responses/event sequences
- Fail the test with descriptive message

### Environment Variable

Add `IK_SKIP_EQUIVALENCE_VALIDATION=1` escape hatch for emergency situations where validation must be bypassed. Log a prominent warning when this is used.

## Directory Structure

```
tests/unit/providers/openai/
├── equivalence_test.c      - Main equivalence test suite
├── equivalence_fixtures.c  - Shared request fixtures
├── equivalence_fixtures.h  - Fixture declarations
├── equivalence_compare.c   - Comparison helper functions
├── equivalence_compare.h   - Comparison declarations
```

## Test Scenarios

### Simple Text Equivalence
1. Create identical request with simple prompt
2. Send through shim provider
3. Send through native provider
4. Compare responses: content, finish_reason, usage
5. Assert: all fields match within tolerance

### Tool Call Equivalence
1. Create request with tool definition and triggering prompt
2. Send through both providers
3. Compare: tool name matches, arguments JSON-equivalent
4. Assert: behavioral equivalence

### Streaming Equivalence
1. Create request for streaming
2. Collect all stream events from shim
3. Collect all stream events from native
4. Compare: event types, accumulated text, tool call data
5. Assert: final accumulated state matches

### Error Equivalence
1. Create request that triggers specific error (invalid model)
2. Send through both providers
3. Compare: error category, HTTP status (if applicable)
4. Assert: same error type returned

### Token Usage Equivalence
1. Send same request through both
2. Extract usage from responses
3. Compare: input_tokens, output_tokens within 5%
4. Assert: usage reporting is consistent

## Postconditions

- [ ] `tests/unit/providers/openai/equivalence_test.c` exists
- [ ] `tests/unit/providers/openai/equivalence_fixtures.c` exists
- [ ] `tests/unit/providers/openai/equivalence_compare.c` exists
- [ ] All equivalence tests use mock server (no live API)
- [ ] Simple text equivalence test passes
- [ ] Tool call equivalence test passes
- [ ] Streaming equivalence test passes
- [ ] Error handling equivalence test passes
- [ ] Token usage equivalence test passes
- [ ] `make build/tests/unit/providers/openai/equivalence_test` succeeds
- [ ] `./build/tests/unit/providers/openai/equivalence_test` passes
- [ ] Makefile updated with new test sources
- [ ] `make check` passes
- [ ] Skip escape hatch documented and logs warning

## Verification

```bash
# Build equivalence tests
make build/tests/unit/providers/openai/equivalence_test

# Run equivalence validation
./build/tests/unit/providers/openai/equivalence_test

# Verify all tests pass
echo $?  # Should be 0

# Verify escape hatch works (should warn)
IK_SKIP_EQUIVALENCE_VALIDATION=1 ./build/tests/unit/providers/openai/equivalence_test
# Should log warning about skipped validation
```

## Blocking Relationship

This task MUST complete successfully before:
- `cleanup-openai-source.md` - Deletes legacy `src/openai/` files
- `cleanup-openai-adapter.md` - Deletes shim adapter

If equivalence validation fails, investigate the difference and fix the native provider before proceeding with cleanup.
