# Error Handling and Defensive Programming

**Quick Reference:**
- Philosophy (3 mechanisms): L16-85
- Result Types - Core API: L87-130
- Result Types - Macros: L132-153
- Result Types - Memory Management: L155-180
- Result Types - Common Patterns: L182-234
- Assertions: L236-469
- FATAL() - Unrecoverable Logic Errors: L471-539
- User Input Validation: L541-650
- Decision Framework: L652-703
- Testing Strategy: L705-803
- Coverage Requirements: L805-878
- Out-of-Memory Handling: L880-895

---

## Philosophy: Three Mechanisms for Three Problems

Ikigai uses three complementary mechanisms to handle different types of problems:

1. **`Result`** - Runtime error handling for expected failures
2. **`assert()`** - Development-time contract enforcement
3. **`FATAL()`** - Production crashes for unrecoverable logic errors

This creates a clear taxonomy of failure modes with appropriate responses for each.

---

### 1. Result Types - Expected Runtime Errors

**Use for:** IO operations, resource allocation, parsing - any external failure

**Characteristics:**
- Failures are unpredictable (OOM, disk full, network down, malformed input)
- Must be handled gracefully by caller
- Caller decides how to recover or propagate
- Always present in both debug and release builds

**Examples:**
```c
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment);
res_t config_load(TALLOC_CTX *ctx, const char *path);
res_t message_parse(TALLOC_CTX *ctx, const char *json);
```

**Operations that return Result:**
- Heap allocation
- File I/O, network operations
- Parsing user input or external data
- Resource acquisition

---

### 2. Assertions - Development-Time Contracts

**Use for:** Preconditions, postconditions, invariants, programmer errors

**Characteristics:**
- Failures indicate bugs in YOUR code, not external conditions
- Fire immediately during development with clear messages
- Compile out in release builds (`-DNDEBUG`) - zero runtime cost
- Enable fearless refactoring with fast feedback

**Examples:**
```c
void ik_array_delete(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    // ... implementation
}
```

**What to assert:**
- NULL checks on pointer parameters
- Array bounds checks
- Valid state for operations
- Relationships between parameters (`start <= end`)
- Internal data structure consistency

---

### 3. FATAL() - Unrecoverable Logic Errors

**Use for:** Data corruption, impossible states that should never occur

**Characteristics:**
- Failures indicate severe logic errors or corruption
- Present in both debug and release builds
- Continuing would be more dangerous than crashing
- Should be extremely rare (~1-2 per 1000 lines of code)

**Examples:**
```c
if (array->size > array->capacity) {
    FATAL("Array corruption: size > capacity");
}

switch (state) {
    case STATE_INIT: /* ... */ break;
    case STATE_READY: /* ... */ break;
    case STATE_DONE: /* ... */ break;
    default:
        FATAL("Invalid state in state machine");
}
```

**When to use FATAL():**
- Data structure invariant violations detected at runtime
- Impossible state combinations
- Switch defaults that should never be reached
- Post-validation logic errors

---

## Result Types - Core API

**Result Structure:**
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

**When to Return `void` vs `res_t`:**

A function must return `void` (not `res_t`) if:
1. It performs no IO operations (no file/network/allocation that can fail)
2. It returns only information the caller already has (like echoing back a pointer parameter)
3. All meaningful results are communicated via output parameters

**Rule: Functions that don't perform IO and only return information the caller already has must return `void`.**

---

## Result Types - Macros

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

## Result Types - Memory Management

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

## Result Types - Common Patterns

