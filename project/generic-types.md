# Generic Expandable Array (ik_array)

## Overview

A generic, type-safe expandable array implementation for ikigai. Provides a reusable foundation for dynamic collections with zero-overhead typed wrappers.

This module is complete and fully tested with 100% coverage (lines, functions, branches).

## Design Pattern

**Two-layer approach:**

1. **Generic implementation** (`ik_array_t`)
   - Single implementation handles all element types
   - Element size is configurable at creation
   - All logic (growth, reallocation, operations) in one place
   - Uses `void*` for generic element handling

2. **Typed wrappers** (`ik_byte_array_t`, `ik_line_array_t`)
   - Type-safe interfaces for specific element types
   - Wrapper functions delegate to generic implementation
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

**Follows the three-category error handling strategy from `project/error_handling.md`:**

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

**Error codes used:**
- `ERR_INVALID_ARG` - Invalid parameter at creation (element_size=0, increment=0)

**OOM handling:** Memory allocation failures call `PANIC("Out of memory")` which immediately terminates the process. OOM is not a recoverable error.

See `project/error_handling.md` for complete error handling philosophy.

### Core Operations

**Lifecycle (IO - Returns res_t):**
- `res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment)`
  - Creates new array with specified element size and initial capacity
  - `element_size` must be > 0 (returns `ERR(ctx, INVALID_ARG, "element_size must be > 0")` if 0)
  - `increment` must be > 0 (returns `ERR(ctx, INVALID_ARG, "increment must be > 0")` if 0)
  - **No memory allocated for data buffer** - allocation deferred until first append/insert
  - Returns `OK(array)` or `ERR(ctx, INVALID_ARG, ...)`
  - Allocation failures cause `PANIC("Out of memory")` - not returned as errors
  - Array structure is owned by talloc context (freed when context is freed)

**Modification (IO - Returns res_t):**
- `res_t ik_array_append(ik_array_t *array, const void *element)`
  - Appends element to end of array
  - Grows array if needed (doubles capacity)
  - Validates: `assert(array != NULL)`, `assert(element != NULL)`
  - Returns `OK(NULL)` on success
  - Allocation failures cause `PANIC("Out of memory")` - not returned as errors

- `res_t ik_array_insert(ik_array_t *array, size_t index, const void *element)`
  - Inserts element at specified index
  - Shifts existing elements right
  - Validates: `assert(array != NULL)`, `assert(element != NULL)`, `assert(index <= array->size)`
  - Grows array if needed
  - Returns `OK(NULL)` on success
  - Allocation failures cause `PANIC("Out of memory")` - not returned as errors

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
- Uses `talloc_realloc_(ctx, NULL, element_size * increment)`

**Subsequent growth** (when `size == capacity` and element needs to be added):
- Double current capacity: `new_capacity = capacity * 2`
- Example: increment=10 → capacity goes 0 → 10 → 20 → 40 → 80 → ...
- Uses `talloc_realloc_(ctx, data, element_size * new_capacity)`
- Maintains existing elements during growth
- **No overflow protection** - assumes realistic array sizes won't overflow size_t

**Validation at creation**:
- `element_size` must be > 0 (returns `ERR(ctx, INVALID_ARG, "element_size must be > 0")` if 0)
- `increment` must be > 0 (returns `ERR(ctx, INVALID_ARG, "increment must be > 0")` if 0)

## Typed Wrappers

The generic `ik_array_t` is wrapped by typed modules that provide type-safe, ergonomic APIs.

**Implemented wrappers:**
- `ik_byte_array_t` - Stores bytes (uint8_t) for UTF-8 text buffers
- `ik_line_array_t` - Stores line pointers (char*) for scrollback buffers

### Byte Array (ik_byte_array_t)

**Purpose**: Stores bytes (uint8_t) for UTF-8 text in dynamic input zone.

**Files**: `src/byte_array.c` + `src/byte_array.h`

