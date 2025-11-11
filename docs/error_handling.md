# Error Handling

**Quick Reference:**
- Philosophy (3 modes): L9-44
- Core API: L46-89
- Macros (OK/ERR/CHECK/TRY): L91-112
- Memory Management: L114-139
- Common Patterns: L141-193
- Assertions: L195-232
- Defensive Aborts: L237-270
- Testing: L274-328
- Coverage Requirements for Assertions: L335-407

---

## Philosophy: Three Categories of Operations

**1. IO Operations (External Failures)** → Return `res_t`
- Heap allocation, file I/O, network operations
- Failures are external and unpredictable (OOM, disk full, network down)
- Must be handled gracefully
- Examples: `create()`, `append()`, `load_file()`, `send_request()`

**2. Contract Violations (Programmer Errors)** → Use `assert()`
- Invalid arguments that should never happen with correct code
- NULL pointers, out-of-bounds indices, invalid states
- Failures indicate bugs, not runtime conditions
- Fast in release builds (asserts compile out with `-DNDEBUG`)
- Examples: passing NULL to function expecting valid pointer, array bounds violations

**3. Pure Operations (Infallible)** → Return value directly or `void`
- Cannot fail with valid inputs
- No side effects, no resource allocation
- Examples: `size()`, `capacity()`, `is_empty()`

**When to Return `void` vs `res_t`:**

A function must return `void` (not `res_t`) if:
1. It performs no IO operations (no file/network/allocation that can fail)
2. It returns only information the caller already has (like echoing back a pointer parameter)
3. All meaningful results are communicated via output parameters

**Rule: Functions that don't perform IO and only return information the caller already has must return `void`.**

**Examples:**
```c
// BAD: Returns OK(parser) - just echoing the input pointer
res_t ik_input_parse_byte(ik_input_parser_t *parser, char byte, ik_input_action_t *action_out);

// GOOD: Returns void - result communicated via action_out parameter
void ik_input_parse_byte(ik_input_parser_t *parser, char byte, ik_input_action_t *action_out);

// GOOD: Returns res_t - can fail (IO) and returns new information
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment);
```

**Rationale:** Distinguishes conditions that must be handled (external failures) from bugs that must be fixed (contract violations). Reduces noise at call sites while maintaining safety during development.

**Assertions Strategy (NASA Power of 10 inspired):**
- Check preconditions, postconditions, invariants, anomalous conditions
- Must be side-effect free
- Zero cost in production (`-DNDEBUG`)
- Every public function checks `ctx != NULL`
- Every pointer parameter gets NULL check unless explicitly nullable
- Array operations assert bounds
- Don't just assert what **must be** - consider what **must never be**

---

## Core Design

**Result Type:**
```c
typedef struct {
    union { void *ok; err_t *err; };
    bool is_err;
} res_t;
```

**Error Structure:**
```c
typedef struct err {
    err_code_t code;
    const char *file;
    int32_t line;
    char message[128];
} err_t;
```

**Error Codes:** Start empty, add organically as needed
```c
typedef enum {
    OK = 0,
    ERR_OOM, ERR_INVALID_ARG, ERR_OUT_OF_RANGE,
    ERR_IO, ERR_PARSE
} err_code_t;
```

**Inspection Functions:**
```c
bool is_ok(const res_t *result);
bool is_err(const res_t *result);
err_code_t error_code(const err_t *err);
const char *error_message(const err_t *err);
void error_fprintf(FILE *f, const err_t *err);
```

---

## Macros

**Creating Results:**
```c
return OK(value);                              // Success
return ERR(ctx, IO, "Cannot open: %s", path);  // Error (allocates on ctx)
```

**Propagating Errors:**
```c
// CHECK - propagate full result (when you don't need the value immediately)
res_t res = config_load(ctx, path);
CHECK(res);
ik_cfg_t *config = res.ok;

// TRY - extract value or propagate error (for inline use)
char *path = TRY(expand_tilde(ctx, "~/.config"));  // Cleaner!
ik_cfg_t *config = TRY(config_load(ctx, path));
```

**When to use CHECK vs TRY:**
- `CHECK`: When you need to inspect the result before using it
- `TRY`: When you just want the value (most common case)
- Both return early on error

---

## Memory Management Integration

**Ownership Rules:**
1. Caller provides `TALLOC_CTX *ctx`
2. Results (success values and errors) allocated as children of `ctx`
3. Caller frees context - single `talloc_free(ctx)` cleans everything

**Lifecycle Example:**
```c
TALLOC_CTX *root = talloc_new(NULL);

ik_cfg_t *config = TRY(config_load(root, "~/.ikigai/config.json"));
int result = server_run(root, config);

talloc_free(root);  // Frees config, server state, everything
return result;
```

**Per-Request Context Pattern:**
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

## Common Patterns

**Pattern 1: Simple function with error**
```c
res_t parse_port(TALLOC_CTX *ctx, const char *str) {
    assert(ctx != NULL);
    assert(str != NULL);

    char *endptr;
    long port = strtol(str, &endptr, 10);

    if (*endptr != '\0')
        return ERR(ctx, INVALID_ARG, "Invalid port: %s", str);
    if (port < 1024 || port > 65535)
        return ERR(ctx, OUT_OF_RANGE, "Port out of range: %ld", port);

    int *result = talloc(ctx, int);
    *result = (int)port;
    return OK(result);
}
```

