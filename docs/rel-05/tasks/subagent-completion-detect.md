# Task: Implement Sub-Agent Completion Detection

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/patterns/state-machine.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Completion semantics

### Source patterns
- `src/repl/repl.c` - REPL state machine
- `src/repl/agent.c` - Agent context

## Pre-conditions

- Sub-agent can be spawned via tool
- Agent state machine exists (IDLE, STREAMING, EXECUTING_TOOL)

## Task

Implement completion detection for sub-agents. A sub-agent "completes" when it would wait for human input:

1. LLM response received (not a tool call)
2. No pending tool execution
3. State transitions to IDLE

When detected:
- Set `agent->completed = true`
- Capture final assistant response in `agent->final_response`

Only applies to agents where `is_sub_agent = true`.

## TDD Cycle

### Red
Write tests:
- Sub-agent completes after LLM response (no tool call)
- Sub-agent does NOT complete if tool call pending
- Completion captures final response
- Regular agents (is_sub_agent=false) don't trigger completion

### Green
Add completion check in state transition to IDLE for sub-agents.

### Verify
`make check` passes.

## Post-conditions

- Sub-agents correctly detect completion
- Final response captured
- All tests pass