**Design**:
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

### Line Array (ik_line_array_t)

**Purpose**: Stores line pointers (char*) for scrollback buffer.

**Files**: `src/line_array.c` + `src/line_array.h`

**Design**: Similar pattern to byte array, wrapping generic array for char* elements.

## Use Cases in REPL Terminal

1. **Dynamic zone text** - Uses `ik_byte_array_t`
   - Stores UTF-8 text being edited
   - Supports insert/delete bytes at cursor position
   - Grows as user types

2. **Scrollback buffer** - Uses `ik_line_array_t`
   - Stores array of line pointers
   - Appends new lines as they're submitted
   - Provides random access for rendering

## Memory Management

- All arrays use talloc for allocation
- Array structure and data buffer both owned by talloc context
- Automatic cleanup when talloc context is freed
- No explicit free function needed (talloc handles it)

## Thread Safety

- **Not thread-safe** - designed for single-threaded use
- REPL terminal runs on single thread, no synchronization needed
- If threading needed in future, add wrapper with locks

## Implementation Notes

The array module was implemented in four sequential steps, each with full TDD and 100% coverage:

1. **TRY macro** - Added ergonomic error propagation to `src/error.h`
2. **Generic array** - Implemented `ik_array_t` with full test coverage
3. **Byte array wrapper** - Created type-safe wrapper for uint8_t elements
4. **Line array wrapper** - Created type-safe wrapper for char* elements

All error handling follows the three-category strategy from `project/error_handling.md`.

### Files

**Implementation files:**
- `src/array.{c,h}` - Generic `ik_array_t` implementation and API
- `src/byte_array.{c,h}` - `ik_byte_array_t` typed wrapper
- `src/line_array.{c,h}` - `ik_line_array_t` typed wrapper
- `src/wrapper.{c,h}` - Includes `talloc_realloc_` (MOCKABLE pattern)

**Test files:**
- `tests/unit/array_test.c` - Unit tests for generic array
- `tests/unit/byte_array_test.c` - Unit tests for byte array
- `tests/unit/line_array_test.c` - Unit tests for line array

### Testing Strategy

The module achieves 100% coverage (lines, functions, branches) through comprehensive testing:

**Testing IO operations (res_t):**
OOM testing has been removed since allocation failures now cause PANIC (abort). Only validation errors (like invalid arguments) can be tested:
```c
// Test invalid argument detection
res_t res = ik_array_create(ctx, 0, 10);  // element_size=0
ck_assert(is_err(&res));
ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

res = ik_array_create(ctx, sizeof(int32_t), 0);  // increment=0
ck_assert(is_err(&res));
ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
```

OOM branches are excluded from coverage using `// LCOV_EXCL_BR_LINE` markers.

**Testing contract violations (assertions):**
Uses Check's `tcase_add_test_raise_signal()` to verify assertions fire correctly:
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

**Reference implementations:**
- `tests/unit/error_test.c` - res_t testing patterns
- See `project/error_handling.md` for complete testing strategy

**Note:** OOM injection testing infrastructure has been removed since allocation failures now cause PANIC.

### Design Decisions

- **IO operations** (`create`, `append`, `insert`) return `res_t` for validation errors
- **OOM handling** - Memory allocation failures call `PANIC("Out of memory")` instead of returning errors
- **Contract violations** (NULL pointers, out-of-bounds) use `assert()` - fail fast in debug, optimized out in release
- **Pure queries** (`size`, `capacity`) return values directly with `assert()` for NULL defense
- Uses `memcpy` for element operations - works for any type
- Growth by doubling prevents frequent reallocations while minimizing memory waste
- Error context obtained via `talloc_parent(array)` for operations on existing arrays
- `increment` parameter must be > 0 - validated at creation time
- Lazy allocation - `capacity` starts at 0, memory allocated on first append/insert
- No overflow protection - assumes realistic array sizes won't overflow size_t