**Pattern 2: Using TRY for clean extraction**
```c
res_t config_load(TALLOC_CTX *ctx, const char *path) {
    assert(ctx != NULL);
    assert(path != NULL);

    char *expanded = TRY(expand_tilde(ctx, path));  // Clean!

    FILE *f = fopen(expanded, "r");
    if (!f) return ERR(ctx, IO, "Cannot open: %s", expanded);

    // ... parse and return config ...
}
```

**Pattern 3: Propagating with CHECK**
```c
res_t server_init(TALLOC_CTX *ctx, const char *path) {
    assert(ctx != NULL);
    assert(path != NULL);

    res_t res = config_load(ctx, path);
    CHECK(res);  // Return early if error

    ik_cfg_t *config = res.ok;
    res = config_validate(ctx, config);
    CHECK(res);

    // ... initialize server ...
}
```

---

## Assertions: Defensive Coding

**What to assert:**
- Preconditions: `assert(ctx != NULL)`, `assert(path != NULL)`
- Invariants: `assert(index < size)`, `assert(refcount > 0)`
- Postconditions: `assert(result != NULL)` after allocation
- Anomalies: `assert(state != CORRUPTED)`

**Example:**
```c
res_t ik_array_get(const ik_array_t *array, size_t index) {
    assert(array != NULL);
    assert(index < array->size);  // Bounds check

    void *element = (char *)array->data + (index * array->element_size);
    return OK(element);
}
```

**Guidelines:**
- Every public function: `assert(ctx != NULL)`
- Every pointer parameter: NULL check unless explicitly nullable
- Array access: `assert(index < size)`
- State transitions: `assert(state == EXPECTED_STATE)`
- Use liberally - zero cost in release builds

**Not for:**
- External failures (file I/O, network, allocation) - those use `res_t`
- User input validation - those return `ERR()`

---

## Defensive Aborts: Handling "Impossible" Conditions

**When to use:** Internal invariants that should never be violated if other (tested) code works correctly.

**Pattern:**
```c
if (impossible_condition) {
    fprintf(stderr, "diagnostic message %s:%d\n", __FILE__, __LINE__); // LCOV_EXCL_LINE
    abort(); // LCOV_EXCL_LINE
}
```

**Rationale:**
- Different from assertions (doesn't compile out with `-DNDEBUG`)
- Different from `res_t` errors (not recoverable - indicates broken invariants)
- Provides production diagnostics if the "impossible" ever happens
- Marked `LCOV_EXCL_LINE` because coverage assumes working preconditions

**Example:** workspace_multiline.c:88-89
```c
if ((first_byte & 0x80) == 0) {
    char_len = 1; // ASCII
} else if ((first_byte & 0xE0) == 0xC0) {
    char_len = 2;
} else if ((first_byte & 0xF0) == 0xE0) {
    char_len = 3;
} else if ((first_byte & 0xF8) == 0xF0) {
    char_len = 4;
} else {
    fprintf(stderr, "invalid UTF-8 %s:%d\n", __FILE__, __LINE__); // LCOV_EXCL_LINE
    abort(); // LCOV_EXCL_LINE
}
```

**When invalid UTF-8 is encountered (which should never happen given the precondition), you get a clear diagnostic message to stderr showing exactly which line hit the abort, making debugging easier if this "impossible" case ever occurs.**

**Use this pattern when:**
- Other code guarantees the invariant (e.g., UTF-8 validation elsewhere)
- Continuing would leave the program in an unknown state
- You want diagnostics even in production builds
- The condition truly should never happen with correct code

**Branch Coverage for Defensive Aborts:**
When the condition leading to abort() creates an untestable branch (the FALSE branch that falls through to the else clause), mark the condition with `// LCOV_EXCL_BR_LINE`. Example:
```c
} else if ((first_byte & 0xF8) == 0xF0) { // LCOV_EXCL_BR_LINE
    char_len = 4;
} else {
    fprintf(stderr, "invalid UTF-8 %s:%d\n", __FILE__, __LINE__); // LCOV_EXCL_LINE
    abort(); // LCOV_EXCL_LINE
}
```

**Important:** Adding new exclusions requires updating `LCOV_EXCL_COVERAGE` in the Makefile. This is a tracked metric to prevent coverage erosion. Request permission with clear justification showing why the branch is untestable.

---

## Testing Strategy

**Organization:**
- `tests/unit/` - One file per source module (1:1 mapping)
- `tests/integration/` - Cross-module tests
- `tests/test_utils.*` - OOM injection infrastructure

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

---

## Coverage Requirements for Assertions

**Policy:** All assertions must be excluded from branch coverage, but both assertion paths must be tested.

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

// In suite setup:
#ifndef NDEBUG
tcase_add_test_raise_signal(tc, test_array_get_null_array, SIGABRT);
tcase_add_test_raise_signal(tc, test_array_get_out_of_bounds, SIGABRT);
#endif
```

**Coverage Result:** 100% branch coverage despite excluded assertion branches, with full test coverage of all code paths.

---

## Out-of-Memory Handling

If `talloc_zero()` fails allocating an error, return global static OOM error:
```c
return &oom_error;  // Static, read-only, safe to access from any thread
```

Allows propagation up the stack for graceful handling instead of immediate abort.

## Thread Safety

All errors allocated independently on talloc contexts. Only shared state is read-only `oom_error`, safe for concurrent access.

---

**Principle:** Errors are values, not control flow. Make them explicit, make them cheap, make them impossible to ignore.
