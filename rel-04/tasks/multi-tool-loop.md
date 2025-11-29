# Task: Multi-Tool Conversation Loop

## Target
User story: 04-glob-then-read-file

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Single tool call works (Story 02: glob execution)
- File read tool works (Story 03: file_read execution)
- Task `conversation-tool-loop.md` completed
- Task `file-read-execute.md` completed

## Context
Read before starting:
- src/repl.c or src/repl_actions.c (conversation loop)
- src/openai/client.c (API request/response handling)
- rel-04/user-stories/04-glob-then-read-file.md (see full walkthrough)
- rel-04/tasks/conversation-tool-loop.md (previous implementation)

## Task
Extend the conversation loop to automatically continue when finish_reason is "tool_calls". The loop should:
1. Execute tool calls
2. Add assistant message (with tool_calls) to conversation
3. Add tool result message to conversation
4. Automatically send next API request
5. Repeat steps 1-4 until finish_reason is "stop"

Currently, the conversation loop might only handle one tool call before returning control. This task ensures it continues automatically for multi-tool scenarios (e.g., glob → file_read → final response).

## TDD Cycle

### Red
1. Add integration test to existing test suite:
   - Mock API sequence:
     - Response A: tool_calls for glob (finish_reason: "tool_calls")
     - Response B: tool_calls for file_read (finish_reason: "tool_calls")
     - Response C: text content (finish_reason: "stop")
   - Verify glob executes and Request B is sent
   - Verify file_read executes and Request C is sent
   - Verify final response is displayed
   - Verify conversation has 6 messages total (user, assistant, tool, assistant, tool, assistant)
2. Run `make check` - expect test failure if loop doesn't continue automatically

### Green
1. Locate conversation loop in repl.c or repl_actions.c
2. Identify where finish_reason is checked
3. Modify loop to continue while finish_reason == "tool_calls":
   - After processing tool results and adding messages
   - Automatically trigger next API request
   - Continue until finish_reason != "tool_calls"
4. May need to refactor to use a while/do-while loop instead of single execution
5. Run `make check` - expect pass

### Refactor
1. Ensure loop has a safety limit (prevent infinite loops)
2. Consider extracting loop logic to separate function if complex
3. Verify error handling for tool execution failures in loop
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Conversation loop automatically continues for multiple tool calls
- Loop terminates when finish_reason is "stop"
- Story 04 end-to-end scenario works (glob → file_read → response)
- 100% test coverage for new code
