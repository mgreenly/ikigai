# Task: Create OpenAI Provider Tests

**Phase:** Testing
**Depends on:** 12-openai-native, 16-tests-provider-common

## Objective

Create comprehensive test suite for OpenAI provider.

## Deliverables

1. Create `tests/unit/providers/openai/`:
   - `test_openai_adapter.c` - Vtable implementation
   - `test_openai_client.c` - Request serialization
   - `test_openai_streaming.c` - SSE event handling
   - `test_openai_errors.c` - Error mapping

2. Create fixtures `tests/fixtures/openai/`:
   - `response_basic.json`
   - `response_tool_call.json`
   - `response_reasoning.json`
   - `stream_basic.txt`
   - `stream_reasoning.txt`
   - `error_auth.json`
   - `error_rate_limit.json`

3. Test coverage:
   - Chat Completions API format
   - Responses API format (if enabled)
   - Tool call with JSON string arguments
   - Reasoning effort mapping
   - Error mapping

## Reference

- `scratch/plan/testing-strategy.md` - Test organization

## Verification

- 100% coverage on OpenAI adapter
- Both API formats tested
