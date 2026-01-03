# Task: Separator Navigation Context

## Target
User Story: rel-06/user-stories/20-separator-context.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (sections on agent relationships and registry)

## Pre-read Source (patterns - READ)
- src/layer_wrappers.h (separator layer interface)
- src/layer_separator.c (current separator implementation)
- src/repl.h (agent relationships and state)

## Pre-read Tests (patterns - READ)
- tests/unit/layer/separator_layer_test.c (existing separator tests)

## Files to CREATE/MODIFY
- src/layer_separator.c (MODIFY - add navigation context rendering)
- tests/unit/layer/separator_layer_test.c (MODIFY - add navigation context tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- agents-tree-cmd.md task complete (provides agent tree structure)

## Task
Update separator to show navigation context: parent, siblings, children indicators.

**Key Decisions (from questions.md):**
- Dead agents are removed from `agents[]` array when killed - not kept in memory
- Gray out unavailable navigation directions (prevent invalid navigation via UI)
- Prevention over error handling - make invalid navigation impossible via UI, not runtime errors

**Format:**
```
───────── ↑abc... ←xyz... [def123...] →ghi... ↓2 ─────────
```

**Indicators:**
- `↑abc...` - parent UUID (or grayed `-` if root or parent dead)
- `←xyz...` - previous sibling (or grayed `-` if none)
- `[current]` - current agent UUID
- `→ghi...` - next sibling (or grayed `-` if none)
- `↓2` - child count (or grayed `-` if none)

**Grayed-out styling:**
- Unavailable directions use dim/gray ANSI color
- Available directions use normal color
- This communicates to user which nav commands will work

## TDD Cycle

### Red
1. Update `tests/unit/layer/separator_layer_test.c`:
   - Test shows parent UUID when parent alive and in agents array
   - Test shows grayed `-` for root (no parent)
   - Test shows grayed `-` when parent was killed (not in agents array)
   - Test shows sibling indicators (←previous, →next)
   - Test shows grayed sibling indicators when no siblings
   - Test shows child count when running children exist
   - Test shows grayed `-` when no running children
   - Test UUIDs truncated appropriately (first 6 chars + "...")
   - Test grayed indicators use dim ANSI color code
   - Test navigation context integrated with existing debug info display

2. Run `make check` - expect test failures

### Green
1. Update `src/layer_separator.c`:
   - Add navigation context data structure to separator layer data
   - Add function to set navigation context (parent UUID, sibling UUIDs, child count)
   - Update `separator_render()` to format navigation indicators
   - Implement UUID truncation (6 chars + "..." for non-current)
   - Add ANSI dim/gray color for unavailable directions
   - Balance navigation context with existing debug info display

2. Update `src/layer_wrappers.h`:
   - Add function signature for setting navigation context

3. Run `make check` - expect pass

### Refactor
1. Review integration with existing debug info
2. Consider separator width allocation between nav context and debug
3. Ensure clean memory management (talloc-based)
4. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- Separator shows navigation context in format: `↑parent ←prev [current] →next ↓count`
- Unavailable directions show grayed `-` indicator
- Indicators update on agent switch (when context pointers updated)
- Working tree is clean (all changes committed)
