# Task: Ctrl+Arrow Input Handling

## Target
User Stories: 12-navigate-siblings.md, 13-navigate-parent.md, 14-navigate-child.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md

## Pre-read Docs
- project/agent-process-model.md (Navigation section)

## Pre-read Source (patterns to READ)
- src/input.h (input handling - existing event types and structures)
- src/input.c (key event processing - existing parsing logic)
- src/input_escape.h (escape sequence handling)
- tests/unit/input/*_test.c (existing test patterns)

## Files to MODIFY
- src/input.h (add new navigation event types to enum)
- src/input.c (add Ctrl+Arrow key detection and mapping)

## Files to CREATE
- tests/unit/input/nav_arrows_test.c (new test file for navigation events)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- kill-cmd-cascade.md task complete

## Task
Add input handling for Ctrl+Arrow keys to trigger agent navigation.

**Key mappings:**
- Ctrl+← : Previous sibling
- Ctrl+→ : Next sibling
- Ctrl+↑ : Parent agent
- Ctrl+↓ : Child agent

**Event types:**
```c
typedef enum {
    // ... existing ...
    IK_INPUT_NAV_PREV_SIBLING,
    IK_INPUT_NAV_NEXT_SIBLING,
    IK_INPUT_NAV_PARENT,
    IK_INPUT_NAV_CHILD,
} ik_input_event_t;
```

## TDD Cycle

### Red
1. Create test file `tests/unit/input/nav_arrows_test.c`

2. Write tests for Ctrl+Arrow key combinations:
   - Test Ctrl+Left generates NAV_PREV_SIBLING
   - Test Ctrl+Right generates NAV_NEXT_SIBLING
   - Test Ctrl+Up generates NAV_PARENT
   - Test Ctrl+Down generates NAV_CHILD
   - Test plain arrows unchanged (no Ctrl modifier)

3. Run `make check` - expect test failures (events not yet defined)

4. Commit RED phase using haiku sub-agent

### Green
1. Add navigation event types to `src/input.h` enum:
   - IK_INPUT_NAV_PREV_SIBLING
   - IK_INPUT_NAV_NEXT_SIBLING
   - IK_INPUT_NAV_PARENT
   - IK_INPUT_NAV_CHILD

2. Update input parser in `src/input.c`:
   - Detect Ctrl modifier in CSI sequences
   - Map Ctrl+Arrow sequences to navigation events
   - Ensure plain arrows remain unchanged

3. Run `make check` - expect all tests to pass

4. Commit GREEN phase using haiku sub-agent

### Refactor
1. Review and refactor if needed:
   - Ensure CSI sequence detection is clean
   - Check for code duplication
   - Verify naming conventions match project style

2. Run `make check` - verify still passing

3. Run `make lint` - verify clean

4. Commit REFACTOR phase using haiku sub-agent (if changes made)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (RED, GREEN, REFACTOR)

## Post-conditions
- `make check` passes
- `make lint` passes
- Ctrl+Arrow keys generate correct navigation events
- Plain arrows unchanged (no regression)
- Working tree is clean (all changes committed in TDD phases)
