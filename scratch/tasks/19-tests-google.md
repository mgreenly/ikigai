# Task: Create Google Provider Tests

**Phase:** Testing
**Depends on:** 11-google-provider, 16-tests-provider-common

## Objective

Create comprehensive test suite for Google (Gemini) provider.

## Deliverables

1. Create `tests/unit/providers/google/`:
   - `test_google_adapter.c` - Vtable implementation
   - `test_google_client.c` - Request serialization
   - `test_google_streaming.c` - SSE event handling
   - `test_google_errors.c` - Error mapping

2. Create fixtures `tests/fixtures/google/`:
   - `response_basic.json`
   - `response_thinking.json`
   - `response_function_call.json`
   - `stream_basic.txt`
   - `stream_thinking.txt`
   - `error_auth.json`
   - `error_rate_limit.json`

3. Test coverage:
   - Request serialization with parts structure
   - Response parsing with thought flag
   - UUID generation for tool calls
   - Gemini 2.5 thinkingBudget
   - Gemini 3 thinkingLevel
   - Thought signature handling
   - Error mapping

## Reference

- `scratch/plan/testing-strategy.md` - Test organization

## Verification

- 100% coverage on Google adapter
- Both Gemini 2.5 and 3 tested
