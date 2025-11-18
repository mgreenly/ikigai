# Test Coverage

## Absolute Requirement

**100% coverage of Lines, Functions, AND Branches for ENTIRE codebase.**

A single uncovered line or branch in ANY file blocks ALL commits. Fix ALL gaps before committing.

## Finding Coverage Gaps

```bash
# Uncovered lines
grep "^DA:" coverage/coverage.info | grep ",0$"

# Uncovered branches
grep "^BRDA:" coverage/coverage.info | grep ",0$"
```

Output format: `DA:line_number,0` or `BRDA:line_number,block,branch,0`

## LCOV Exclusions: Strict Policy

**ONLY use exclusions for:**
1. `assert()` statements on function parameters and invariants
2. `PANIC()` statements (unconditional termination)

**NEVER use exclusions for:**
- Defensive programming that checks runtime conditions
- Library function error returns (malloc, yyjson, curl, POSIX calls)
- Any code path reachable at runtime
- "Logically impossible" branches made unreachable by validation

**Format requirement:** Single-line only
```c
// GOOD:
if (invariant_broken) PANIC("message"); // LCOV_EXCL_LINE

// BAD (multi-line):
if (invariant_broken) {  // LCOV_EXCL_LINE
    PANIC("message");     // LCOV_EXCL_LINE
}
```

**Exclusion types:**
- `LCOV_EXCL_BR_LINE` - exclude branch coverage only (for asserts, PANICs)
- `LCOV_EXCL_LINE` - exclude line coverage (rare, usually for unreachable code)
- `LCOV_EXCL_START/STOP` - exclude entire blocks (wrapper.c test infrastructure only)

## Coverage Strategies (in priority order)

### 1. Write Tests
Default approach. If code can execute, write a test that exercises it.

### 2. Mock External Dependencies
**Use wrapper.c/wrapper.h MOCKABLE pattern for:**
- External library functions (talloc, yyjson, curl)
- System calls (POSIX: open, stat, mkdir, tcgetattr, ioctl, read, write)

**Pattern:**
```c
// In src/wrapper.h - add declaration
MOCKABLE int posix_write_(int fd, const void *buf, size_t count);

// In test - override with weak symbol
int posix_write_(int fd, const void *buf, size_t count) {
    return -1;  // Inject failure
}
```

**MOCKABLE = zero overhead:**
- Debug/test builds: weak symbols (tests can override)
- Release builds: static inline (compiler optimizes away)

**NEVER mock our own code** - only external dependencies.

### 3. Wrap Inline Functions
For vendor inline functions with defensive branches (e.g., yyjson):

**Problem:** Inline functions like `yyjson_doc_get_root()` contain ternaries (`doc ? doc->root : NULL`) that create branch coverage requirements at EVERY call site.

**Solution:** Wrap in regular function, test both branches once.

**Example:** See `src/openai/sse_parser.h` and `src/openai/sse_parser.c`
```c
// sse_parser.h - exposed for testing
yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc);

// sse_parser.c - simple wrapper
yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc) {
    return yyjson_doc_get_root(doc);  // Inline function with ternary
}

// Test both branches once
START_TEST(test_wrapper_null) {
    yyjson_val *root = yyjson_doc_get_root_wrapper(NULL);
    ck_assert_ptr_null(root);
}
END_TEST

START_TEST(test_wrapper_valid) {
    yyjson_doc *doc = create_test_doc();
    yyjson_val *root = yyjson_doc_get_root_wrapper(doc);
    ck_assert_ptr_nonnull(root);
}
END_TEST
```

Then use `yyjson_doc_get_root_wrapper()` in production code instead of the inline version.

### 4. Refactor for Testability
Always encouraged. Examples:
- Extract logic into testable functions
- Reduce cyclomatic complexity
- Split functions with mixed concerns
- Convert functions that can't fail from `res_t` to `void`

### 5. Single-Line LCOV Exclusions
**ONLY for asserts and PANICs.**

```c
// Parameter validation (contract enforcement)
assert(ptr != NULL);     // LCOV_EXCL_BR_LINE
assert(len > 0);         // LCOV_EXCL_BR_LINE

// Invariant checks that should never trigger
if (allocation_failed) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
```

## Common Patterns

### Pattern: Library Error Paths
**WRONG:**
```c
yyjson_val *root = yyjson_doc_get_root(doc);
if (!root) {  // LCOV_EXCL_LINE - "logically impossible"
    return ERR(ctx, PARSE, "No root");
}
```

**RIGHT:**
```c
// Add to wrapper.h/wrapper.c if not already there
MOCKABLE yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc);

// Use wrapper in production code
yyjson_val *root = yyjson_doc_get_root_(doc);
if (!root) {  // Tested via mock
    return ERR(ctx, PARSE, "No root");
}

// In test - override to inject failure
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc) {
    return NULL;  // Simulate failure
}
```

### Pattern: System Call Errors
**Use wrappers from wrapper.h:**
```c
// Production code
int fd = posix_open_("/dev/tty", O_RDWR);
if (fd < 0) {  // Testable via mock
    return ERR(ctx, IO, "Failed to open terminal");
}

// Test - override weak symbol
int posix_open_(const char *pathname, int flags) {
    (void)pathname; (void)flags;
    return -1;  // Inject failure
}
```

### Pattern: OOM Handling
```c
// OOM causes PANIC - not testable, exclude the check
void *ptr = talloc_zero_(ctx, size);
if (!ptr) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
```

## Coverage Philosophy

**Coverage gaps are design feedback.** Before excluding:
1. Can I write a test? (default answer: yes)
2. Can I mock the external dependency? (wrapper.c/wrapper.h)
3. Can I wrap an inline function? (yyjson pattern)
4. Can I refactor to make testable? (always encouraged)
5. Is this truly an invariant check? (then exclude)

**Don't design code to make error paths impossible - that's what tests are for.**

## References

- `docs/lcov_exclusion_strategy.md` - Full exclusion strategy
- `docs/error_testing.md` - Error handling testing patterns
- `docs/build-system.md` - Coverage tooling and workflow
- `src/wrapper.h` - MOCKABLE pattern implementation
- `src/openai/sse_parser.h` - yyjson wrapper pattern example
