# Task: Create Integration Tests

**Layer:** 6 - Testing
**Model:** opus/extended (cross-cutting system understanding)
**Depends on:** repl-provider-abstraction.md, fork-model-override.md, tests-anthropic.md, tests-openai.md, tests-google.md

## Pre-Read

**Skills:**
- `/load tdd`
- `/load ddd`

**Plan:**
- `scratch/plan/testing-strategy.md` (Integration Tests section)

## Objective

Create end-to-end integration tests for multi-provider flows. These are **mocked** tests that run as part of `make check`.

## Deliverables

1. Create `tests/integration/`:
   - `test_multi_provider_e2e.c` - Provider switching
   - `test_thinking_levels_e2e.c` - Thinking across providers
   - `test_tool_calls_e2e.c` - Tool calling across providers

2. Test scenarios (all mocked, no real API calls):
   - Start with one provider, switch to another
   - Fork with different model
   - Tool call flows across providers
   - Error handling and recovery
   - Session restoration with provider info

## Reference

- `scratch/plan/testing-strategy.md` - Integration Tests section

## Postconditions

- [ ] All providers work through REPL (mocked)
- [ ] Provider switching works correctly
- [ ] Fork inherits/overrides correctly
- [ ] Tests run in `make check`

## Note

Live API validation is handled separately by `contract-validations.md` task.
