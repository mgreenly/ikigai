# Error Handling Testing and Coverage:

## Table of Contents

### Testing
- [Testing Strategy](#testing-strategy)
  - [Organization](#organization)
  - [Testing Result Types](#testing-result-types)
  - [Testing Assertions](#testing-assertions)
  - [Testing FATAL()](#testing-fatal)

### Coverage
- [Coverage Requirements](#coverage-requirements)
  - [Policy for Assertions](#policy-for-assertions)
  - [Policy for FATAL()](#policy-for-fatal)

### Special Cases
- [Out-of-Memory Handling](#out-of-memory-handling)
  - [Thread Safety](#thread-safety)

### Summary
- [Summary](#summary)
  - [Testing Checklist](#testing-checklist)
  - [Coverage Rules](#coverage-rules)

### Related Documentation
- **[error_handling.md](error_handling.md)** - Core philosophy and mechanisms
- **[error_patterns.md](error_patterns.md)** - Detailed patterns and best practices

---

## Testing Strategy

### Organization

- `tests/unit/` - One file per source module (1:1 mapping)
- `tests/integration/` - Cross-module tests

### Testing Result Types

**Testing IO Operations:**
```c
START_TEST(test_config_load_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    res_t res = config_load(ctx, "fixtures/valid.json");
    ck_assert(is_ok(&res));

    ik_cfg_t *config = res.ok;
    ck_assert_str_eq(config->api_key, "test-key");

    talloc_free(ctx);
}
END_TEST
```

**Note:** Out of memory conditions are no longer testable via injection - they cause immediate PANIC (abort). OOM handling cannot be tested in unit tests since the process terminates.

### Testing Assertions

**Testing Assertions (Contract Violations):**
```c
#ifndef NDEBUG
START_TEST(test_array_null_asserts) {
    ik_array_get(NULL, 0);  // Should abort
}
END_TEST
#endif

// In suite setup:
#ifndef NDEBUG
tcase_add_test_raise_signal(tc, test_array_null_asserts, SIGABRT);
#endif
```

**How it works:** Test runs in forked child process, assertion fires → `abort()` → child terminates with `SIGABRT` → parent catches and verifies signal.

### Testing PANIC()

Similar to assertions, test PANIC() calls with `tcase_add_test_raise_signal`:

```c
START_TEST(test_impossible_state_panic) {
    // Set up condition that should trigger PANIC()
    // ...
    function_that_calls_panic();  // Should abort
}
END_TEST

// In suite setup:
tcase_add_test_raise_signal(tc, test_impossible_state_panic, SIGABRT);
```

**Note:** Unlike assertions, PANIC() tests are not wrapped in `#ifndef NDEBUG` since PANIC() exists in release builds.

**OOM Testing:** Out of memory conditions that call PANIC cannot be tested via injection - they immediately terminate the process. OOM paths are not testable in unit tests.

---

## Coverage Requirements

### Policy for Assertions

All assertions must be excluded from branch coverage, but both assertion paths must be tested.

**Implementation:**
1. Mark all assertions with `// LCOV_EXCL_BR_LINE` to exclude from coverage
2. Write tests that **pass** the assertion (normal path)
3. Write tests that **fail** the assertion (using `tcase_add_test_raise_signal` with `SIGABRT`)

**Rationale:**
- Assertions compile out in release builds (`-DNDEBUG`), creating untested branches in production code
- Excluding them from coverage metrics prevents artificially low coverage scores
- We still test both paths to verify contracts during development
- This maintains 100% coverage without compromising test quality

**Example:**
```c
// In src/array.c:
void *ik_array_get(const ik_array_t *array, size_t index)
{
    assert(array != NULL); // LCOV_EXCL_BR_LINE
    assert(index < array->size); // LCOV_EXCL_BR_LINE

    return (char *)array->data + (index * array->element_size);
}

// In tests/unit/array_test.c:
// Test 1: Normal path (assertion passes)
START_TEST(test_array_get_valid)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int), 10);
    ik_array_t *array = res.ok;

    int value = 42;
    ik_array_append(array, &value);

    int *result = ik_array_get(array, 0);  // Valid - assertion passes
    ck_assert_int_eq(*result, 42);

    talloc_free(ctx);
}
END_TEST

// Test 2: Contract violation (assertion fails)
#ifndef NDEBUG
START_TEST(test_array_get_null_array)
{
    ik_array_get(NULL, 0);  // NULL array - assertion fires SIGABRT
}
END_TEST

START_TEST(test_array_get_out_of_bounds)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int), 10);
    ik_array_t *array = res.ok;

    ik_array_get(array, 0);  // Out of bounds (size=0) - assertion fires SIGABRT

    talloc_free(ctx);
}
END_TEST
#endif
```

### Policy for PANIC()

PANIC() calls create branches that lead to `abort()`. These should be marked with `// LCOV_EXCL_BR_LINE` (for OOM checks) or `// LCOV_EXCL_LINE` (for logic errors):

```c
// OOM check - exclude branch
void *ptr = ik_talloc_zero_wrapper(ctx, size);
if (ptr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

// Logic error - exclude entire line
if (array->size > array->capacity) {
    PANIC("Array corruption: size > capacity");  // LCOV_EXCL_LINE
}
```

**Important:** Adding new exclusions requires updating `LCOV_EXCL_COVERAGE` in the Makefile. This is a tracked metric to prevent coverage erosion. Request permission with clear justification showing why the branch is untestable.

**OOM branches:** All OOM checks (allocation failures) should be marked `// LCOV_EXCL_BR_LINE` since they cannot be tested after removing OOM injection infrastructure.

---

## Out-of-Memory Handling

Out of memory conditions are handled by calling `PANIC("Out of memory")` which immediately terminates the process. This cannot be tested via injection in unit tests.

The error creation function `_make_error()` also uses PANIC when it fails to allocate an error structure, ensuring consistent OOM behavior throughout the system.

### Thread Safety

All errors are allocated independently on talloc contexts. No shared error state exists.

---

## Summary

### Testing Checklist

- Test Result error paths
- Test assertion violations (SIGABRT, debug only)
- Test PANIC() calls for logic errors (SIGABRT, all builds)
- Mark assertions with `// LCOV_EXCL_BR_LINE`
- Mark OOM checks with `// LCOV_EXCL_BR_LINE`
- Mark PANIC() logic errors with `// LCOV_EXCL_LINE`

### Coverage Rules

- 100% coverage requirement for lines, functions, and branches
- Assertions excluded from branch coverage (`LCOV_EXCL_BR_LINE`)
- OOM checks excluded from branch coverage (`LCOV_EXCL_BR_LINE`)
- PANIC() logic errors excluded from line coverage (`LCOV_EXCL_LINE`)
- Assertion paths must still be tested even when excluded
- PANIC() logic error paths should be tested when practical
- OOM paths cannot be tested (removed OOM injection infrastructure)
- New exclusions require user permission and Makefile updates

---

**Principle:** Test everything. Exclude only what compiles out or aborts. Maintain 100% coverage of production code paths.
