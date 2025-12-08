# Task: Implement Agent Switching Mechanics

## Target
User Stories: 02-switch-next-agent.md, 03-switch-prev-agent.md, 04-switch-wrap-forward.md, 05-switch-wrap-backward.md, 11-preserved-input-on-switch.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/ddd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/facade.md
- .agents/skills/patterns/composite.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Switching Agents section, Lazy SIGWINCH)
- docs/rel-05/user-stories/02-switch-next-agent.md
- docs/rel-05/user-stories/03-switch-prev-agent.md
- docs/rel-05/user-stories/04-switch-wrap-forward.md
- docs/rel-05/user-stories/05-switch-wrap-backward.md
- docs/rel-05/user-stories/11-preserved-input-on-switch.md

## Pre-read Source (patterns)
- src/repl.h (ik_repl_ctx_t with agents array)
- src/repl.c (main loop, current agent access)
- src/repl_actions.c (action dispatch)
- src/agent.h (ik_agent_ctx_t)
- src/render.h (render context)
- src/render.c (terminal rendering)
- src/layer.h (layer interface)
- src/layer_wrappers.c (layer_cake handling)
- src/scrollback.h (scrollback reflow)
- src/input.h (IK_INPUT_AGENT_PREV/NEXT)

## Pre-read Tests (patterns)
- tests/unit/repl/repl_test.c (repl tests)
- tests/unit/scrollback/scrollback_test.c (reflow tests)

## Pre-conditions
- `make check` passes
- repl-agent-array.md complete (agents[] array exists)
- input-ctrl-arrow-actions.md complete (AGENT_PREV/NEXT actions exist)
- agent-event-loop.md complete (agents process independently)

## Task
Implement agent switching mechanics including circular navigation, I/O attachment, lazy SIGWINCH reflow, and full terminal redraw.

**Switching function:**
```c
// Switch to agent at given index
// Handles: detach current, attach target, lazy reflow, render
res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, size_t target_idx);

// Navigation helpers
size_t ik_repl_next_agent_idx(ik_repl_ctx_t *repl);  // Circular forward
size_t ik_repl_prev_agent_idx(ik_repl_ctx_t *repl);  // Circular backward
```

**Circular navigation logic:**
```c
// Next: (current + 1) % count
size_t ik_repl_next_agent_idx(ik_repl_ctx_t *repl)
{
    return (repl->current_agent_idx + 1) % repl->agent_count;
}

// Prev: current == 0 ? count - 1 : current - 1
size_t ik_repl_prev_agent_idx(ik_repl_ctx_t *repl)
{
    if (repl->current_agent_idx == 0) {
        return repl->agent_count - 1;
    }
    return repl->current_agent_idx - 1;
}
```

**Switch operation steps:**
1. Calculate target index (circular)
2. If target == current, no-op
3. Check if target agent needs reflow (lazy SIGWINCH)
4. If needs reflow, call `ik_scrollback_ensure_layout()`
5. Update `current_agent_idx`
6. Attach target agent's layer_cake to render context
7. Trigger full terminal redraw

**Lazy SIGWINCH handling:**
```c
// In ik_agent_ctx_t (from DI-1):
bool needs_reflow;  // Set true on SIGWINCH for non-current agents

// On SIGWINCH:
for (size_t i = 0; i < repl->agent_count; i++) {
    if (i == repl->current_agent_idx) {
        ik_scrollback_reflow(repl->agents[i]->scrollback, new_width);
    } else {
        repl->agents[i]->needs_reflow = true;  // Defer until switch
    }
}

// On switch, before render:
if (target_agent->needs_reflow) {
    ik_scrollback_reflow(target_agent->scrollback, term_width);
    target_agent->needs_reflow = false;
}
```

**I/O attachment:**
- Input: keystrokes go to current agent's input_buffer
- Output: current agent's layer_cake renders to terminal
- Switching changes which agent is "attached" to terminal I/O

## TDD Cycle

### Red
1. Create `src/agent_switch.h` and `src/agent_switch.c`:
   ```c
   res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, size_t target_idx);
   size_t ik_repl_next_agent_idx(ik_repl_ctx_t *repl);
   size_t ik_repl_prev_agent_idx(ik_repl_ctx_t *repl);
   ```

2. Create tests in `tests/unit/repl/agent_switch_test.c`:
   - Test next_agent_idx with 3 agents: 0→1, 1→2, 2→0 (wrap)
   - Test prev_agent_idx with 3 agents: 2→1, 1→0, 0→2 (wrap)
   - Test switch_agent updates current_agent_idx
   - Test switch_agent to same index is no-op
   - Test switch_agent triggers reflow if needs_reflow is true
   - Test switch_agent clears needs_reflow flag
   - Test switch does NOT affect other agents' state
   - Test input goes to current agent after switch

3. Run `make check` - expect test failures

### Green
1. Implement navigation helpers in `src/agent_switch.c`

2. Implement `ik_repl_switch_agent()`:
   ```c
   res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, size_t target_idx)
   {
       assert(repl != NULL);  // LCOV_EXCL_BR_LINE
       assert(target_idx < repl->agent_count);  // LCOV_EXCL_BR_LINE

       // No-op if already current
       if (target_idx == repl->current_agent_idx) {
           return OK(NULL);
       }

       ik_agent_ctx_t *target = repl->agents[target_idx];

       // Lazy reflow if needed
       if (target->needs_reflow) {
           ik_scrollback_reflow(target->scrollback, repl->shared->term->width);
           target->needs_reflow = false;
       }

       // Update current index
       repl->current_agent_idx = target_idx;

       // Trigger full redraw
       ik_repl_render(repl);

       return OK(NULL);
   }
   ```

3. Update action handlers in `src/repl_actions.c`:
   ```c
   case IK_INPUT_AGENT_NEXT:
       return ik_repl_switch_agent(repl, ik_repl_next_agent_idx(repl));

   case IK_INPUT_AGENT_PREV:
       return ik_repl_switch_agent(repl, ik_repl_prev_agent_idx(repl));
   ```

4. Update SIGWINCH handler to set needs_reflow for non-current agents

5. Run `make check` - expect pass

### Refactor
1. Verify switching preserves input buffer content (per-agent ownership)
2. Verify switching preserves scrollback content (per-agent ownership)
3. Verify render uses current agent's layer_cake
4. Extract magic numbers (if any) to constants
5. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- Ctrl+Right switches to next agent (circular)
- Ctrl+Left switches to previous agent (circular)
- Input buffer content preserved on switch
- Scrollback content preserved on switch
- Lazy reflow applied on switch if SIGWINCH occurred while away
- Full terminal redraw on switch
- Single agent: Ctrl+arrows are no-op (switch to self)
