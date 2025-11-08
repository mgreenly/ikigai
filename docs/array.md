# Generic Expandable Array (ik_array)

## Overview

A generic, type-safe expandable array implementation for ikigai. Provides a reusable foundation for dynamic collections with zero-overhead typed wrappers.

## Design Pattern

**Two-layer approach:**

1. **Generic implementation** (`ik_array_t`)
   - Single implementation handles all element types
   - Element size is configurable at creation
   - All logic (growth, reallocation, operations) in one place
   - Uses `void*` for generic element handling

2. **Typed wrappers** (e.g., `ik_byte_array_t`, `ik_line_array_t`)
   - Type-safe interfaces for specific element types
   - Inline wrapper functions delegate to generic implementation
   - Zero overhead when compiled with optimization
   - Easy to add new types without duplicating logic

**Inspiration:** Based on generic_queue pattern from space-captain project.

## Generic Implementation

### Data Structure

```c
typedef struct ik_array {
    void *data;              // Allocated storage (element_size * capacity bytes), NULL until first use
    size_t element_size;     // Size of each element in bytes (must be > 0)
    size_t size;             // Current number of elements (starts at 0)
    size_t capacity;         // Current allocated capacity in elements (starts at 0)
    size_t increment;        // Size for first allocation (then double)
} ik_array_t;
```

### Error Handling

**Follows the three-category error handling strategy from `docs/error_handling.md`:**

**1. IO Operations (heap allocation)** → Return `res_t`
- `create()`, `append()`, `insert()` - Can fail due to OOM
- Return `OK(value)` on success or `ERR(ctx, ERR_OOM, ...)` on allocation failure

**2. Contract Violations (programmer errors)** → Use `assert()`
- NULL pointer checks, out-of-bounds indices
- Fast path in release builds (asserts compile out)
- Tested using Check's `tcase_add_test_raise_signal()` with `SIGABRT`

**3. Pure Operations (infallible)** → Return value directly
- `size()`, `capacity()`, `clear()` - Cannot fail with valid pointer
- Use `assert(array != NULL)` for NULL pointer defense

**Error codes** (added to `src/error.h` via TDD as tests require them):
- `ERR_INVALID_ARG` - Invalid parameter at creation (element_size=0, increment=0)
- `ERR_OOM` - Memory allocation failed

**Note**: Do not add error codes until a test actually needs them. Follow strict TDD.

See `docs/error_handling.md` for complete error handling philosophy.

### Core Operations

**Lifecycle (IO - Returns res_t):**
- `res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment)`
  - Creates new array with specified element size and initial capacity
  - `element_size` must be > 0 (returns `ERR(ctx, INVALID_ARG, "element_size must be > 0")` if 0)
  - `increment` must be > 0 (returns `ERR(ctx, INVALID_ARG, "increment must be > 0")` if 0)
  - **No memory allocated for data buffer** - allocation deferred until first append/insert
  - Returns `OK(array)` or `ERR(ctx, OOM, ...)` or `ERR(ctx, INVALID_ARG, ...)`
  - Array structure is owned by talloc context (freed when context is freed)

**Modification (IO - Returns res_t):**
- `res_t ik_array_append(ik_array_t *array, const void *element)`
  - Appends element to end of array
  - Grows array if needed (doubles capacity)
  - Validates: `assert(array != NULL)`, `assert(element != NULL)`
  - Returns `OK(NULL)` or `ERR(talloc_parent(array), ERR_OOM, ...)`

- `res_t ik_array_insert(ik_array_t *array, size_t index, const void *element)`
  - Inserts element at specified index
  - Shifts existing elements right
  - Validates: `assert(array != NULL)`, `assert(element != NULL)`, `assert(index <= array->size)`
  - Grows array if needed
  - Returns `OK(NULL)` or `ERR(talloc_parent(array), ERR_OOM, ...)`

**Modification (No IO - Direct return with asserts):**
- `void ik_array_delete(ik_array_t *array, size_t index)`
  - Removes element at index
  - Shifts remaining elements left
  - Validates: `assert(array != NULL)`, `assert(index < array->size)`
  - No allocation, cannot fail beyond contract violations

- `void ik_array_set(ik_array_t *array, size_t index, const void *element)`
  - Replaces element at index
  - Validates: `assert(array != NULL)`, `assert(element != NULL)`, `assert(index < array->size)`
  - No allocation, cannot fail beyond contract violations

- `void ik_array_clear(ik_array_t *array)`
  - Removes all elements (sets size to 0)
  - Does not free memory (capacity remains)
  - Validates: `assert(array != NULL)`
  - Cannot fail beyond contract violations

