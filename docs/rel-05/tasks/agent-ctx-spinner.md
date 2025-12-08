# Task: Migrate spinner state to Agent Context

## Target
Phase 1: Agent Context Extraction - Step 7 (spinner field migration)

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
- src/repl.h (spinner_state field)
- src/spinner/spinner.h (ik_spinner_state_t)
- src/layer_wrappers.c (spinner layer creation)

## Pre-read Tests (patterns)
- tests/unit/agent/agent_test.c

## Pre-conditions
- `make check` passes
- Display, input, conversation, LLM, and tool fields already migrated
- `repl->agent` exists with all previous state

## Task
Migrate spinner state from `ik_repl_ctx_t` to `ik_agent_ctx_t`:
- `spinner_state` - spinner animation state

This is a small migration - just one field.

After this task:
- Agent owns its spinner state
- Each agent has independent spinner animation
- Access pattern becomes `repl->agent->spinner_state`

## TDD Cycle

### Red
1. Update `src/agent.h`:
   - Add forward declaration or include:
     ```c
     typedef struct ik_spinner_state ik_spinner_state_t;
     ```
     Or if ik_spinner_state_t is a simple struct, include spinner.h
   - Add field:
     ```c
     // Spinner state (per-agent)
     ik_spinner_state_t spinner_state;
     ```

2. Update `tests/unit/agent/agent_test.c`:
   - Test spinner_state is properly initialized

3. Run `make check` - expect failures

### Green
1. Update `src/agent.c`:
   - Initialize spinner_state (check current repl_init.c for pattern):
     ```c
     ik_spinner_init(&agent->spinner_state);
     ```
     Or if it's zero-initialized by talloc_zero, just verify that's correct.

2. Update spinner layer creation in agent.c:
   - Spinner layer needs pointer to agent's spinner_state:
     ```c
     agent->spinner_layer = ik_spinner_layer_create(agent, &agent->spinner_state);
     ```

3. Update `src/repl.h`:
   - Remove `ik_spinner_state_t spinner_state;` field

4. Update `src/repl_init.c`:
   - Remove spinner_state initialization (now in agent_create)

5. Update ALL files that access spinner_state:
   - Change `repl->spinner_state` to `repl->agent->spinner_state`
   - Change `&repl->spinner_state` to `&repl->agent->spinner_state`

6. Run `make check` - expect pass

### Refactor
1. Verify spinner layer points to agent's spinner_state
2. Verify spinner animation works correctly
3. Verify no direct spinner_state access remains in repl_ctx
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `spinner_state` is in `ik_agent_ctx_t`, not `ik_repl_ctx_t`
- Spinner layer references agent's spinner_state
- All spinner access uses `repl->agent->spinner_state` pattern
- 100% test coverage maintained
