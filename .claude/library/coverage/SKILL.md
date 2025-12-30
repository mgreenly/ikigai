---
name: coverage
description: Coverage Policy skill for the ikigai project
---

# Coverage Policy

## Description
The 100% coverage requirement and exclusion rules for ikigai.

## The Requirement

**100% coverage of Lines, Functions, and Branches for the ENTIRE codebase.**

This is absolute:
- 100% of ALL code, not just new code
- A single uncovered line or branch in ANY file blocks ALL commits
- Fix ALL gaps before committing

## Philosophy

Coverage gaps reveal design opportunities. They tell you:
- This code path isn't tested (risk)
- This code might be unreachable (dead code)
- This code is hard to test (design smell)

Fix the design, don't silence the messenger.

## Exclusion Rules

### Allowed Exclusions

Only these may use `LCOV_EXCL_BR_LINE`:
1. `assert()` - Compiled out in release builds
2. `PANIC()` - Invariant violations that terminate

```c
assert(ptr != NULL);                    // LCOV_EXCL_BR_LINE
if (!ptr) PANIC("Invariant violated");  // LCOV_EXCL_BR_LINE
```

**Important:** These markers are NOT reliably honored inside `static` functions. See `style.md` "Avoid Static Functions" rule - inline code instead of using static helpers to ensure 100% branch coverage is achievable.

### Never Exclude

- Defensive programming checks
- Library error returns
- System call failures
- "Should never happen" branches
- Any code reachable at runtime

If it can execute in production, it must be tested.

## Critical Rules

1. **Never use exclusions without explicit user permission**
2. **Never change LCOV_EXCL_COVERAGE in Makefile without permission**
3. **Never generate HTML coverage reports** (slow, unnecessary)

## Verification

```bash
make BUILD=coverage check
cat reports/coverage/summary.txt
```

All three metrics must show 100%.

## Related Skills

- `lcov` - Finding and understanding gaps
- `mocking` - Testing external dependencies
- `testability` - Refactoring for better tests