**Access (No IO - Direct return with asserts):**
- `void *ik_array_get(const ik_array_t *array, size_t index)`
  - Returns pointer to element at index
  - Validates: `assert(array != NULL)`, `assert(index < array->size)`
  - Returned pointer is valid until next modification (append/insert/delete invalidates)
  - No allocation, cannot fail beyond contract violations

**Queries (Pure - Direct return with asserts):**
- `size_t ik_array_size(const ik_array_t *array)`
  - Returns current number of elements
  - Validates: `assert(array != NULL)`

- `size_t ik_array_capacity(const ik_array_t *array)`
  - Returns allocated capacity
  - Validates: `assert(array != NULL)`

### Growth Strategy

**Lazy allocation policy**: Arrays NEVER allocate memory at creation time. Memory is allocated only when needed (on first append/insert).

**Creation behavior**:
- `ik_array_create()` sets `data = NULL`, stores `increment` parameter (must be > 0)
- No memory allocated until first element is added
- `size` and `capacity` both start at 0

**First allocation** (when `capacity == 0` and element needs to be added):
- Allocate `increment` elements
- Set `capacity = increment`
- Uses `ik_talloc_realloc_wrapper(ctx, NULL, element_size * increment)`

**Subsequent growth** (when `size == capacity` and element needs to be added):
- Double current capacity: `new_capacity = capacity * 2`
- Example: increment=10 → capacity goes 0 → 10 → 20 → 40 → 80 → ...
- Uses `ik_talloc_realloc_wrapper(ctx, data, element_size * new_capacity)`
- Maintains existing elements during growth
- **No overflow protection** - assumes realistic array sizes won't overflow size_t

**Validation at creation**:
- `element_size` must be > 0 (returns `ERR(ctx, INVALID_ARG, "element_size must be > 0")` if 0)
- `increment` must be > 0 (returns `ERR(ctx, INVALID_ARG, "increment must be > 0")` if 0)

## Typed Wrappers (Future Phases)

**Phase 0 includes ONLY the generic `ik_array_t` implementation.**

The generic `ik_array_t` will be wrapped by typed modules in future phases that provide type-safe, ergonomic APIs.

### Byte Array (ik_byte_array_t) - NOT IN PHASE 0

**Purpose**: Stores bytes (for UTF-8 text in dynamic input zone).

**Files**: `src/byte_array.c` + `src/byte_array.h`

**Created in Phase 1** when dynamic zone text storage is needed.

**Pattern**:
```c
// In src/byte_array.h
typedef ik_array_t ik_byte_array_t;

res_t ik_byte_array_create(TALLOC_CTX *ctx, size_t increment);
res_t ik_byte_array_append(ik_byte_array_t *array, uint8_t byte);
res_t ik_byte_array_insert(ik_byte_array_t *array, size_t index, uint8_t byte);
res_t ik_byte_array_get(const ik_byte_array_t *array, size_t index);
// ... etc

// In src/byte_array.c
res_t ik_byte_array_create(TALLOC_CTX *ctx, size_t increment) {
    return ik_array_create(ctx, sizeof(uint8_t), increment);
}

res_t ik_byte_array_append(ik_byte_array_t *array, uint8_t byte) {
    return ik_array_append(array, &byte);
}
// ... etc
```

### Line Array (ik_line_array_t) - NOT IN PHASE 0

**Purpose**: Stores line pointers (for scrollback buffer).

**Files**: `src/line_array.c` + `src/line_array.h`

**Created in Phase 2** when scrollback buffer is needed.

## Use Cases in REPL Terminal

1. **Dynamic zone text** - Use `ik_byte_array_t`
   - Store UTF-8 text being edited
   - Insert/delete bytes at cursor position
   - Grow as user types

2. **Scrollback buffer** - Use `ik_line_array_t` (Phase 2)
   - Store array of line pointers
   - Append new lines as they're submitted
   - Random access for rendering

## Memory Management

- All arrays use talloc for allocation
- Array structure and data buffer both owned by talloc context
- Automatic cleanup when talloc context is freed
- No explicit free function needed (talloc handles it)

## Thread Safety

- **Not thread-safe** - designed for single-threaded use
- REPL terminal runs on single thread, no synchronization needed
- If threading needed in future, add wrapper with locks

## Implementation Guide

### Phase 0 Implementation Order

**Step 1: Add TRY macro to error handling (FIRST)**

Before implementing the array, add ergonomic error propagation to `src/error.h`:

```c
#define TRY(expr) \
    ({ \
        res_t _res = (expr); \
        if (is_err(&_res)) { \
            return _res; \
        } \
        _res.value; \
    })
```

**Usage:**
```c
void *element = TRY(ik_array_get(array, index));  // Propagates error or returns value
```

**Requirements:**
- Add macro to `src/error.h` alongside existing `OK`/`ERR` macros
- Update `tests/unit/error_test.c` to verify TRY macro behavior
- Test that errors propagate correctly with proper context
- Test that successful results extract values correctly
- Ensure 100% coverage of TRY macro paths

