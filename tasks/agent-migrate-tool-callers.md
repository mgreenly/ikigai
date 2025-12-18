# Task: Migrate Tool Executor Field Callers

## Target

Refactoring #1: Decompose `ik_agent_ctx_t` God Object - Tool Field Migration

## Pre-read Skills

- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Source (patterns)

- src/agent.h (ik_agent_ctx_t with embedded ik_agent_tool_executor_t)
- src/agent.c (tool executor initialization and state transitions)
- src/repl_tool.c (tool execution thread functions)
- src/repl_event_handlers.c (tool completion polling)
- src/repl.h
- src/repl_viewport.c

## Pre-read Tests (patterns)

- tests/unit/agent/agent_test.c (updated accessor patterns)
- tests/unit/agent/agent_tool_executor_test.c
- tests/unit/repl/agent_tool_execution_test.c

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Task `agent-migrate-llm-callers` is complete
- All LLM field migrations done

## Task

Migrate all production code (src/*.c) to use the new tool executor field accessor pattern.

### What

Update all source files that access tool executor fields to use the new path:

| Old Pattern | New Pattern |
|-------------|-------------|
| `agent->pending_tool_call` | `agent->tool.pending_tool_call` |
| `agent->tool_thread` | `agent->tool.tool_thread` |
| `agent->tool_thread_mutex` | `agent->tool.tool_thread_mutex` |
| `agent->tool_thread_running` | `agent->tool.tool_thread_running` |
| `agent->tool_thread_complete` | `agent->tool.tool_thread_complete` |
| `agent->tool_thread_ctx` | `agent->tool.tool_thread_ctx` |
| `agent->tool_thread_result` | `agent->tool.tool_thread_result` |
| `agent->tool_iteration_count` | `agent->tool.tool_iteration_count` |

### How

1. **Discovery Phase** (use sub-agents):

   Search BOTH access patterns - direct agent and via repl->current:
   ```bash
   # Direct agent access
   grep -rn "agent->pending_tool_call" src/
   grep -rn "agent->tool_thread\b" src/
   grep -rn "agent->tool_thread_mutex" src/
   grep -rn "agent->tool_thread_running\|agent->tool_thread_complete" src/
   grep -rn "agent->tool_thread_ctx\|agent->tool_thread_result" src/
   grep -rn "agent->tool_iteration_count" src/

   # Indirect via repl->current (accounts for 80%+ of callsites)
   grep -rn "repl->current->pending_tool_call" src/
   grep -rn "repl->current->tool_thread\b" src/
   grep -rn "repl->current->tool_thread_mutex" src/
   grep -rn "repl->current->tool_thread_running\|repl->current->tool_thread_complete" src/
   grep -rn "repl->current->tool_thread_ctx\|repl->current->tool_thread_result" src/
   grep -rn "repl->current->tool_iteration_count" src/
   ```

2. **Update each file**:
   - For each file with matches, update the accessor pattern
   - Pay special attention to mutex operations (threading safety)
   - Run `make check` after each file

3. **Expected files with changes**:
   - `src/agent.c` - State transitions use mutex
   - `src/repl_tool.c` - Main tool execution code
   - `src/repl_event_handlers.c` - Tool completion polling
   - `src/repl_callbacks.c` - Tool state checks

### Why

Tool executor fields are thread-sensitive. This migration:
- Groups all thread-related state together
- Makes mutex ownership clear
- Enables future tool execution model changes

## TDD Cycle

### Red

Code should fail to compile if old field paths are removed.

### Green

1. Run grep to find all callers (expect 30-40 locations)
2. Start with `src/repl_tool.c` (highest density)
3. Update `src/agent.c` state transitions (uses mutex)
4. Update remaining files
5. Run `make check` after each file

### Refactor

1. Verify no old patterns remain
2. Run `make lint` - verify clean
3. Run helgrind to verify thread safety

## Post-conditions

- `make check` passes
- `make lint` passes
- No source files use `agent->pending_tool_call` (use `agent->tool.pending_tool_call`)
- No source files use `agent->tool_thread` (use `agent->tool.tool_thread`)
- No source files use `agent->tool_thread_mutex` (use `agent->tool.tool_thread_mutex`)
- No source files use `agent->tool_thread_*` fields directly
- No source files use `agent->tool_iteration_count` (use `agent->tool.tool_iteration_count`)
- Working tree is clean (all changes committed)
- Helgrind shows no new thread errors

## Sub-agent Usage

**RECOMMENDED: Use sub-agents for this task**

Tool fields have concentrated usage (mainly repl_tool.c). Sub-agents should:
1. Handle `src/repl_tool.c` first (most changes)
2. Verify mutex operations are correct after migration
3. Run helgrind verification

Pattern for sub-agent:
```
For file "repl_tool.c":
  - Read file
  - Replace agent->pending_tool_call with agent->tool.pending_tool_call
  - Replace agent->tool_thread with agent->tool.tool_thread
  - Replace agent->tool_thread_mutex with agent->tool.tool_thread_mutex
  - etc.
  - Write file
  - Run make check
```

## Sub-Agent Execution Strategy

This task has 82 callsites across multiple files. Use sub-agents to parallelize:

1. Spawn one sub-agent per file group (e.g., tool execution files, state transition files)
2. Each sub-agent:
   - Grep for all patterns in assigned files
   - Update field access to new pattern (e.g., `agent->tool_thread_mutex` → `agent->tool.tool_thread_mutex`)
   - Verify with `make check`
   - Report completion

### Suggested Batches:
- **Batch 1:** src/repl_tool.c (highest density - main tool execution code)
- **Batch 2:** src/agent.c (state transitions using mutex)
- **Batch 3:** src/repl_event_handlers.c, src/repl_callbacks.c (tool completion polling)
- **Batch 4:** Remaining files

## Notes

**Critical: Mutex Operations**

The mutex `agent->tool_thread_mutex` protects state transitions and tool thread completion. After migration, mutex operations look like:
```c
pthread_mutex_lock_(&agent->tool.tool_thread_mutex);
agent->llm.state = IK_AGENT_STATE_EXECUTING_TOOL;  // Note: llm.state!
pthread_mutex_unlock_(&agent->tool.tool_thread_mutex);
```

The mutex is in `tool` sub-context but protects the `llm.state` field. This cross-reference is intentional.

**Thread Function Context**

The tool thread function receives agent context and accesses:
- `agent->tool.pending_tool_call` - What to execute
- `agent->tool.tool_thread_result` - Where to store result
- `agent->tool.tool_thread_ctx` - Memory context for result

These must all be migrated consistently.

**ik_agent_has_running_tools()**

This function checks `agent->tool_thread_running`. After migration:
```c
bool ik_agent_has_running_tools(const ik_agent_ctx_t *agent)
{
    return agent->tool.tool_thread_running;
}
```

**Address-of Operations**

Mutex operations use address-of:
```c
pthread_mutex_lock_(&agent->tool.tool_thread_mutex);
```

The `&agent->tool.tool_thread_mutex` form is correct for embedded struct.

This is the final migration task. After this, all 35+ fields have been migrated to their respective sub-contexts.
