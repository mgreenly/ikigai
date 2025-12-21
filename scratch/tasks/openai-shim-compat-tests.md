# Task: OpenAI Shim Compatibility Tests

**Layer:** 2
**Model:** sonnet/thinking
**Depends on:** openai-shim-send.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test patterns
- `/load makefile` - Test execution

**Source:**
- `tests/unit/openai/` - Existing OpenAI unit tests
- `tests/integration/` - Existing integration test patterns
- `src/providers/openai/shim.h` - Shim interface

**Plan:**
- `scratch/plan/testing-strategy.md` - Mock HTTP patterns

## Objective

Create comprehensive compatibility tests verifying the OpenAI shim produces identical results to direct client usage. These tests catch transformation bugs by comparing shim output against known-good existing behavior.

## Interface

### Test Files to Create

| File | Purpose |
|------|---------|
| `tests/unit/providers/openai/shim_request_test.c` | Request transformation unit tests |
| `tests/unit/providers/openai/shim_response_test.c` | Response transformation unit tests |
| `tests/unit/providers/openai/shim_send_test.c` | Send integration tests with mocked HTTP |
| `tests/unit/providers/openai/shim_compat_test.c` | Side-by-side comparison tests |

## Behaviors

### Request Transform Tests

Verify each message type transforms correctly:

| Test | Input | Expected Output |
|------|-------|-----------------|
| text_user | ik_message_t(USER, "hello") | ik_msg_t(kind="user", content="hello") |
| text_assistant | ik_message_t(ASSISTANT, "hi") | ik_msg_t(kind="assistant", content="hi") |
| tool_call | content_block(TOOL_CALL, id, name, args) | ik_msg_t(kind="tool_call", data_json=...) |
| tool_result | content_block(TOOL_RESULT, id, content) | ik_msg_t(kind="tool_result", data_json=...) |
| system_prompt | req->system_prompt with 2 blocks | First message kind="system", content=concatenated |

### Response Transform Tests

| Test | Input (ik_msg_t) | Expected Output (ik_response_t) |
|------|------------------|--------------------------------|
| text_response | kind="assistant", content="hello" | content[0].type=TEXT, text="hello" |
| tool_call_response | kind="tool_call", data_json={...} | content[0].type=TOOL_CALL, id/name/args extracted |
| finish_stop | from completion with "stop" | finish_reason=IK_FINISH_STOP |
| finish_tool | from completion with "tool_calls" | finish_reason=IK_FINISH_TOOL_USE |

### Compatibility Comparison Tests

For each test case:
1. Build identical request data
2. Execute via shim: `provider->vtable->send()`
3. Execute via direct client: `ik_openai_chat_create()`
4. Compare: serialized request JSON must be identical
5. Compare: response content must be equivalent

Test cases:
- Simple text conversation
- Multi-turn conversation
- Conversation with tool call
- Conversation with tool result
- System prompt with user message

### Round-Trip Tests

Verify data survives transformation in both directions:

1. Create normalized request with all field types
2. Transform to legacy format
3. Serialize to JSON
4. Verify JSON structure
5. Create mock response in legacy format
6. Transform to normalized format
7. Verify all fields preserved

### Error Handling Tests

| Test | Trigger | Expected |
|------|---------|----------|
| missing_credentials | No API key in credentials | ERR_MISSING_CREDENTIALS |
| empty_messages | Request with 0 messages | ERR_INVALID_ARG |
| malformed_tool_call | Invalid data_json | ERR_PARSE |
| http_error | Mock 500 response | Error propagated |

## Test Patterns

### Mock HTTP Setup

Use existing mock patterns from `tests/unit/openai/`:
```c
// Set up mock response before calling shim
ik_test_mock_http_response(200, fixture_json);

// Call through shim
res_t result = provider->vtable->send(provider->impl_ctx, &req, &resp);

// Verify
ck_assert(is_ok(&result));
```

### Fixture Files

Create fixtures in `tests/fixtures/openai/`:
- `simple_response.json` - Basic text response
- `tool_call_response.json` - Response with tool call
- `error_response.json` - Error response format

## Postconditions

- [ ] All shim unit test files exist and compile
- [ ] Request transformation tests pass
- [ ] Response transformation tests pass
- [ ] Compatibility comparison tests pass
- [ ] Round-trip tests pass
- [ ] Error handling tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
- [ ] All existing `tests/unit/openai/` tests still pass
