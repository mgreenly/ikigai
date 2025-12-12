# Task: Migrate completion state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 8 (completion field migration)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/backlog/shared-context-di.md (design document)

## Pre-read Source (patterns)
- src/agent.h (current agent context)
- src/agent.c (current agent create)
- src/repl.h (completion field)
- src/completion.h (ik_completion_t)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Display, input, conversation, LLM, tool, and spinner fields already migrated
- `repl->agent` exists with all previous state

## Task
Migrate tab completion state from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `completion` - tab completion context (NULL when inactive)

This is a small migration - just one field.

After this task:
- Agent owns its completion state
- Each agent has independent tab completion
- Access pattern becomes `repl->agent->completion`

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add forward declaration:
     ```c
     typedef struct ik_completion ik_completion_t;
     ```
   - Add field:
     ```c
     // Tab completion state (per-agent)
     ik_completion_t *completion;
     ```

2. Update `tests/unit/agent/agent_test.c`:
   - Test `agent->completion` is NULL initially

3. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Initialize completion:
     ```c
     agent->completion = NULL;  // Created on Tab press, destroyed on completion
     ```

2. Update `src/repl.h`:
   - Remove `ik_completion_t *completion;` field

3. Update `src/repl_init.c`:
   - Remove completion initialization if present (should already be NULL)

4. Update ALL files that access completion:
   - Change `repl->completion` to `repl->agent->completion`

5. Run `make check` - expect pass

### Refactor
1. Verify completion lifecycle works correctly (create on Tab, destroy on accept/cancel)
2. Verify completion layer interacts correctly with agent's completion
3. Verify no direct completion access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `completion` is in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- Tab completion works correctly with agent context
- All completion access uses `repl->agent->completion` pattern
- 100% test coverage maintained
- Working tree is clean (all changes committed)
