# Task: Fork Sync Barrier

## Target
User Story: 06-fork-sync-barrier.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Fork Semantics section)

## Pre-read Source (patterns)
- src/commands.c (READ - cmd_fork pattern, MODIFY)
- src/tool.h (READ - tool execution state)
- src/agent.h (READ - agent state, MODIFY - add has_running_tools declaration)

## Pre-read Tests (patterns)
- tests/unit/commands/cmd_fork_test.c (CREATE/MODIFY - fork barrier tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- fork-history-copy.md complete

## Task
Implement sync barrier: fork waits for all running tools to complete before executing. This ensures conversation state is consistent at fork point.

**Check:**
```c
bool ik_agent_has_running_tools(const ik_agent_ctx_t *agent);
```

**Behavior:**
1. On `/fork`, check if any tools are running
2. If yes, display "Waiting for tools to complete..."
3. Block until all tools finish
4. Then proceed with fork

## TDD Cycle

### Red
1. Add declaration to `src/agent.h`:
   ```c
   bool ik_agent_has_running_tools(const ik_agent_ctx_t *agent);
   ```

2. Add tests:
   - Test fork with no running tools proceeds immediately
   - Test fork with running tool waits
   - Test message displayed while waiting
   - Test fork completes after tool finishes
   - Test tool results in history before fork

3. Run `make check` - expect test failures

### Green
1. Implement `ik_agent_has_running_tools()`
2. Update `cmd_fork()` with barrier logic:
   ```c
   res_t cmd_fork(ik_repl_ctx_t *repl, const char *args)
   {
       // Sync barrier
       if (ik_agent_has_running_tools(repl->current)) {
           ik_scrollback_append_system(repl->current->scrollback,
               "Waiting for tools to complete...");

           // Wait for tools (event loop handles this)
           while (ik_agent_has_running_tools(repl->current)) {
               // Process events...
           }
       }

       // Proceed with fork...
   }
   ```

3. Run `make check` - expect pass

### Refactor
1. Consider async/callback pattern instead of blocking
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed)
- Fork waits for running tools
- User sees waiting message
- Fork completes with consistent state