**Note:** Uses GNU C statement expressions - supported by GCC/Clang (our target compilers).

**Step 2: Implement generic array**

Once TRY macro is working and tested, proceed with array implementation.

### Files and Build System

**Phase 0 includes ONLY these files:**

Generic array implementation:
- `src/array.c` - Generic `ik_array_t` implementation
- `src/array.h` - Generic `ik_array_t` API

Error handling enhancement (Step 1):
- `src/error.h` - Add TRY macro
- `tests/unit/error_test.c` - Add TRY macro tests

External library wrapper (add to existing):
- `src/wrapper.h` - Add `ik_talloc_realloc_wrapper` declaration (MOCKABLE pattern)
- `src/wrapper.c` - Add `ik_talloc_realloc_wrapper` implementation

**Test files for Phase 0:**
- `tests/unit/error_test.c` - Update with TRY macro tests (Step 1)
- `tests/unit/array_test.c` - Unit tests for generic array (Step 2)

**Makefile integration for Phase 0:**
Add to `Makefile` (see existing patterns):
```make
CLIENT_SOURCES = src/client.c src/error.c src/logger.c src/wrapper.c src/array.c
MODULE_SOURCES = src/error.c src/logger.c src/config.c src/wrapper.c src/protocol.c src/array.c
```

**Future phases will add:**
- Phase 1: `src/byte_array.c` + `src/byte_array.h` + `tests/unit/byte_array_test.c`
- Phase 2: `src/line_array.c` + `src/line_array.h` + `tests/unit/line_array_test.c`

**Dependencies:** Add `ik_talloc_realloc_wrapper` to `src/wrapper.h` and `src/wrapper.c`:

In `src/wrapper.h`, add to talloc wrappers section:
```c
#ifdef NDEBUG
// Release build: inline definitions for zero overhead
MOCKABLE void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}
#else
// Debug/test build: weak symbol declaration
MOCKABLE void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size);
#endif
```

In `src/wrapper.c`, add to talloc wrappers section (inside `#ifndef NDEBUG` block):
```c
MOCKABLE void *ik_talloc_realloc_wrapper(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    return talloc_realloc_size(ctx, ptr, size);
}
```

### Testing Strategy

**TDD approach:**
1. Write failing test first (RED)
2. Implement minimal code to pass (GREEN)
3. Run `make check`, `make lint`, `make coverage`
4. Repeat until 100% coverage

**Testing IO operations (res_t):**
Use `oom_test_*` functions from `tests/test_utils.h` to test allocation failures:
```c
// Test allocation failure in ik_array_create
oom_test_fail_next_alloc();
res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
ck_assert(is_err(&res));
ck_assert_int_eq(error_code(res.err), ERR_OOM);
oom_test_reset();

// Test allocation failure in ik_array_append
oom_test_fail_next_alloc();
res = ik_array_append(array, &value);
ck_assert(is_err(&res));
oom_test_reset();
```

**Testing contract violations (assertions):**
Use Check's `tcase_add_test_raise_signal()` to test assertions fire correctly:
```c
#ifndef NDEBUG
START_TEST(test_array_get_null_array_asserts)
{
    // Will abort() in debug builds
    ik_array_get(NULL, 0);
}
END_TEST

START_TEST(test_array_get_out_of_bounds_asserts)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_array_create(ctx, sizeof(int32_t), 10);
    ik_array_t *array = res.ok;

    // Access beyond bounds - should assert
    ik_array_get(array, 100);

    talloc_free(ctx);
}
END_TEST
#endif

// In suite:
#ifndef NDEBUG
tcase_add_test_raise_signal(tc_core, test_array_get_null_array_asserts, SIGABRT);
tcase_add_test_raise_signal(tc_core, test_array_get_out_of_bounds_asserts, SIGABRT);
#endif
```

**Study existing patterns:**
- `tests/unit/error_test.c` - res_t testing patterns
- `tests/unit/config_test.c` - OOM injection examples
- `tests/integration/oom_integration_test.c` - Comprehensive OOM testing
- `docs/error_handling.md` - Complete testing strategy for all three categories

### Implementation Notes

- **IO operations** (`create`, `append`, `insert`) return `res_t` for OOM handling
- **Contract violations** (NULL pointers, out-of-bounds) use `assert()` - fail fast in debug, compiled out in release
- **Pure queries** (`size`, `capacity`) return values directly with `assert()` for NULL defense
- Uses `memcpy` for element operations (works for any type)
- Growth by doubling prevents frequent reallocations
- Error context obtained via `talloc_parent(array)` for operations on existing arrays
- `increment` parameter must be > 0 (validated at creation time with `res_t` error)
- `capacity` field starts at 0 (lazy allocation - memory allocated on first append/insert)
