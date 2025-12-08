# Task: Mouse Scroll End-to-End Test

## Target
Feature: Mouse wheel scrolling

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/return_values.md
- docs/memory.md

### Pre-read Source (patterns)
- src/input.h (input parser and action types)
- src/repl.h (ik_repl_ctx_t, viewport_offset)
- src/repl_actions.c (action processing)
- src/terminal.c (terminal init with mouse enable)

### Pre-read Tests (patterns)
- tests/integration/ (integration test patterns)
- tests/unit/input/input_test.c (input parsing tests)

## Pre-conditions
- `make check` passes
- Task `mouse-terminal-enable.md` completed
- Task `mouse-sgr-parse.md` completed
- Task `mouse-scroll-handler.md` completed

## Task
Create an end-to-end integration test verifying the complete mouse scroll flow:
1. Terminal init enables mouse reporting
2. SGR mouse scroll sequences are parsed correctly
3. Viewport offset is adjusted appropriately
4. Terminal cleanup disables mouse reporting

This test validates all components work together.

## TDD Cycle

### Red
1. Create `tests/integration/mouse_scroll_test.c`:
   - Test setup creates repl context with scrollback content (enough to require scrolling)
   - Simulate SGR scroll up sequence through input parser
   - Verify viewport_offset increases
   - Simulate SGR scroll down sequence
   - Verify viewport_offset decreases
   - Test scroll at boundaries (top and bottom)
2. Add test to Makefile integration test list
3. Run `make check` - verify tests execute and pass

### Green
1. Since all components are implemented, tests should pass
2. If any failures, debug the integration:
   - Check terminal mouse enable sequences
   - Verify parser state machine transitions
   - Confirm action handler clamps correctly
3. Run `make check` - expect pass

### Refactor
1. Ensure test coverage is comprehensive:
   - Multiple scroll events in sequence
   - Scroll when document fits in viewport (no-op)
   - Scroll after resize
2. Clean up test structure
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Full mouse scroll pipeline tested end-to-end
- 100% coverage on new mouse scroll code
