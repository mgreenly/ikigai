# Task: Sibling Navigation

## Target
User Story: 12-navigate-siblings.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Navigation section)

## Pre-read Source (READ - patterns)
- src/repl.h (ik_repl_ctx_t)
- src/agent.h (ik_agent_ctx_t)
- src/input.c (event handling)

## Pre-read Tests (READ - patterns)
- tests/unit/repl/*_test.c

## Source Files (CREATE/MODIFY)
- src/repl.h (add function declarations)
- src/repl.c (implement ik_repl_nav_prev_sibling, ik_repl_nav_next_sibling)
- src/input.c (wire up Ctrl+← and Ctrl+→ event handlers)
- tests/unit/repl/nav_sibling_test.c (CREATE - new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- agent-switch-state.md complete (ik_repl_switch_agent exists)
- nav-ctrl-arrows.md complete

## Task
Implement sibling navigation: Ctrl+← for previous, Ctrl+→ for next. Siblings are agents with the same parent_uuid.

**Key Decisions (from questions.md):**
- Use `ik_repl_switch_agent()` for all switching (saves/restores state)
- Only agents in `agents[]` array are running (dead ones removed)

**API:**
```c
// Switch to previous sibling (wraps around)
res_t ik_repl_nav_prev_sibling(ik_repl_ctx_t *repl);

// Switch to next sibling (wraps around)
res_t ik_repl_nav_next_sibling(ik_repl_ctx_t *repl);
```

**Behavior:**
- Find all siblings (same parent_uuid) in agents[] array
- Order by created_at (see note below)
- Navigate wraps around (last → first, first → last)
- If no siblings, no action

**Note (from questions.md Q8):** The `agents[]` array maintains creation order by construction:
- Fork always appends new agents to end of array
- Agents are never reordered
- Insertion order = creation order (no explicit sort needed)
- Only running agents are in the array (dead agents are removed)

## TDD Cycle

### Red
1. Add declarations to `src/repl.h`

2. Create tests:
   - Test nav_next with siblings switches to next
   - Test nav_next wraps to first after last
   - Test nav_prev switches to previous
   - Test nav_prev wraps to last from first
   - Test no siblings = no action
   - Test only counts running siblings

3. Run `make check` - expect test failures

### Green
1. Implement in `src/repl.c`:
   ```c
   res_t ik_repl_nav_next_sibling(ik_repl_ctx_t *repl)
   {
       const char *parent = repl->current->parent_uuid;

       // Collect siblings (all in agents[] are running)
       size_t sibling_count = 0;
       ik_agent_ctx_t *siblings[64];
       for (size_t i = 0; i < repl->agent_count; i++) {
           ik_agent_ctx_t *a = repl->agents[i];
           if (/* same parent */) {
               siblings[sibling_count++] = a;
           }
       }

       if (sibling_count <= 1) return OK(NULL);

       // Find current index and switch to next
       size_t current_idx = 0;
       for (size_t i = 0; i < sibling_count; i++) {
           if (siblings[i] == repl->current) {
               current_idx = i;
               break;
           }
       }
       size_t next_idx = (current_idx + 1) % sibling_count;
       CHECK(ik_repl_switch_agent(repl, siblings[next_idx]));
       return OK(NULL);
   }
   ```

2. Wire up event handlers in input processing

3. Run `make check` - expect pass

### Refactor
1. Verify using ik_repl_switch_agent for state preservation
2. Consider caching sibling list
3. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (Red, Green, Refactor)

## Post-conditions
- `make check` passes
- `make lint` passes
- Ctrl+←/→ navigates siblings
- Wraps around at ends
- No action if no siblings
- Working tree is clean (all changes committed)