**Pattern 1: Simple function with error**
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

    int *result = talloc(ctx, int);
    *result = (int)port;
    return OK(result);
}
```

**Pattern 2: Using TRY for clean extraction**
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

**Pattern 3: Propagating with CHECK**
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

---

## Assertions: Defensive Coding

### Purpose

Assertions in ikigai are **development-time contract enforcement mechanisms**. They exist to:

1. **Accelerate development** - Catch bugs immediately with clear error messages
2. **Document contracts** - Make preconditions, postconditions, and invariants explicit
3. **Enable fearless refactoring** - Changes that violate contracts fail fast in tests
4. **Prevent cascading failures** - Stop bad calls from causing confusing downstream errors

**Assertions compile out in release builds (`-DNDEBUG`)** - they provide zero runtime overhead in production.

### Philosophy

Assertions are **not** production safety mechanisms. They are development aids that:
- Help you know immediately when you use a function incorrectly
- Serve as executable documentation of function contracts
- Provide fast feedback during development and testing
- Catch integration bugs before they cause mysterious failures

Think of assertions as **extensions to the type system** - documenting and enforcing constraints that C's type system cannot express (non-NULL, in-bounds, valid state).

### When to Assert

**Assert liberally.** Since assertions compile out in release builds, there is no cost to being thorough.

#### Preconditions (Function Inputs)

Assert all preconditions at function entry:

```c
void ik_array_delete(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    // ... implementation
}
```

**What to check:**
- Pointer parameters are non-NULL (unless explicitly nullable)
- Indices are within valid ranges
- State is valid for the operation
- Relationships between parameters (`start <= end`)

#### Invariants (Internal Consistency)

Assert internal consistency throughout functions:

```c
void process_buffer(buffer_t *buf) {
    assert(buf != NULL);                    // LCOV_EXCL_BR_LINE
    assert(buf->data != NULL);              // LCOV_EXCL_BR_LINE
    assert(buf->size <= buf->capacity);     // LCOV_EXCL_BR_LINE

    // ... do work ...

    // Recheck critical invariants after modifications
    assert(buf->size <= buf->capacity);     // LCOV_EXCL_BR_LINE
}
```

#### Postconditions (Function Outputs)

Assert outcomes before returning:

```c
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment) {
    assert(ctx != NULL);                // LCOV_EXCL_BR_LINE
    assert(element_size > 0);           // LCOV_EXCL_BR_LINE

    ik_array_t *array = /* ... allocate ... */;
    if (!array) return ERR(ctx, OOM, "Failed to allocate array");

    array->size = 0;
    array->capacity = increment;

    assert(array->size == 0);           // LCOV_EXCL_BR_LINE
    assert(array->capacity > 0);        // LCOV_EXCL_BR_LINE

    return OK(array);
}
```

### Assertion Best Practices

#### 1. Assert Both Sides

Don't just assert what **must be** - also assert what **must never be**:

```c
assert(count > 0);          // Must be positive
assert(count <= MAX_SIZE);  // Must not exceed limit
```

#### 2. Split Compound Assertions

Prefer separate assertions for clearer failure messages:

```c
// Good - shows which condition failed
assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
assert(array != NULL);      // LCOV_EXCL_BR_LINE

// Bad - can't tell which pointer is NULL
assert(ctx != NULL && array != NULL);  // LCOV_EXCL_BR_LINE
```

#### 3. Every Public Function Asserts Context

```c
assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
```

This should be the first line of every function taking a talloc context.

#### 4. Array Access Always Bounds-Checked

```c
void *ik_array_get(ik_array_t *array, size_t index) {
    assert(array != NULL);           // LCOV_EXCL_BR_LINE
    assert(index < array->size);     // LCOV_EXCL_BR_LINE
    return (char *)array->data + (index * array->element_size);
}
```

#### 5. Assert Side-Effect Free

Never put functional code in assertions:

```c
// Bad - increments only in debug builds!
assert(count++ < MAX);

// Good - assert doesn't change behavior
count++;
assert(count <= MAX);  // LCOV_EXCL_BR_LINE
```

#### 6. Use Implications with If-Statements

For logical implications, use readable if-statements:

```c
// Good - clear and readable
if (has_data) assert(buffer != NULL);  // LCOV_EXCL_BR_LINE

// Bad - requires mental translation
assert(!has_data || buffer != NULL);  // LCOV_EXCL_BR_LINE
```

### Testing Assertions

All assertions must be tested to verify they fire correctly when contracts are violated.

#### Test Both Paths

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

#### Why Test Assertions?

Since assertions compile out in release builds, you might wonder why test them at all:

1. **Verify guard rails work** - Assertions should fire when you expect
2. **Prevent silent removal** - If someone removes an assertion, test fails
3. **Document contracts** - Tests show what violations look like
4. **Catch development bugs** - Prove that invalid calls fail fast in debug builds

---

## FATAL() - Unrecoverable Logic Errors

### Purpose

While assertions compile out in release builds, some logic errors indicate such severe corruption that continuing would be more dangerous than crashing. For these rare cases, use `FATAL()`.

### The FATAL() Macro

**Location:** `src/abort.h`

```c
#ifndef IK_ABORT_H
#define IK_ABORT_H

#include <stdio.h>
#include <stdlib.h>

// Abort with formatted message, file, and line
// Use for unrecoverable logic errors that indicate corruption
// or impossible states. Should be used sparingly (~1-2 per 1000 LOC).
#define FATAL(msg) \
    do { \
        fprintf(stderr, "FATAL: %s\n  at %s:%d\n", \
                (msg), __FILE__, __LINE__); \
        fflush(stderr); \
        abort(); \
    } while(0)

#endif
```

### When to Use FATAL()

**Use sparingly** - approximately 1-2 per 1000 lines of code.

#### ✅ Good Candidates:

**1. Data structure corruption detected:**
```c
void resize_array(array_t *array) {
    if (array->size > array->capacity) {
        // Fundamental invariant violated - data structure corrupted
        FATAL("Array corruption: size > capacity");
    }
    // ...
}
```

**2. Impossible state combinations:**
```c
if (state == STATE_CLOSED && fd >= 0) {
    // Logically impossible - closed state should never have valid fd
    FATAL("Inconsistent state: closed but fd valid");
}
```

**3. Switch defaults (always):**
```c
// If all cases are covered, default should never be reached
switch (action.type) {
    case ACTION_INSERT: /* ... */ break;
    case ACTION_DELETE: /* ... */ break;
    case ACTION_MOVE: /* ... */ break;
    default:
        FATAL("Invalid action type in switch");
}

