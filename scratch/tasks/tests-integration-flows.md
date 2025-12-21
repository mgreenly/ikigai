# Task: Create Integration Flow Tests

**Layer:** 7
**Model:** sonnet/extended
**Depends on:** tests-integration-switching.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load tdd` - Test-driven development patterns
- `/load database` - Database schema and query patterns
- `/load errors` - Error handling patterns

**Source:**
- `tests/integration/` - Existing integration test patterns
- `src/tool_dispatcher.c` - Tool execution
- `src/repl_init.c` - Session restoration

**Plan:**
- `scratch/plan/testing-strategy.md` - Integration Tests section

## Objective

Create integration tests for cross-cutting flows: tool calling across providers, error handling and recovery, and session restoration. Verifies end-to-end behavior with mocked providers.

## Interface

**Test files to create:**

| File | Purpose |
|------|---------|
| `tests/integration/test_tool_calls_e2e.c` | Tool calling with different providers |
| `tests/integration/test_error_handling_e2e.c` | Error handling and recovery |
| `tests/integration/test_session_restore_e2e.c` | Session restoration |

## Test Scenarios

### Tool Calls E2E (6 tests)

1. **Anthropic tool call format**
   - Define weather tool
   - Send "What's the weather in Tokyo?"
   - Mock returns tool_use content block with id
   - Verify tool call parsed correctly
   - Return tool result
   - Verify assistant incorporates result

2. **OpenAI tool call format**
   - Same flow
   - Mock returns tool_calls array with JSON string arguments
   - Verify JSON arguments parsed correctly

3. **Google tool call format**
   - Same flow
   - Mock returns functionCall parts
   - Verify UUID generated for tool call

4. **Tool result format per provider**
   - Return tool result to each provider
   - Verify result formatted correctly for each

5. **Multiple tool calls in one response**
   - Mock returns multiple tool calls
   - Verify all executed and results returned

6. **Tool error handling**
   - Tool returns error
   - Verify error propagated to provider correctly

### Error Handling E2E (6 tests)

1. **Rate limit from Anthropic**
   - Mock 429 rate limit error
   - Verify ERR_RATE_LIMIT category
   - Verify retryable flag true
   - Verify user-friendly message displayed

2. **Rate limit from OpenAI**
   - Same flow, OpenAI format
   - Verify same error category

3. **Auth error from OpenAI**
   - Mock 401 auth error
   - Verify ERR_AUTH category
   - Verify retryable flag false

4. **Overloaded error from Anthropic**
   - Mock 529 overloaded error
   - Verify ERR_SERVICE category
   - Verify retryable flag true

5. **Context length error**
   - Mock 400 with context_length_exceeded
   - Verify ERR_INVALID_REQUEST category

6. **Network error**
   - Mock connection failure
   - Verify ERR_NETWORK category
   - Verify retryable flag true

### Session Restoration E2E (5 tests)

1. **Restore provider setting**
   - Create session with openai/gpt-4o
   - Save to database
   - Simulate restart (new REPL context)
   - Load from database
   - Verify provider is OpenAI

2. **Restore model setting**
   - Create session with specific model
   - Restore
   - Verify model preserved

3. **Restore thinking level**
   - Create session with thinking level high
   - Restore
   - Verify thinking level preserved

4. **Restore conversation history**
   - Create session with messages
   - Restore
   - Verify messages loaded in order
   - Send new message
   - Verify context includes history

5. **Restore forked agent**
   - Create parent and forked child
   - Kill session
   - Restore
   - Verify parent-child relationship preserved
   - Verify child has correct provider/model

## Behaviors

**Tool Call Flow:**
```
1. User sends message
2. Provider returns tool_call in response
3. REPL parses tool call (provider-specific format)
4. REPL executes tool
5. REPL sends tool_result back to provider
6. Provider returns final response
```

**Error Category Mapping:**

| HTTP Status | Category | Retryable |
|-------------|----------|-----------|
| 401, 403 | ERR_AUTH | false |
| 429 | ERR_RATE_LIMIT | true |
| 400 | ERR_INVALID_REQUEST | false |
| 500, 502, 503 | ERR_SERVICE | true |
| 529 (Anthropic) | ERR_SERVICE | true |
| Connection fail | ERR_NETWORK | true |

**Session Restoration Flow:**
```
1. REPL starts
2. Query database for active session
3. Load agents from agents table
4. For each agent, replay messages using ik_agent_replay_history()
5. Reconstruct mark stack
6. Resume with provider/model/thinking from agent record
```

## Postconditions

- [ ] 3 test files with 17 tests total
- [ ] Tool calling works with all 3 providers
- [ ] Tool result formatting verified
- [ ] Error categories mapped correctly
- [ ] Retryable flag verified for each error type
- [ ] Session restoration preserves all settings
- [ ] All tests run in `make check`
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] No real API calls made
