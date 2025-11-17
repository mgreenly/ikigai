# LCOV Exclusion Strategy

## Core Principle

**100% coverage is the goal**. Exclusions are rare exceptions, not the norm.

## Acceptable Exclusions

### 1. Truly Untestable Code

**ACCEPT** - Code that cannot be tested in a normal test harness:
- Functions that call `abort()` or `exit()` (panic handlers, fatal loggers)
- `main()` entry point (requires integration tests)
- Test infrastructure code (wrapper functions for mocking)

**Format**: Entire files or functions excluded with `LCOV_EXCL_START/STOP`

### 2. OOM Defensive Checks

**REFACTOR to single-line PANIC** - Error checks after allocations that now panic on failure:
- After OOM refactoring, allocations PANIC instead of returning errors
- Downstream `if (is_err(&res))` checks are now unreachable
- Convert multi-line checks to single-line PANIC statements

**Pattern**:
```c
// FROM (3 lines, 2 exclusions):
result_t res = allocate_something();
if (is_err(&res)) {                // LCOV_EXCL_LINE
    return res;                     // LCOV_EXCL_LINE
}

// TO (1 line, 1 exclusion):
result_t res = allocate_something();
if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_LINE
```

### 3. Broken Invariant Checks

**ACCEPT** - Defensive checks for internal consistency, not external errors:

**Criteria**:
- Checks internal code logic, not IO/environment/user input
- Indicates programming error or data corruption if triggered
- Should never occur in correct execution
- Must be single-line format

**Examples**:
- NULL pointer checks where creation guarantees non-NULL
- Bounds checks where caller guarantees valid range
- Enum validation in exhaustive switch default cases
- Data structure consistency checks

**Format**: Single line with `LCOV_EXCL_LINE` or `LCOV_EXCL_BR_LINE`

### 4. Environmental/IO Errors

**ADD TEST COVERAGE** - System and environment failures must be tested:
- Terminal initialization failures
- Filesystem operations (stat, mkdir, write)
- IO errors writing to terminal

**Approach**:
- Add wrappers to `wrapper.c/wrapper.h` for system calls
- Mock failures in tests
- Refactor code if needed to make testable
- **No exclusions** - these must be covered

### 5. Vendor Function Error Paths

**ADD WRAPPERS AND TEST** - If we rely on vendor function checks, we must test them:

**Anti-Pattern**: Making error paths "logically impossible" through validation
```c
// BAD: Validation makes error path unreachable
yyjson_doc *doc = yyjson_read_file(...);
if (!doc) {
    // Already validated - this can't happen!
    return ERR(ctx, PARSE, "Failed to parse");
}

yyjson_val *root = yyjson_doc_get_root(doc);
if (!root) {  // LCOV_EXCL_LINE - "logically impossible"
    return ERR(ctx, PARSE, "No root");
}
```

**Correct Pattern**: Wrap vendor functions and test both paths
```c
// GOOD: Wrappers allow testing both success and failure
yyjson_doc *doc = yyjson_read_file_(...);
if (!doc) {
    return ERR(ctx, PARSE, "Failed to parse");
}

yyjson_val *root = yyjson_doc_get_root_(doc);
if (!root) {  // Tested via mock in test
    return ERR(ctx, PARSE, "No root");
}
```

**Why This Matters**:
- If we rely on vendor function checks, they ARE our error handling
- Making paths "impossible" through validation is a coverage anti-pattern
- Design flaw: prevents testing the error handling we depend on
- Both success AND failure paths must be exercised in tests

**Approach**:
- Wrap ALL vendor functions whose results we check
- Use MOCKABLE pattern (weak symbols in debug, inline in release)
- Write tests that mock both success and failure
- **No exclusions** for vendor function error paths

**Key Lesson**: Don't design code to make error paths impossible - that's what tests are for!

## Unacceptable Exclusions

### User Input Validation

**MUST TEST** - Error handling for user input cannot be excluded:
- Invalid data from user
- Malformed input
- Out-of-range values from external sources

If code handles user input errors, it must be tested.

### Possible Edge Cases

**MUST TEST** - If a condition can actually occur, it must be tested:
- Boundary conditions
- Zero-width characters
- 4-byte UTF-8 sequences
- Rare but valid inputs

### Lazy Exclusions

**NOT ALLOWED**:
- "Too hard to test"
- "Unlikely to occur"
- Excluding because you don't want to write the test
- Bulk exclusions without individual justification

## Implementation Requirements

### Single-Line Format

All exclusions (except entire files) must be single-line:

```c
// GOOD:
if (invariant_violation) PANIC("invariant broken"); // LCOV_EXCL_LINE

// BAD (multi-line):
if (invariant_violation) {  // LCOV_EXCL_LINE
    PANIC("invariant broken"); // LCOV_EXCL_LINE
}
```

### Explicit Justification

Every exclusion must have a comment explaining why:

```c
// Environmental failure (no /dev/tty)
if (is_err(&result)) return result; // LCOV_EXCL_LINE

// Defensive: input buffer always has text when len > 0
if (text == NULL) PANIC("invariant violation"); // LCOV_EXCL_LINE
```

### Verification Requirements

When accepting exclusions based on invariants, verify:
- The invariant is actually maintained
- Input validation exists where claimed
- Preconditions are documented and enforced

## Decision Tree

```
Can this code path execute in production?
├─ No → ACCEPT exclusion (unreachable code)
└─ Yes
   └─ What triggers it?
      ├─ User input / External data → MUST TEST
      ├─ Environment / IO failure → ADD TEST (mock it)
      ├─ OOM condition → REFACTOR to PANIC
      ├─ Function can never fail → REFACTOR to void
      └─ Broken invariant / Data corruption → ACCEPT (single-line)
```

## Special Case: Functions That Can't Fail

**REFACTOR to `void`** - If a function returns `res_t` but can never fail:

**Symptoms**:
- Function only does arithmetic/logic (no allocations, no I/O)
- Always returns `OK()`
- All callers have unreachable error checks

**Pattern**:
```c
// FROM:
res_t calculate_something(obj_t *obj, int value) {
    obj->field = value * 2;  // Pure arithmetic
    return OK(obj);
}

// Callers have fake error checks:
res_t result = calculate_something(obj, 5);
if (is_err(&result)) return result; // LCOV_EXCL_LINE - Never fails!

// TO:
void calculate_something(obj_t *obj, int value) {
    obj->field = value * 2;
}

// Callers simplified:
calculate_something(obj, 5);
```

**Benefits**:
- Eliminates LCOV exclusions
- Type system tells the truth
- Code becomes clearer
- No performance cost

**When NOT to use**:
- Function might need error handling in the future (but see YAGNI)
- Function is part of a callback interface requiring `res_t`

## Review Process

For each exclusion:

1. **Understand context** - Read surrounding code, understand when it executes
2. **Classify** - Determine which category it belongs to
3. **Decide**:
   - **ACCEPT** - Legitimate exclusion, keep it (verify format)
   - **TEST** - Add test coverage, remove exclusion
   - **REFACTOR** - Change code to eliminate exclusion
   - **INVESTIGATE** - Need more information
4. **Document** - Update exclusion comment with clear justification
5. **Verify** - Confirm format and invariants during implementation

## Summary

**Philosophy**: Coverage gaps reveal design opportunities. When tempted to exclude, first ask: "Can I refactor to make this testable?"

**Default**: Add tests. Only exclude when truly justified.

**Quality**: Single-line format, clear justification, documented invariants.
