---
name: coverage
description: Coverage policy - 80% requirement and exclusion rules
---

# Coverage Policy

## The Requirement

**80% coverage of Lines, Functions, and Branches for EACH file in the codebase.**

- Each file must have >= 80% line coverage, >= 80% function coverage, and >= 80% branch coverage
- All three metrics must be >= 80% for each file
- Fix gaps to reach 80% threshold for each file before committing

## Decision Framework

When you encounter uncovered code:

```
Can this code path execute in production?
├─ No → ACCEPT exclusion (assert/PANIC only)
└─ Yes
   └─ What triggers it?
      ├─ User input / External data → MUST TEST
      ├─ Environment / IO failure → WRAP AND MOCK (see mocking skill)
      ├─ Vendor library error → WRAP AND MOCK
      ├─ OOM condition → REFACTOR to PANIC (see testability skill)
      ├─ Function can never fail → REFACTOR res_t to void
      └─ Broken invariant → REFACTOR to PANIC
```

## Exclusion Policy

### Allowed (LCOV_EXCL_BR_LINE only)

1. **`assert()`** - Compiled out in release builds
2. **`PANIC()`** - Invariant violations that terminate

Must be single-line. Multi-line blocks require refactoring.

### Never Exclude

- Defensive programming checks (test them)
- Library error returns (wrap and mock)
- System call failures (wrap and mock)
- "Should never happen" branches (PANIC if truly impossible)
- Any code reachable at runtime

**If it can execute in production, it must be tested.**

## Critical Rules

1. Never use exclusions without explicit user permission
2. Never change LCOV_EXCL_COVERAGE in Makefile without permission
3. Never generate HTML coverage reports (slow, unnecessary)

## Incremental Progress

While 80% per file is the goal, progress toward 80% is valid work:
- Easy wins first
- Commit incrementally
- Respect context limits

## Verification

```bash
make check-coverage
```

All three metrics must show >= 80% for each file.

## Related Skills

- `lcov` - Finding gaps, reading coverage files, marker syntax
- `testability` - Refactoring patterns for hard-to-test code
- `mocking` - Testing external dependencies