// Even for internal state machines - not just validated user input
switch (state) {
    case STATE_INIT: /* ... */ break;
    case STATE_READY: /* ... */ break;
    case STATE_DONE: /* ... */ break;
    default:
        FATAL("Invalid state in state machine");
}
```

**4. Post-validation logic errors:**
```c
// After extensive validation of file format, structure, checksums, etc.
if (/* condition that validation should have prevented */) {
    FATAL("Validation passed but invariant violated");
}
```

#### ❌ Don't Use FATAL() For:

**1. Precondition checks** - Use `assert()`:
```c
// Bad
if (ptr == NULL) FATAL("NULL pointer");

// Good
assert(ptr != NULL);  // LCOV_EXCL_BR_LINE
```

**2. Expected errors** - Use `Result`:
```c
// Bad
if (file_open_failed) FATAL("Can't open file");

// Good
return ERR(ctx, IO, "Cannot open file: %s", path);
```

**3. Rare but possible cases** - Handle gracefully:
```c
// Bad - race conditions, external modifications can cause this
if (file_size_changed) FATAL("File size changed");

// Good
return ERR(ctx, IO, "File was modified during read");
```

### FATAL() vs assert()

**Key distinction:** assert() compiles out in release builds (`-DNDEBUG`), FATAL() is always present.

**Use `assert()` for:**
- **Precondition checks** - Caller's responsibility (NULL pointers, bounds checks)
- **Contract violations** - Bugs in how functions are called
- **Development-time verification** - Catching programmer errors during testing

**Use `FATAL()` for:**
- **Unreachable code** - Switch defaults, impossible states after validation
- **Data structure corruption** - Invariants violated at runtime
- **Internal logic errors** - Conditions that should be impossible with correct code

**Critical principle:** If reaching a code location means the program is in an undefined/corrupted state, use `FATAL()`. Never let the program continue in an unknown state - immediate termination in production is safer than undefined behavior.

**Examples:**
```c
// assert() - precondition, caller's bug
void process(int *data) {
    assert(data != NULL);  // LCOV_EXCL_BR_LINE - caller should never pass NULL
    // ...
}

// FATAL() - unreachable code, internal corruption
switch (validated_enum) {
    case VALUE_A: /* ... */ break;
    case VALUE_B: /* ... */ break;
    default:
        FATAL("Invalid enum value after validation");  // Should never reach here
}
```

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
│  - state_machine_transition()      │  ← Validated but now impossible?
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

## Decision Framework

When something goes wrong, use this decision tree:

### 1. Can this happen with correct code and valid input?
   - **Yes** → Use `Result` (e.g., file not found, out of memory, network timeout)
   - **No** → Continue to #2

### 2. Is this a precondition / function contract violation?
   - **Yes** → Use `assert()` (e.g., NULL pointer passed by caller, out-of-bounds index)
   - **No** → Continue to #3

### 3. Is this unreachable code or impossible internal state?
   - **Yes** → Use `FATAL()` (e.g., switch defaults, enum values after validation, corrupted data structures)
   - **No** → Reconsider - you may have a precondition (step 2) or expected error (step 1)

**Key insight:** If you reach code that should be impossible to reach, that's `FATAL()` territory. The program is in an undefined state and must terminate immediately, even in production.

### Quick Reference

| Situation | Mechanism | Reason |
|-----------|-----------|--------|
| User passed NULL | `Result` | Expected error - validate at boundary |
| Internal function received NULL | `assert()` | Contract violation - caller's bug |
| File not found | `Result` | Expected runtime error |
| Out of memory | `Result` | Expected runtime error |
| Array size > capacity | `FATAL()` | Data corruption - unrecoverable |
| Invalid enum after validation | `FATAL()` | Logic error - should be impossible |
| NULL pointer check | `assert()` | Precondition - development aid |
| Bounds check | `assert()` | Precondition - development aid |
| Switch default | `FATAL()` | Should never be reached |

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

### Three Mechanisms

1. **`Result`** - Runtime errors
   - Expected failures (IO, resources, user input)
   - Must be handled by caller
   - Clear error messages

2. **`assert()`** - Development-time contracts
   - Compiles out in release builds (`-DNDEBUG`)
   - Fast feedback during development
   - Use liberally - zero cost

3. **`FATAL()`** - Unrecoverable logic errors
   - Production crashes for corruption
   - Use sparingly (~1-2 per 1000 LOC)
   - When continuing is more dangerous than crashing

### Key Principles

- **Assert liberally** - Zero cost, huge development value
- **Validate exhaustively** at trust boundaries
- **Crash explicitly** when corruption detected
- **Handle errors gracefully** when recovery is possible

### Testing

- Test Result error paths
- Test assertion violations (SIGABRT, debug only)
- Test FATAL() calls (SIGABRT, all builds)
- Mark assertions with `// LCOV_EXCL_BR_LINE`
- Mark FATAL() with `// LCOV_EXCL_LINE`

---

**Principle:** Make errors explicit. Make contracts visible. Make corruption impossible to ignore.
