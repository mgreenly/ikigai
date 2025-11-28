# Error Handling Patterns and Best Practices

## Table of Contents

### Assertion Patterns
- [Assertion Best Practices](#assertion-best-practices)
  - [1. Assert Both Sides](#1-assert-both-sides)
  - [2. Split Compound Assertions](#2-split-compound-assertions)
  - [3. Every Public Function Asserts Context](#3-every-public-function-asserts-context)
  - [4. Array Access Always Bounds-Checked](#4-array-access-always-bounds-checked)
  - [5. Assert Side-Effect Free](#5-assert-side-effect-free)
  - [6. Use Implications with If-Statements](#6-use-implications-with-if-statements)

### Testing
- [Testing Assertions](#testing-assertions)
  - [Test Both Paths](#test-both-paths)
  - [Why Test Assertions?](#why-test-assertions)

### Validation
- [User Input Validation](#user-input-validation)
  - [The Trust Boundary](#the-trust-boundary)
  - [User Input Functions Must](#user-input-functions-must)
  - [Pattern: Trust Boundary](#pattern-trust-boundary)
  - [What Constitutes "User Input"?](#what-constitutes-user-input)
  - [Principle](#principle)

### Common Patterns
- [Common Result Patterns](#common-result-patterns)
  - [Pattern 1: Simple function with error](#pattern-1-simple-function-with-error)
  - [Pattern 2: Using TRY for clean extraction](#pattern-2-using-try-for-clean-extraction)
  - [Pattern 3: Propagating with CHECK](#pattern-3-propagating-with-check)
  - [Pattern 4: Per-Request Context](#pattern-4-per-request-context)

### Related Documentation
- **[error_handling.md](error_handling.md)** - Core philosophy and mechanisms
- **[error_testing.md](error_testing.md)** - Testing strategy and coverage requirements

---

## Assertion Best Practices

### 1. Assert Both Sides

Don't just assert what **must be** - also assert what **must never be**:

```c
assert(count > 0);          // Must be positive
assert(count <= MAX_SIZE);  // Must not exceed limit
```

### 2. Split Compound Assertions

Prefer separate assertions for clearer failure messages:

```c
// Good - shows which condition failed
assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
assert(array != NULL);      // LCOV_EXCL_BR_LINE

// Bad - can't tell which pointer is NULL
assert(ctx != NULL && array != NULL);  // LCOV_EXCL_BR_LINE
```

### 3. Every Public Function Asserts Context

```c
assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
```

This should be the first line of every function taking a talloc context.

### 4. Array Access Always Bounds-Checked

```c
void *ik_array_get(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    return (char *)array->data + (index * array->element_size);
}
```

### 5. Assert Side-Effect Free

Never put functional code in assertions:

```c
// Bad - increments only in debug builds!
assert(count++ < MAX);

// Good - assert doesn't change behavior
count++;
assert(count <= MAX);  // LCOV_EXCL_BR_LINE
```

### 6. Use Implications with If-Statements

For logical implications, use readable if-statements:

```c
// Good - clear and readable
if (has_data) assert(buffer != NULL);  // LCOV_EXCL_BR_LINE

// Bad - requires mental translation
assert(!has_data || buffer != NULL);  // LCOV_EXCL_BR_LINE
```

---

## Testing Assertions

All assertions must be tested to verify they fire correctly when contracts are violated.

### Test Both Paths

1. **Normal path** - assertion passes
2. **Violation path** - assertion fires (SIGABRT)

```c
// Test 1: Normal usage - assertion passes
START_TEST(test_array_get_valid) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int), 10);
    ik_array_t *array = unwrap(&res);

    int value = 42;
    ik_array_append(array, &value);

    int *result = ik_array_get(array, 0);  // Valid - passes
    ck_assert_int_eq(*result, 42);

    talloc_free(ctx);
}
END_TEST

// Test 2: Contract violation - assertion fires
#ifndef NDEBUG
START_TEST(test_array_get_null_array_asserts) {
    ik_array_get(NULL, 0);  // NULL array - fires SIGABRT
}
END_TEST

START_TEST(test_array_get_out_of_bounds_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int), 10);
    ik_array_t *array = unwrap(&res);

    ik_array_get(array, 0);  // Out of bounds (size=0) - fires SIGABRT

    talloc_free(ctx);
}
END_TEST
#endif

// Register tests in suite
void array_suite(void) {
    Suite *s = suite_create("Array");
    TCase *tc = tcase_create("Core");

    // Normal path
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_array_get_valid);

    // Assertion violation tests (debug only)
#ifndef NDEBUG
    tcase_add_test_raise_signal(tc, test_array_get_null_array_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc, test_array_get_out_of_bounds_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc);
}
```

### Why Test Assertions?

Since assertions compile out in release builds, you might wonder why test them at all:

1. **Verify guard rails work** - Assertions should fire when you expect
2. **Prevent silent removal** - If someone removes an assertion, test fails
3. **Document contracts** - Tests show what violations look like
4. **Catch development bugs** - Prove that invalid calls fail fast in debug builds

---

## User Input Validation

### The Trust Boundary

Functions that accept user input are the **trust boundary**. These functions are responsible for exhaustive validation and must never crash on bad input.

```
┌─────────────────────────────────────┐
│  User Input (untrusted boundary)    │
├─────────────────────────────────────┤
│  Input Validation Layer             │  ← Result types, exhaustive checks
│  - parse_command()                  │  ← Never assert on input content
│  - validate_filepath()              │  ← Handle all possible bad input
│  - parse_json()                     │
├─────────────────────────────────────┤
│  Internal Functions                 │  ← assert() on contracts
│  - process_validated_command()      │  ← Can assert preconditions
│  - array_operations()               │  ← Trust caller validated
├─────────────────────────────────────┤
│  Deep Logic                         │  ← FATAL() on impossible states
│  - state_machine_transition()       │  ← Validated but now impossible?
│  - switch defaults after enum check │
└─────────────────────────────────────┘
```

### User Input Functions Must:

1. **Validate exhaustively** - Account for ALL possible bad input
2. **Return Result types** - Never crash on bad input
3. **Provide clear error messages** - Help users fix their mistakes
4. **Never assert on input content** - Only assert on internal preconditions

### Pattern: Trust Boundary

```c
// Public API - accepts untrusted user input
res_t handle_user_command(TALLOC_CTX *ctx, const char *cmd) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE - our precondition, not user input

    // Validate ALL possible bad input
    if (cmd == NULL) {
        return ERR(ctx, INVALID_ARG, "Command cannot be NULL");
    }

    if (strlen(cmd) == 0) {
        return ERR(ctx, INVALID_ARG, "Command cannot be empty");
    }

    if (strlen(cmd) > MAX_COMMAND_LENGTH) {
        return ERR(ctx, INVALID_ARG, "Command too long (max %d chars)",
                   MAX_COMMAND_LENGTH);
    }

    if (!is_valid_format(cmd)) {
        return ERR(ctx, PARSE, "Invalid command format: %s", cmd);
    }

    // Now pass validated data to internal function
    return process_command(ctx, cmd);
}

// Internal - trusts caller validated input
static res_t process_command(TALLOC_CTX *ctx, const char *cmd) {
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(cmd != NULL);              // LCOV_EXCL_BR_LINE
    assert(strlen(cmd) > 0);          // LCOV_EXCL_BR_LINE
    assert(strlen(cmd) <= MAX_COMMAND_LENGTH);  // LCOV_EXCL_BR_LINE

    // Can assert because caller validated
    // ...
}
```

### What Constitutes "User Input"?

User input includes:
- Command-line arguments
- Terminal input (keystrokes, escape sequences)
- File contents being parsed
- Environment variables
- Configuration files
- Network data (future)

**Rule:** If it comes from outside your process boundaries, validate exhaustively.

### Principle

**Assertions are for contracts between YOUR functions, not for validating external input.**

---

## Common Result Patterns

### Pattern 1: Simple function with error

```c
res_t parse_port(TALLOC_CTX *ctx, const char *str) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(str != NULL);  // LCOV_EXCL_BR_LINE

    char *endptr;
    long port = strtol(str, &endptr, 10);

    if (*endptr != '\0')
        return ERR(ctx, INVALID_ARG, "Invalid port: %s", str);
    if (port < 1024 || port > 65535)
        return ERR(ctx, OUT_OF_RANGE, "Port out of range: %ld", port);

    int *result = ik_talloc_wrapper(ctx, int);
    if (!result) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *result = (int)port;
    return OK(result);
}
```

### Pattern 2: Using TRY for clean extraction

```c
res_t config_load(TALLOC_CTX *ctx, const char *path) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(path != NULL);  // LCOV_EXCL_BR_LINE

    char *expanded = TRY(expand_tilde(ctx, path));  // Clean!

    FILE *f = fopen(expanded, "r");
    if (!f) return ERR(ctx, IO, "Cannot open: %s", expanded);

    // ... parse and return config ...
}
```

### Pattern 3: Propagating with CHECK

```c
res_t server_init(TALLOC_CTX *ctx, const char *path) {
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(path != NULL);  // LCOV_EXCL_BR_LINE

    res_t res = config_load(ctx, path);
    CHECK(res);  // Return early if error

    ik_cfg_t *config = res.ok;
    res = config_validate(ctx, config);
    CHECK(res);

    // ... initialize server ...
}
```

### Pattern 4: Per-Request Context

```c
int websocket_callback(request, response, user_data) {
    ws_connection_t *conn = user_data;
    TALLOC_CTX *msg_ctx = talloc_new(conn->ctx);

    message_t *msg = TRY(message_parse(msg_ctx, json_data));
    // ... process ...

    talloc_free(msg_ctx);  // Frees msg and all allocations
    return 0;
}
```

---

**Principle:** Validate at boundaries. Assert internal contracts. Use clear, consistent patterns.
