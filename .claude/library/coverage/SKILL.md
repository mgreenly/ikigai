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

**Fix the design, don't silence the messenger.**

Key lesson: **Don't design code to make error paths impossible - that's what tests are for!** If we check a condition in production, both success AND failure paths must be tested.

## Incremental Progress

While 100% is the goal, **progress toward 100% is valid work**. When working on coverage:

1. **Easy wins first** - Tackle simple branches before complex edge cases
2. **Commit incrementally** - Each improvement captured is better than none
3. **Respect context limits** - A partial improvement committed beats exhausting context with nothing saved

The policy is 100%. The practice is steady, committed progress toward it.

## Decision Framework

When you encounter an uncovered branch/line, use this tree:

```
Can this code path execute in production?
├─ No → ACCEPT exclusion (unreachable code - assert/PANIC only)
└─ Yes
   └─ What triggers it?
      ├─ User input / External data → MUST TEST (no exclusions)
      ├─ Environment / IO failure → ADD TEST (wrap and mock)
      ├─ Vendor library error → ADD WRAPPER AND TEST (see below)
      ├─ OOM condition → REFACTOR to single-line PANIC
      ├─ Function can never fail → REFACTOR res_t to void
      └─ Broken invariant → REFACTOR to single-line PANIC
```

## Vendor Function Error Paths

**Critical Rule: If we check vendor function results, we MUST test both paths.**

### Anti-Pattern: "Logically Impossible" Branches

```c
// BAD: Validation makes error path unreachable
yyjson_doc *doc = yyjson_read_file(...);
if (!doc) return ERR(...);

yyjson_val *root = yyjson_doc_get_root(doc);
if (!root) {  // "Can't happen - doc is valid!"
    return ERR(...);  // Uncovered, marked as "library internal"
}
```

This is a **design flaw**:
- The error check IS our error handling
- Making it "impossible" prevents testing the path we depend on
- Inline library functions still create branches we must cover

### Correct Pattern: Wrap and Test

```c
// GOOD: Wrapper allows testing both paths
yyjson_doc *doc = yyjson_read_file_(...);  // Wrapped
if (!doc) return ERR(...);  // Tested with mock failure

yyjson_val *root = yyjson_doc_get_root_(doc);  // Wrapped
if (!root) return ERR(...);  // Tested with mock returning NULL
```

**Action for inline vendor functions:**
1. Add wrapper to `wrapper.c/wrapper.h` using MOCKABLE pattern
2. Write tests mocking both success and failure
3. **No exclusions** - both paths must be covered

## Common Refactoring Patterns

### 1. OOM Checks → Single-Line PANIC

When allocations now PANIC on failure, downstream checks become unreachable:

```c
// FROM (3 lines, 2 exclusions):
result_t res = allocate_something();
if (is_err(&res)) {                // LCOV_EXCL_LINE
    return res;                     // LCOV_EXCL_LINE
}

// TO (1 line, 1 exclusion):
result_t res = allocate_something();
if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
```

### 2. Infallible Functions → void

If a function never fails, the type system should say so:

```c
// FROM: Fake error handling
res_t calculate(obj_t *obj, int value) {
    obj->field = value * 2;
    return OK(obj);
}
// Callers:
res_t r = calculate(obj, 5);
if (is_err(&r)) return r;  // LCOV_EXCL_LINE - never fails!

// TO: Honest signature
void calculate(obj_t *obj, int value) {
    obj->field = value * 2;
}
// Callers:
calculate(obj, 5);  // No fake error check
```

### 3. Unreachable Else Branches

When if/else covers all cases but one is impossible:

```c
// FROM: Unreachable else
if (condition_always_true) {
    handle();
} else {
    // Can't happen due to precondition
    return ERR(...);  // LCOV_EXCL_LINE
}

// TO: Assert the invariant
if (condition_always_true) {
    handle();
} else {
    PANIC("Invariant violated");  // LCOV_EXCL_BR_LINE
}
```

## Exclusion Rules

### Allowed Exclusions (LCOV_EXCL_BR_LINE)

**Only these two cases:**

1. **`assert()`** - Compiled out in release builds
2. **`PANIC()`** - Invariant violations that terminate

```c
assert(ptr != NULL);                    // LCOV_EXCL_BR_LINE
if (!ptr) PANIC("Invariant violated");  // LCOV_EXCL_BR_LINE
```

**Format:** Must be single-line. Multi-line blocks require refactoring.

**Important:** These markers are NOT reliably honored inside `static` functions. See `style.md` "Avoid Static Functions" rule.

### Never Exclude

- Defensive programming checks (test them!)
- Library error returns (wrap and test!)
- System call failures (mock them!)
- "Should never happen" branches (PANIC if truly impossible)
- Any code reachable at runtime
- Inline vendor function branches (wrap the function!)

**If it can execute in production, it must be tested.**

## Critical Rules

1. **Never use exclusions without explicit user permission**
2. **Never change LCOV_EXCL_COVERAGE in Makefile without permission**
3. **Never generate HTML coverage reports** (slow, unnecessary)
4. **Never accept "library internal branches" as untestable** - wrap the library function

## Verification

```bash
make BUILD=coverage check
cat reports/coverage/summary.txt
```

All three metrics must show 100%.

## Related Skills

- `lcov` - Finding and understanding gaps
- `mocking` - Testing external dependencies (includes wrapping vendor functions)
- `testability` - Refactoring for better tests

## References

- `project/lcov_exclusion_strategy.md` - Full decision tree and patterns
- `project/LCOV_EXCLUSIONS.md` - Current exclusions with justifications
