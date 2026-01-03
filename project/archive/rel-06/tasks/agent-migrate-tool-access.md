# Task: Migrate Tool Executor Field Access

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 3 (Migration)

## Model

sonnet/extended (8 fields, thread-sensitive - use sub-agents)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm

### Docs

- project/memory.md (talloc ownership)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t)

### Source Files

- src/agent_tool_executor.h (tool executor struct)
- src/agent.h (current ik_agent_ctx_t with both legacy and tool_executor fields)
- src/tool.h (tool call type)

### Test Patterns

- tests/unit/agent/agent_test.c (agent tests)
- tests/unit/tool/*.c (tool tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make helgrind` passes
- `ik_agent_tool_executor_t *tool_executor` field exists in ik_agent_ctx_t
- Both legacy and tool_executor fields populated during creation
- Identity field migration complete (from agent-migrate-identity-access.md)
- Display field migration complete (from agent-migrate-display-access.md)
- LLM field migration complete (from agent-migrate-llm-access.md)

## Task

Migrate all access to tool execution fields from legacy to sub-context. This is the highest-risk migration due to thread safety concerns.

### What

Replace all occurrences:

**Tool Call:**
- `agent->pending_tool_call` -> `agent->tool_executor->pending`

**Thread State:**
- `agent->tool_thread` -> `agent->tool_executor->thread`
- `agent->tool_thread_mutex` -> `agent->tool_executor->mutex`
- `agent->tool_thread_running` -> `agent->tool_executor->running`
- `agent->tool_thread_complete` -> `agent->tool_executor->complete`

**Thread Context:**
- `agent->tool_thread_ctx` -> `agent->tool_executor->ctx`
- `agent->tool_thread_result` -> `agent->tool_executor->result`

**Iteration Count:**
- `agent->tool_iteration_count` -> `agent->tool_executor->iteration_count`

### How

**Step 1: Identify all files with tool execution field access**

```bash
# Find all files accessing tool execution fields
grep -rn "->pending_tool_call\|->tool_thread\|->tool_thread_mutex\|->tool_thread_running\|->tool_thread_complete\|->tool_thread_ctx\|->tool_thread_result\|->tool_iteration_count" src/ tests/ \
    | grep -v "tool_executor->" \
    > /tmp/tool_access.txt
```

**Step 2: Handle mutex migration (CRITICAL)**

The mutex migration is the most sensitive part. During Phase 2, we had two mutexes. Now we consolidate to one:

```c
// Before (transition period - two mutexes)
pthread_mutex_lock_(&agent->tool_thread_mutex);
pthread_mutex_lock_(&agent->tool_executor->mutex);
// ... critical section ...
pthread_mutex_unlock_(&agent->tool_executor->mutex);
pthread_mutex_unlock_(&agent->tool_thread_mutex);

// After (migration complete - one mutex)
pthread_mutex_lock_(&agent->tool_executor->mutex);
// ... critical section ...
pthread_mutex_unlock_(&agent->tool_executor->mutex);
```

**Step 3: Remove legacy mutex from agent creation**

In `ik_agent_create()`, remove legacy mutex initialization:

```c
// Before
int rc = pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
if (rc != 0) PANIC("Failed to initialize mutex");

// After
// Removed - tool_executor->mutex is the only mutex
```

**Step 4: Update thread function**

The tool execution thread uses the mutex and flags:

```c
// Before
static void *tool_thread_func(void *arg)
{
    ik_agent_ctx_t *agent = arg;
    // ... execute tool ...

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_result = result;
    agent->tool_thread_running = false;
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    return NULL;
}

// After
static void *tool_thread_func(void *arg)
{
    ik_agent_ctx_t *agent = arg;
    // ... execute tool ...

    pthread_mutex_lock_(&agent->tool_executor->mutex);
    agent->tool_executor->result = result;
    agent->tool_executor->running = false;
    agent->tool_executor->complete = true;
    pthread_mutex_unlock_(&agent->tool_executor->mutex);
    return NULL;
}
```

**Step 5: Use thread-safe accessors where appropriate**

```c
// Before
pthread_mutex_lock_(&agent->tool_thread_mutex);
bool running = agent->tool_thread_running;
pthread_mutex_unlock_(&agent->tool_thread_mutex);

// After (using accessor)
bool running = ik_agent_tool_executor_is_running(agent->tool_executor);
```

### Why

The tool executor migration is highest risk because:

1. **Thread safety:** Incorrect mutex usage causes race conditions or deadlocks
2. **Dual mutex removal:** Must carefully remove legacy mutex after migration
3. **Thread function update:** Background thread code must use new struct
4. **Testing with helgrind:** Must verify no threading errors

### Files Likely Affected

Based on codebase analysis:
- `src/agent.c` - tool execution start/complete, creation
- `src/repl_event_handlers.c` - tool completion polling
- `src/repl_tool.c` - tool dispatch
- `tests/unit/agent/*.c` - agent tests
- `tests/unit/tool/*.c` - tool tests
- `tests/integration/*.c` - integration tests

## TDD Cycle

### Red

No new tests needed - existing tests and helgrind verify behavior. Run `make check` AND `make helgrind` before migration.

### Green

**CRITICAL: Use sub-agents for parallel migration, but coordinate carefully**

Due to thread safety concerns, coordinate more carefully than other migrations:

1. **Sub-agent 1:** Migrate `src/agent.c` (FIRST - defines patterns)
2. **Sub-agent 2:** Migrate `src/repl_event_handlers.c` (after Agent 1)
3. **Sub-agent 3:** Migrate `src/repl_tool.c` (after Agent 1)
4. **Sub-agent 4:** Migrate test files (after source files)

Each sub-agent:
1. Opens assigned file(s)
2. Replaces mutex access (`tool_thread_mutex` -> `tool_executor->mutex`)
3. Replaces state flag access (running, complete)
4. Replaces result/ctx access
5. Replaces pending_tool_call access
6. Runs `make check` AND `make helgrind` to verify

**Migration Order Within Agent:**
1. First: mutex replacement (highest risk)
2. Second: state flags (running, complete)
3. Third: other fields (pending, result, ctx, iteration_count)

### Verify

After all sub-agents complete:
- `make check` passes
- `make lint` passes
- `make helgrind` passes (CRITICAL - thread safety)
- Tool execution works correctly (manual verification)

```bash
# Verify no remaining legacy access
grep -rn "->tool_thread_mutex" src/ tests/
grep -rn "->tool_thread_running" src/ tests/
grep -rn "->pending_tool_call" src/ tests/
```

## Post-conditions

- All tool execution field access uses `agent->tool_executor->*`
- Only one mutex (tool_executor->mutex)
- Legacy mutex initialization removed from ik_agent_create
- Thread function uses new struct
- All existing tests pass
- Tool execution works correctly
- `make check` passes
- `make lint` passes
- `make helgrind` passes (no thread errors)
- Working tree is clean (all changes committed)

## Rollback

If migration fails partway:
1. `git checkout -- .` to revert all changes
2. Debug with helgrind
3. Try migration again with fix

**IMPORTANT:** If helgrind reports errors, DO NOT proceed - fix thread safety issues first.

**Alternative:** Migrate one mutex consumer at a time:
1. First commit: agent.c tool execution functions
2. Second commit: repl_event_handlers.c
3. Third commit: repl_tool.c
4. Fourth commit: test files
5. Fifth commit: remove legacy mutex

This enables granular rollback and incremental helgrind verification.

## Sub-agent Usage

**MANDATORY:** Use sub-agents, but with strict ordering due to thread safety.

```
Sub-agent assignments (SEQUENTIAL for source, parallel for tests):
- Agent A: src/agent.c (MUST COMPLETE FIRST)
  - Run `make check && make helgrind` after completion

- Agent B: src/repl_event_handlers.c (AFTER Agent A)
  - Run `make check && make helgrind` after completion

- Agent C: src/repl_tool.c (AFTER Agent A, parallel with B)
  - Run `make check && make helgrind` after completion

- Agent D: tests/ (AFTER source agents complete)
  - Run `make check && make helgrind` after completion

Final verification by main agent:
1. `make check` passes
2. `make helgrind` passes
3. Remove legacy mutex from agent.h (if not already done)
4. Final `make check && make helgrind`
```

**WARNING:** This is the highest-risk migration. Take extra care with:
- Mutex lock ordering
- Double-locking prevention
- Thread function updates
- Helgrind verification after each step
