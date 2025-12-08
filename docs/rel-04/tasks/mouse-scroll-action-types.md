# Task: Mouse Scroll Action Types

## Target
Feature: Mouse wheel scrolling

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/return_values.md

### Pre-read Source (patterns)
- src/input.h (ik_input_action_type_t enum, ik_input_action_t struct)

### Pre-read Tests (patterns)
- tests/unit/input/input_test.c (existing input action tests)

## Pre-conditions
- `make check` passes

## Task
Add `IK_INPUT_SCROLL_UP` and `IK_INPUT_SCROLL_DOWN` to the `ik_input_action_type_t` enum in `src/input.h`.

These action types will be emitted by the input parser when SGR mouse scroll events are detected. For now, just add the enum values - the parser and handler will be added in subsequent tasks.

## TDD Cycle

### Red
1. Add test in `tests/unit/input/input_test.c`:
   - Test that `IK_INPUT_SCROLL_UP` and `IK_INPUT_SCROLL_DOWN` enum values exist
   - Simple compile-time verification by using them in assertions
2. Run `make check` - expect compilation failure (enum values don't exist)

### Green
1. Add to `ik_input_action_type_t` enum in `src/input.h`:
   - `IK_INPUT_SCROLL_UP` after `IK_INPUT_PAGE_DOWN`
   - `IK_INPUT_SCROLL_DOWN` after `IK_INPUT_SCROLL_UP`
2. Run `make check` - expect pass

### Refactor
1. Ensure enum values are grouped logically with scroll-related actions
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `IK_INPUT_SCROLL_UP` and `IK_INPUT_SCROLL_DOWN` exist in enum
- No behavioral changes yet (just type definitions)
