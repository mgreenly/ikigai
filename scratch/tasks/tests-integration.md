# Task: Create Integration Tests

**Layer:** 6
**Model:** opus/extended
**Depends on:** repl-provider-abstraction.md, fork-model-override.md, tests-anthropic.md, tests-openai.md, tests-google.md

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load database` - Database schema and query patterns

**Source:**
- `tests/integration/` - Existing integration test patterns
- `src/commands_basic.c` - Model switching command
- `src/commands_fork.c` - Fork command with model override

**Plan:**
- `scratch/plan/testing-strategy.md` - Integration Tests section

## Objective

Create end-to-end integration tests for multi-provider flows including provider switching, thinking level changes, tool calling across different providers, and session restoration. All tests are mocked (no real API calls) and run as part of `make check` to verify cross-cutting system behavior.

## Interface

Test files to create:

| File | Purpose |
|------|---------|
| `tests/integration/test_multi_provider_e2e.c` | Tests for provider switching and session continuity |
| `tests/integration/test_thinking_levels_e2e.c` | Tests for thinking level changes across providers |
| `tests/integration/test_tool_calls_e2e.c` | Tests for tool calling with different providers |

Mock utilities to create:

| Utility | Purpose |
|---------|---------|
| Mock HTTP client | Return canned responses for each provider |
| Mock credentials | Provide test API keys |
| Test database | Isolated database for integration tests |

## Behaviors

**Multi-Provider E2E Tests:**
- Start session with one provider (e.g., Anthropic)
- Send message and receive mocked response
- Switch to different provider (e.g., OpenAI)
- Send message with new provider
- Verify message history preserved across providers
- Verify provider-specific formatting maintained

**Thinking Levels E2E Tests:**
- Set thinking level for current provider
- Verify request includes correct thinking parameter
- Switch to different provider with same thinking level
- Verify thinking level translated to new provider's format:
  - Anthropic: thinking budget in tokens
  - OpenAI: reasoning effort (low/medium/high)
  - Google: thinkingBudget or thinkingLevel based on version
- Change thinking level and verify update

**Tool Calls E2E Tests:**
- Define tools for agent
- Send message triggering tool call
- Verify tool call formatted correctly for each provider:
  - Anthropic: tool_use content block with id
  - OpenAI: tool_calls array with JSON string arguments
  - Google: functionCall parts with generated UUID
- Return tool result
- Verify assistant processes result
- Test across all three providers

**Fork with Model Override Tests:**
- Create parent agent with provider A and model M1
- Fork child without override - verify inheritance
- Fork child with model override - verify child uses new model
- Fork child with provider change - verify provider switch
- Verify database stores correct values for each agent

**Error Handling and Recovery Tests:**
- Trigger rate limit error from provider
- Verify error category identified correctly
- Verify retryable flag set appropriately
- Trigger non-retryable error
- Verify error handling differs

**Session Restoration Tests:**
- Create session with provider info
- Save to database
- Restore from database
- Verify provider, model, and thinking level restored
- Send message with restored session

## Test Scenarios

**Provider Switching:**
- Start with Anthropic claude-sonnet-4-5
- Send message, receive response
- Switch to OpenAI gpt-4o via /model command
- Send message, receive response
- Verify both messages in history with correct formatting

**Thinking Level Translation:**
- Start with Anthropic, set thinking to "medium"
- Verify request has thinkingBudget parameter
- Switch to OpenAI o1 model
- Verify request has reasoning_effort: "medium"
- Switch to Google gemini-2.5
- Verify request has thinkingBudget parameter

**Tool Calling:**
- Define weather tool
- Ask "What's the weather in Tokyo?"
- Anthropic: verify tool_use block with id
- OpenAI: verify tool_calls array with JSON args
- Google: verify functionCall with generated UUID
- Return weather data
- Verify response incorporates tool result

**Fork Inheritance:**
- Parent: claude-sonnet-4-5/medium
- /fork - child inherits claude-sonnet-4-5/medium
- /fork --model gpt-4o/low - child uses gpt-4o/low
- Verify database has correct provider/model/thinking for each

**Error Handling:**
- Mock 429 rate limit from Anthropic
- Verify ERR_RATE_LIMIT category
- Verify retryable flag true
- Mock 401 auth error from OpenAI
- Verify ERR_AUTH category
- Verify retryable flag false

**Session Restoration:**
- Create session with openai/gpt-4o/high
- Save to database
- Load from database in new process
- Verify provider is OpenAI
- Verify model is gpt-4o
- Verify thinking level is high
- Send message successfully

## Postconditions

- [ ] All providers work through REPL with mocked responses
- [ ] Provider switching preserves message history
- [ ] Thinking levels translate correctly across providers
- [ ] Tool calling works with all providers
- [ ] Fork inheritance and override both tested
- [ ] Error handling verified for all error categories
- [ ] Session restoration works correctly
- [ ] All tests run in `make check`
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] No real API calls made during tests
