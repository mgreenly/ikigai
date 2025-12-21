# Task: Create Anthropic Provider Tests

**Layer:** 4 - Testing
**Depends on:** anthropic-provider.md, tests-provider-common.md

## Pre-Read

**Skills:**
- `/load tdd`

**Plan:**
- `scratch/plan/testing-strategy.md`

## Objective

Create comprehensive test suite for Anthropic provider.

## Deliverables

1. Create `tests/unit/providers/anthropic/`:
   - `test_anthropic_adapter.c` - Vtable implementation
   - `test_anthropic_client.c` - Request serialization
   - `test_anthropic_streaming.c` - SSE event handling
   - `test_anthropic_errors.c` - Error mapping

2. Create fixtures `tests/fixtures/anthropic/`:
   - `response_basic.json`
   - `response_thinking.json`
   - `response_tool_call.json`
   - `stream_basic.txt`
   - `stream_thinking.txt`
   - `error_auth.json`
   - `error_rate_limit.json`

3. Test coverage:
   - Request serialization (all content types)
   - Response parsing (all content types)
   - Thinking budget calculation
   - Error mapping for all HTTP statuses
   - Streaming event normalization

## Reference

- `scratch/plan/testing-strategy.md` - Test organization

## Postconditions

- [ ] 100% coverage on Anthropic adapter
- [ ] All fixtures validate against real API
