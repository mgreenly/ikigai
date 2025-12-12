# Task: Reduce Scroll Burst Threshold to 10ms

## Target

Input Handling: Reduce scroll burst detection threshold from 20ms to 10ms for faster mouse wheel response.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/scm.md

## Pre-read Source
- src/scroll_detector.h (IK_SCROLL_BURST_THRESHOLD_MS constant)
- src/scroll_detector.c (uses the constant)
- tests/unit/scroll_detector/scroll_detector_test.c (tests using the constant)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

The scroll detector uses a timing-based burst threshold to distinguish mouse wheel events (rapid arrow key sequences) from keyboard arrow presses. The current threshold is 20ms.

Keyboard auto-repeat is never faster than ~30ms, so 10ms is safe and will provide slightly more responsive mouse wheel detection.

## Task

Change `IK_SCROLL_BURST_THRESHOLD_MS` from 20 to 10.

## TDD Cycle

### Red

Update tests in `tests/unit/scroll_detector/scroll_detector_test.c` that reference the 20ms threshold to use 10ms instead. Tests should fail because the constant hasn't changed yet.

### Green

Change the constant in `src/scroll_detector.h`:
```c
#define IK_SCROLL_BURST_THRESHOLD_MS 10
```

Run `make check` - tests should pass.

### Verify

1. `make check` - all tests pass
2. `make lint` - no issues

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `IK_SCROLL_BURST_THRESHOLD_MS` is 10
