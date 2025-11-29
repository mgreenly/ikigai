# Task: Integrate Tool Execution into Conversation Loop

## Target
User story: 02-single-glob-call

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Tool call parsing works (`ik_openai_parse_tool_calls`)
- Glob execution works (`ik_tool_exec_glob`)
- Tool result message works
- Assistant tool_calls message serialization works
- Task `assistant-tool-calls-msg.md` completed

## Context
Read before starting:
- src/repl.c or src/repl_actions.c (conversation handling)
- src/openai/client.c (API interaction)
- src/openai/sse_parser.c (response parsing)
- rel-04/user-stories/02-single-glob-call.md (full walkthrough)

## Task
Integrate tool execution into the conversation flow:
1. After receiving Response A, detect finish_reason: "tool_calls"
2. Parse tool calls from response
3. Execute glob tool
4. Add assistant message (with tool_calls) to conversation
5. Add tool result message to conversation
6. Send Request B automatically
7. Continue with Response B (normal text response)

## TDD Cycle

### Red
1. Create integration test or extend existing REPL tests:
   - Mock API to return Response A (tool_calls)
   - Verify tool execution is triggered
   - Verify Request B is sent with correct messages
   - Mock API to return Response B
   - Verify final response is displayed
2. Run `make check` - expect test failure (integration not wired)

### Green
1. Identify the response handling code path
2. Add check for finish_reason: "tool_calls"
3. When detected:
   - Parse tool calls
   - For each tool call, execute appropriate tool (glob for now)
   - Add assistant message with tool_calls to conversation
   - Add tool result message to conversation
   - Trigger follow-up API request
4. Run `make check` - expect pass

### Refactor
1. Consider extracting tool dispatch logic (for future tools)
2. Ensure error handling for tool execution failures
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Single glob tool call executes end-to-end
- Conversation continues after tool result
- 100% test coverage for new code
