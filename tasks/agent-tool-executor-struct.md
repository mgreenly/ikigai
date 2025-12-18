# Task: Create ik_agent_tool_executor_t Struct and Factory

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Tool Execution Sub-context

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/ddd.md
- .agents/skills/di.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/factory.md

## Pre-read Source (patterns)

- src/agent.h (lines 111-119 - tool execution fields in ik_agent_ctx_t)
- src/agent.c (lines 119-137 - tool execution state initialization, destructor)
- src/repl_tool.c (tool thread functions)
- src/tool.h (tool call interface)
- src/wrapper_pthread.h (pthread wrapper functions)

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (tool field initialization tests)
- tests/unit/repl/agent_tool_execution_test.c (tool execution tests)
- tests/unit/agent/agent_identity_test.c (pattern from previous task)
- tests/unit/agent/meson.build (test registration pattern)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Tasks `agent-identity-struct`, `agent-display-struct`, and `agent-llm-struct` are complete
- All three sub-context structs exist

## Task

Extract tool execution fields from `ik_agent_ctx_t` into a new `ik_agent_tool_executor_t` struct with its own factory function.

### What

Create a new struct `ik_agent_tool_executor_t` containing:

**Pending Tool Call (1 field):**
- `ik_tool_call_t *pending_tool_call` - Current tool being executed

**Thread State (4 fields):**
- `pthread_t tool_thread` - Tool execution thread handle
- `pthread_mutex_t tool_thread_mutex` - Thread safety mutex
- `bool tool_thread_running` - Whether thread is active
- `bool tool_thread_complete` - Whether thread finished

**Result State (2 fields):**
- `TALLOC_CTX *tool_thread_ctx` - Memory context for thread result
- `char *tool_thread_result` - Tool execution result JSON

**Iteration Tracking (1 field):**
- `int32_t tool_iteration_count` - Number of tool calls in current turn

**Total: 8 fields** (adjusted after pthread_t is a single field)

### How

1. In `src/agent.h`:
   - Add `ik_agent_tool_executor_t` struct definition AFTER `ik_agent_llm_t`, BEFORE `ik_agent_ctx_t`
   - Add factory declaration: `res_t ik_agent_tool_executor_create(TALLOC_CTX *ctx, ik_agent_tool_executor_t **out);`
   - Add destructor declaration for cleanup: `void ik_agent_tool_executor_destroy(ik_agent_tool_executor_t *executor);`

2. In `src/agent.c` (or new `src/agent_tool_executor.c`):
   - Implement `ik_agent_tool_executor_create()`:
     - Allocate `ik_agent_tool_executor_t` under ctx
     - Set pending_tool_call to NULL
     - Initialize mutex via `pthread_mutex_init_()`
     - Set tool_thread_running to false
     - Set tool_thread_complete to false
     - Set tool_thread_ctx to NULL
     - Set tool_thread_result to NULL
     - Set tool_iteration_count to 0
     - Set talloc destructor for mutex cleanup
     - Return ERR if mutex init fails

   - Implement destructor (talloc callback):
     - Lock/unlock mutex (helgrind requirement)
     - Destroy mutex

### Why

The tool execution fields (8 total) represent a distinct concern: running tools in background threads. They are:
- All related to thread management
- Require special cleanup (mutex destruction)
- Independent of identity, display, and LLM state
- Encapsulate thread safety concerns

Extracting them enables:
- Focused testing of thread state management
- Clear ownership of mutex lifecycle
- Isolation of pthread dependencies
- Easier future changes to tool execution model

## TDD Cycle

### Red

Create `tests/unit/agent/agent_tool_executor_test.c`:

1. Test `ik_agent_tool_executor_create()` succeeds
2. Test executor->pending_tool_call is NULL initially
3. Test executor->tool_thread_running is false initially
4. Test executor->tool_thread_complete is false initially
5. Test executor->tool_thread_ctx is NULL initially
6. Test executor->tool_thread_result is NULL initially
7. Test executor->tool_iteration_count is 0 initially
8. Test mutex can be locked and unlocked
9. Test executor is allocated under provided parent context
10. Test talloc_free properly cleans up mutex (no pthread errors)

Run `make check` - expect compilation failure (struct doesn't exist yet)

### Green

1. Add struct definition to `src/agent.h`
2. Add factory declaration to `src/agent.h`
3. Implement factory and destructor in `src/agent.c`
4. Add test file to `tests/unit/agent/meson.build`
5. Run `make check` - expect pass

### Refactor

1. Verify destructor matches existing `agent_destructor()` pattern
2. Verify error handling for mutex init failure
3. Run `make lint` - verify clean
4. Run with helgrind to verify no thread errors

## Post-conditions

- `make check` passes
- `make lint` passes
- `ik_agent_tool_executor_t` struct exists in `src/agent.h`
- `ik_agent_tool_executor_create()` implemented
- Destructor properly cleans up mutex
- Unit tests cover all tool executor fields
- Working tree is clean (all changes committed)

## Sub-agent Usage

- Use sub-agents to search for tool field usages: `grep -r "agent->pending_tool_call\|agent->tool_thread" src/`
- Identify callers that will need migration
- Check `src/repl_tool.c` for main usage patterns

## Notes

The mutex is the most critical part of this extraction. The destructor MUST:
1. Lock the mutex (helgrind requirement)
2. Unlock the mutex
3. Destroy the mutex

This matches the existing `agent_destructor()` in `src/agent.c` lines 21-30.

The `tool_thread` field (pthread_t) is uninitialized by default. It only gets a valid value when `pthread_create()` is called in `ik_agent_start_tool_execution()`. The `tool_thread_running` flag indicates whether the thread handle is valid.
