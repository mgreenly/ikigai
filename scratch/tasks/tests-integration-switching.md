# Task: Create Provider Switching Integration Tests

**Model:** sonnet/extended
**Depends on:** repl-provider-routing.md, fork-model-override.md, tests-openai-basic.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

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

Create integration tests for provider switching and fork inheritance. Verifies that switching providers preserves message history, thinking levels translate correctly, and forked agents correctly inherit or override parent settings.

## Interface

**Test file to create:**

| File | Purpose |
|------|---------|
| `tests/integration/test_provider_switching.c` | Provider switching and fork inheritance |

**Mock utilities to use:**

| Utility | Purpose |
|---------|---------|
| `tests/helpers/mock_http.h` | Return canned responses for each provider |
| `tests/test_utils.h` | Test database setup |

## Test Scenarios

**Provider Switching (5 tests):**

1. **Start with Anthropic, switch to OpenAI**
   - Start session with Anthropic claude-sonnet-4-5
   - Send message, receive mocked response
   - Switch to OpenAI gpt-4o via /model command
   - Send message, receive mocked response
   - Verify both messages in history with correct formatting

2. **Start with OpenAI, switch to Google**
   - Similar flow with different providers
   - Verify message history preserved

3. **Multiple switches in one session**
   - Anthropic → OpenAI → Google → Anthropic
   - Verify all messages preserved

4. **Switch preserves system prompt**
   - Set system prompt with Anthropic
   - Switch to OpenAI
   - Verify system prompt still in context

5. **Switch updates agent database record**
   - Switch provider
   - Query database
   - Verify agent record updated

**Thinking Level Translation (4 tests):**

1. **Anthropic thinking to OpenAI reasoning**
   - Start with Anthropic, set thinking to "medium"
   - Verify request has thinkingBudget parameter
   - Switch to OpenAI o1 model
   - Verify request has reasoning_effort: "medium"

2. **OpenAI reasoning to Google thinking**
   - Start with OpenAI o1, set thinking to "high"
   - Switch to Google gemini-2.5
   - Verify request has thinkingBudget parameter

3. **Thinking level preserved across switch**
   - Set thinking level
   - Switch provider
   - Verify level preserved in new format

4. **Thinking level change after switch**
   - Switch provider
   - Change thinking level
   - Verify new level applied

**Fork Inheritance (5 tests):**

1. **Fork inherits parent provider**
   - Parent: claude-sonnet-4-5/medium
   - /fork without override
   - Verify child has claude-sonnet-4-5/medium

2. **Fork with model override**
   - Parent: claude-sonnet-4-5/medium
   - /fork --model gpt-4o
   - Verify child uses gpt-4o (implicit provider switch)

3. **Fork with thinking override**
   - Parent: gpt-4o/low
   - /fork --thinking high
   - Verify child uses gpt-4o/high

4. **Fork with full override**
   - Parent: claude-sonnet-4-5/medium
   - /fork --model gemini-2.5-flash/high
   - Verify child uses gemini-2.5-flash/high

5. **Database records fork hierarchy**
   - Create parent and forked child
   - Query database
   - Verify parent_uuid and fork_message_id correct

## Behaviors

**Mock Setup:**
- Configure mock HTTP to return appropriate responses for each provider
- Set environment variables for test API keys
- Use isolated test database

**Provider Detection:**
- Model names map to providers:
  - `claude-*` → anthropic
  - `gpt-*`, `o1-*` → openai
  - `gemini-*` → google

**Thinking Level Mapping:**
| Level | Anthropic | OpenAI | Google 2.5 | Google 3.0 |
|-------|-----------|--------|------------|------------|
| none | - | - | - | - |
| low | 1024 tokens | "low" | 1024 tokens | "low" |
| medium | 4096 tokens | "medium" | 4096 tokens | "medium" |
| high | 16384 tokens | "high" | 16384 tokens | "high" |

## Postconditions

- [ ] 1 test file with 14 tests
- [ ] Provider switching preserves message history
- [ ] Thinking levels translate correctly across providers
- [ ] Fork inheritance and override both tested
- [ ] Database records updated correctly
- [ ] All tests run in `make check`
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] No real API calls made
