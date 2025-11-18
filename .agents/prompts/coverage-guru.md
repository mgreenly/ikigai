# Test Coverage

## Requirement

**100% coverage (Lines, Functions, Branches) for ENTIRE codebase.** A single gap blocks commits.

## When You Hit a Coverage Gap

Coverage gaps reveal design opportunities. Ask yourself in this order:

1. **Can I write a test?** (default answer: yes)
2. **Can I mock the external dependency?** (wrapper.c/wrapper.h)
3. **Can I wrap an inline function?** (yyjson pattern)
4. **Can I refactor to make testable?** (always encouraged)
5. **Is this truly an invariant check?** (then exclude)

Default to writing tests, not exclusions.

## Finding Gaps

```bash
grep "^DA:" coverage/coverage.info | grep ",0$"    # Lines
grep "^BRDA:" coverage/coverage.info | grep ",0$"  # Branches
```

Output: `DA:line_number,0` or `BRDA:line_number,block,branch,0`

## Coverage Strategies (priority order)

### 1. Write Tests
Default. If code executes, test it.

### 2. Mock External Dependencies
Use `wrapper.h` MOCKABLE pattern for libraries (talloc, yyjson, curl) and system calls (open, stat, read, write):

```c
// wrapper.h
MOCKABLE int posix_write_(int fd, const void *buf, size_t count);

// test - override weak symbol
int posix_write_(int fd, const void *buf, size_t count) {
    return -1;  // Inject failure
}
```

MOCKABLE = zero overhead (weak symbols in debug, static inline in release). **Never mock our code** - only external deps.

### 3. Wrap Vendor Inline Functions
Problem: `yyjson_doc_get_root()` has ternary (`doc ? doc->root : NULL`) creating branches at every call.

Solution: Wrap once, test both branches once (see `src/openai/sse_parser.h`):

```c
// sse_parser.h
yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc);

// sse_parser.c
yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc) {
    return yyjson_doc_get_root(doc);
}

// Test NULL and valid cases once
```

Use wrapper in production code.

### 4. Refactor for Testability
Extract logic, reduce complexity, split functions, convert can't-fail `res_t` to `void`.

### 5. LCOV Exclusions (Last Resort)

**ONLY for:**
- `assert()` on parameters/invariants
- `PANIC()` on invariants

**NEVER for:**
- Runtime checks, library errors, system call errors, "logically impossible" branches

**Format:** Single-line only
```c
assert(ptr != NULL);                            // LCOV_EXCL_BR_LINE
if (!ptr) PANIC("Out of memory");               // LCOV_EXCL_BR_LINE
```

**Types:**
- `LCOV_EXCL_BR_LINE` - branch only (asserts/PANICs)
- `LCOV_EXCL_LINE` - line (rare)
- `LCOV_EXCL_START/STOP` - blocks (wrapper.c only)

## Quick Reference

**Library errors** → MOCKABLE wrapper → test via mock
**System calls** → use existing wrapper.h → test via override
**Inline ternaries** → wrap → test both branches once
**OOM** → PANIC → exclude check

## References

- `docs/lcov_exclusion_strategy.md` - Full strategy
- `docs/error_testing.md` - Error testing patterns
- `src/wrapper.h` - MOCKABLE implementation
- `src/openai/sse_parser.h` - Inline wrapper example
