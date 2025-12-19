# Task: Refactor Tool Execution to Use Agent

## Target

Background Agent Event Loop (gap.md) - Tool Execution Bug

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm
- database

### Source Files

- src/repl_tool.c (tool execution implementation)
- src/repl.h (function declarations, lines 107-108)
- src/repl_event_handlers.c (callers, lines 230, 307)

### Test Patterns

- tests/unit/repl/tool_test.c (if exists)
- tests/integration/tool_execution_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- Task `agent-repl-backpointer` is complete
- Task `agent-state-transitions` is complete

## Task

Refactor tool execution functions to take `ik_agent_ctx_t *agent` parameter and fix the thread worker to use the correct agent.

### What

The tool thread worker has the same bug as callbacks - it uses `args->repl->current`:

```c
// repl_tool.c:48-55
args->repl->current->tool_thread_result = result.ok;
pthread_mutex_lock_(&args->repl->current->tool_thread_mutex);
args->repl->current->tool_thread_complete = true;
```

If user switches agents while tool runs, results go to wrong agent.

### How

#### Change `tool_thread_args_t`

```c
// Old:
typedef struct {
    TALLOC_CTX *ctx;
    const char *tool_name;
    const char *arguments;
    ik_repl_ctx_t *repl;       // Problem: uses repl->current
} tool_thread_args_t;

// New:
typedef struct {
    TALLOC_CTX *ctx;
    const char *tool_name;
    const char *arguments;
    ik_agent_ctx_t *agent;     // Direct reference to target agent
} tool_thread_args_t;
```

#### Update `tool_thread_worker`

```c
// Old:
args->repl->current->tool_thread_result = result.ok;
pthread_mutex_lock_(&args->repl->current->tool_thread_mutex);

// New:
args->agent->tool_thread_result = result.ok;
pthread_mutex_lock_(&args->agent->tool_thread_mutex);
```

#### Refactor `ik_repl_start_tool_execution`

Rename to `ik_agent_start_tool_execution(ik_agent_ctx_t *agent)`:
- Change all `repl->current->` to `agent->`
- Set `args->agent = agent` instead of `args->repl = repl`
- Access shared via `agent->shared` if needed

#### Refactor `ik_repl_complete_tool_execution`

Rename to `ik_agent_complete_tool_execution(ik_agent_ctx_t *agent)`:
- Change all `repl->current->` to `agent->`
- Access shared via `agent->shared` for DB operations

#### Update Callers

In `src/repl_event_handlers.c`:
```c
// Old:
ik_repl_start_tool_execution(repl);
ik_repl_complete_tool_execution(repl);

// New:
ik_agent_start_tool_execution(repl->current);  // or specific agent
ik_agent_complete_tool_execution(agent);       // the agent being processed
```

### Why

When user switches agents while a tool is running, the thread writes results to `repl->current` which is now a different agent. The correct agent's tool never "completes" and the wrong agent gets corrupted state.

## TDD Cycle

### Red

Write tests that:
1. Create two agents (A and B)
2. Start tool execution on agent A
3. Switch repl->current to agent B (simulate user switch)
4. Complete tool execution
5. Verify agent A's tool_thread_result is set (not agent B's)
6. Verify agent A's conversation has the tool result

### Green

1. Update tool_thread_args_t to hold agent pointer
2. Update tool_thread_worker to use args->agent
3. Rename and refactor start_tool_execution
4. Rename and refactor complete_tool_execution
5. Update callers in repl_event_handlers.c
6. Update declarations in agent.h (or repl.h if keeping there)
7. Run `make check`

### Verify

- `make check` passes
- `make lint` passes

## Post-conditions

- `tool_thread_args_t` holds `ik_agent_ctx_t *agent`
- Thread worker writes to `args->agent->`, not `args->repl->current->`
- `ik_agent_start_tool_execution(agent)` exists and works
- `ik_agent_complete_tool_execution(agent)` exists and works
- All callers updated
- All tests pass
