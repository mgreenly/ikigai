# Task: Parent/Child Navigation

## Target
User Stories: 13-navigate-parent.md, 14-navigate-child.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Navigation section)

## Pre-read Source (patterns)
- src/repl.h (READ - navigation function declarations)
- src/repl.c (READ - sibling navigation implementation patterns)

## Pre-read Tests (patterns)
- tests/unit/repl/*_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- agent-switch-state.md complete (ik_repl_switch_agent exists)
- nav-sibling-switch.md complete (sibling navigation patterns established)
- registry-create.md complete (created_at field exists on ik_agent_ctx_t)

## Task
Implement parent/child navigation: Ctrl+↑ for parent, Ctrl+↓ for child.

**Key Decisions (from questions.md):**
- Dead agents are removed from `agents[]` array when killed - not kept in memory
- Use `ik_repl_switch_agent()` for all switching (saves/restores state)
- Separator shows grayed-out indicator when nav is unavailable (prevention over error)
- If parent is dead (not in agents[]), nav_parent is no-op (separator already gray)

**API:**
```c
// Switch to parent agent
res_t ik_repl_nav_parent(ik_repl_ctx_t *repl);

// Switch to most recent running child
res_t ik_repl_nav_child(ik_repl_ctx_t *repl);
```

**Behavior:**
- Ctrl+↑: Go to parent (no action if at root or parent not in agents array)
- Ctrl+↓: Go to most recently created running child (no action if no children)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (Red, Green, Refactor)
- Keep sub-agent tasks focused and single-purpose

## TDD Cycle

### Red
1. Add declarations to `src/repl.h` (MODIFY)

2. Create test file: `tests/unit/repl/nav_parent_child_test.c` (CREATE)
   Tests should cover:
   - Test nav_parent switches to parent
   - Test nav_parent at root = no action
   - Test nav_child switches to child
   - Test nav_child selects most recent running child
   - Test nav_child with no children = no action
   - Test nav_child skips dead children

3. Run `make check` - expect test failures (use haiku sub-agent)

### Green
1. Implement in `src/repl.c` (MODIFY):
   ```c
   res_t ik_repl_nav_parent(ik_repl_ctx_t *repl)
   {
       if (repl->current->parent_uuid == NULL) {
           return OK(NULL);  // Already at root
       }

       // Dead agents are removed from agents[], so find returns NULL
       // if parent was killed. Separator shows grayed indicator.
       ik_agent_ctx_t *parent = ik_repl_find_agent(repl,
           repl->current->parent_uuid);
       if (parent) {
           CHECK(ik_repl_switch_agent(repl, parent));
       }
       return OK(NULL);
   }

   res_t ik_repl_nav_child(ik_repl_ctx_t *repl)
   {
       // Find most recent running child
       // Only agents in agents[] are running (dead ones removed)
       ik_agent_ctx_t *newest = NULL;
       int64_t newest_time = 0;

       for (size_t i = 0; i < repl->agent_count; i++) {
           ik_agent_ctx_t *a = repl->agents[i];
           if (a->parent_uuid &&
               strcmp(a->parent_uuid, repl->current->uuid) == 0 &&
               a->created_at > newest_time) {
               newest = a;
               newest_time = a->created_at;
           }
       }

       if (newest) {
           CHECK(ik_repl_switch_agent(repl, newest));
       }
       return OK(NULL);
   }
   ```

2. Wire up event handlers in `src/repl.c` (MODIFY - in keyboard handler)

3. Run `make check` - expect pass (use haiku sub-agent)

### Refactor
1. Verify created_at field exists on ik_agent_ctx_t (added in registry-create.md)
2. Verify using ik_repl_switch_agent for state preservation
3. Review code for edge cases and error handling
4. Run `make check` - verify still passing (use haiku sub-agent)
5. Run `make lint` - verify clean (use haiku sub-agent)

## Post-conditions
- `make check` passes
- `make lint` passes
- Working tree is clean (all changes committed with proper git commits after each TDD phase)
- Ctrl+↑ navigates to parent
- Ctrl+↓ navigates to most recent child
- No action at boundaries (root parent, no children)
- Edge cases handled correctly (dead parents, dead children)
