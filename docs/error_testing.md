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
- `tests/test_utils.*` - OOM injection infrastructure

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

**Testing OOM Behavior:**
```c
START_TEST(test_oom_handling) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    oom_test_fail_next_alloc();

    res_t res = ERR(ctx, IO, "Triggers OOM");
    ck_assert(is_err(&res));
    ck_assert(error_is_static(res.err));
    ck_assert_int_eq(res.err->code, ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}
END_TEST
```

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

### Testing FATAL()

Similar to assertions, test FATAL() calls with `tcase_add_test_raise_signal`:

```c
START_TEST(test_impossible_state_fatal) {
    // Set up condition that should trigger FATAL()
    // ...
    function_that_calls_fatal();  // Should abort
}
END_TEST

// In suite setup:
tcase_add_test_raise_signal(tc, test_impossible_state_fatal, SIGABRT);
```

**Note:** Unlike assertions, FATAL() tests are not wrapped in `#ifndef NDEBUG` since FATAL() exists in release builds.

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

### Policy for FATAL()

FATAL() calls create branches that lead to `abort()`. Like defensive aborts, these should be marked with `// LCOV_EXCL_LINE`:

```c
if (array->size > array->capacity) {
    FATAL("Array corruption: size > capacity");  // LCOV_EXCL_LINE
}
```

**Important:** Adding new exclusions requires updating `LCOV_EXCL_COVERAGE` in the Makefile. This is a tracked metric to prevent coverage erosion. Request permission with clear justification showing why the branch is untestable.

---

## Out-of-Memory Handling

If `talloc_zero()` fails allocating an error, return global static OOM error:
```c
return &oom_error;  // Static, read-only, safe to access from any thread
```

Allows propagation up the stack for graceful handling instead of immediate abort.

### Thread Safety

All errors allocated independently on talloc contexts. Only shared state is read-only `oom_error`, safe for concurrent access.

---

## Summary

### Testing Checklist

- Test Result error paths
- Test assertion violations (SIGABRT, debug only)
- Test FATAL() calls (SIGABRT, all builds)
- Mark assertions with `// LCOV_EXCL_BR_LINE`
- Mark FATAL() with `// LCOV_EXCL_LINE`

### Coverage Rules

- 100% coverage requirement for lines, functions, and branches
- Assertions excluded from branch coverage (`LCOV_EXCL_BR_LINE`)
- FATAL() calls excluded from line coverage (`LCOV_EXCL_LINE`)
- Both paths must still be tested even when excluded
- New exclusions require user permission and Makefile updates

---

**Principle:** Test everything. Exclude only what compiles out or aborts. Maintain 100% coverage of production code paths.
