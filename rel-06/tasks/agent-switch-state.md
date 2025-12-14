# Task: Agent Switch State Save/Restore

## Target
User Stories: 12-navigate-sibling.md, 13-navigate-parent.md, 14-navigate-child.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Navigation section)
- project/memory.md (talloc ownership)

## Pre-read Source (patterns)
- READ: src/repl.h (repl context)
- READ: src/repl.c (event loop)
- READ: src/agent.h (ik_agent_ctx_t)
- READ: src/input_buffer/core.h (input buffer state)
- READ: src/scrollback.h (scroll position)

## Pre-read Tests (patterns)
- READ: tests/unit/repl/*_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- repl-agent-lookup.md complete

## Task
Create a centralized agent switch function that saves state from the outgoing agent and restores state to the incoming agent.

**Key Decisions (from questions.md):**
- Save ALL per-agent state: `input_buffer` contents, cursor position, `scroll_pos`, viewport offset
- Create shared `ik_repl_switch_agent(repl, new_agent)` function
- All nav commands use this single entry point
- Preserve unsent input - user can continue typing after switching back

**State to Save/Restore:**
- `input_buffer` contents (the text user was typing)
- Cursor byte position within input buffer
- Cursor grapheme position
- `scroll_pos` (viewport offset into scrollback)
- Any pending input state

**API:**
```c
// Switch from current agent to new_agent
// Saves state on outgoing, restores on incoming
// Returns ERR if new_agent is NULL or same as current
res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent);
```

**Note:** State is stored directly on `ik_agent_ctx_t`. The input buffer, cursor, and scroll position are already per-agent fields. This function ensures they're properly preserved across switches.

## TDD Cycle

### Red
1. MODIFY: Add declaration to `src/repl.h`:
   ```c
   res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent);
   ```

2. CREATE: `tests/unit/repl/switch_test.c`:
   - Test switch to different agent succeeds
   - Test switch to NULL returns error
   - Test switch to same agent is no-op
   - Test input buffer preserved on outgoing agent
   - Test input buffer restored on incoming agent
   - Test cursor position preserved and restored
   - Test scroll position preserved and restored
   - Test repl->current updated after switch
   - Test typing in A, switch to B, type, switch back to A - A's input intact

3. Run `make check` - expect test failures

### Green
1. MODIFY: Implement in `src/repl.c`:
   ```c
   res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent)
   {
       assert(repl != NULL);

       if (new_agent == NULL) {
           return ERR(repl, ERR_INVALID_ARG, "Cannot switch to NULL agent");
       }

       if (new_agent == repl->current) {
           return OK(NULL);  // No-op, already on this agent
       }

       // State is already stored on agent_ctx:
       // - repl->current->input_buffer (per-agent)
       // - repl->current->scroll_pos (per-agent)
       // These don't need explicit save/restore because
       // they're already per-agent fields.

       // Switch current pointer
       ik_agent_ctx_t *old = repl->current;
       repl->current = new_agent;

       // Mark display as needing full redraw
       // (scrollback content changed, input changed)
       repl->needs_redraw = true;

       return OK(NULL);
   }
   ```

2. Verify `ik_agent_ctx_t` has per-agent fields:
   - `ik_input_buffer_t *input_buffer`
   - `int64_t scroll_pos`
   - `ik_scrollback_t *scrollback`

3. Run `make check` - expect pass

### Refactor
1. Verify all navigation functions use `ik_repl_switch_agent()`
2. Update existing navigation stubs to call this function
3. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `make lint` passes
- `ik_repl_switch_agent()` function exists
- State preserved across agent switches
- Single entry point for all navigation
