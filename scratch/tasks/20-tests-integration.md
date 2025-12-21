# Task: Create Integration Tests

**Phase:** Testing
**Depends on:** 09-repl-provider-abstraction, 17-tests-anthropic, 18-tests-openai, 19-tests-google

## Objective

Create end-to-end integration tests for multi-provider flows.

## Deliverables

1. Create `tests/integration/`:
   - `test_multi_provider_e2e.c` - Provider switching
   - `test_thinking_levels_e2e.c` - Thinking across providers
   - `test_tool_calls_e2e.c` - Tool calling across providers

2. Test scenarios:
   - Start with one provider, switch to another
   - Fork with different model
   - Tool call flows across providers
   - Error handling and recovery
   - Session restoration with provider info

3. Optional live validation:
   - Opt-in via `ENABLE_LIVE_API_TESTS=1`
   - Verify mocks against real APIs
   - Update fixtures capability

## Reference

- `scratch/plan/testing-strategy.md` - Integration Tests section

## Verification

- All providers work through REPL
- Provider switching works correctly
- Fork inherits/overrides correctly
