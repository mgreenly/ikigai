# Phase 0 Tasks: Foundation

## Task 1: Error Handling Cleanup ✅ COMPLETE

- [x] Remove helper functions (`ik_check_null`, `ik_check_range`)
- [x] Add ~35 missing assertions to all existing functions
- [x] Replace defensive NULL checks with assertions
- [x] Fix `expand_tilde` to return `res_t`

## Task 2: Generic Array Utility

See [docs/array.md](docs/array.md) for complete design.

This task is implemented in **four sequential steps**, each with full TDD and 100% coverage before proceeding to the next step.

### Step 1: Add TRY Macro to Error Handling ✅ COMPLETE

- [x] Add `TRY` macro to `src/error.h` for ergonomic error propagation
- [x] Write tests in `tests/unit/error_test.c` for TRY macro behavior
- [x] Verify errors propagate correctly with proper context
- [x] Verify successful results extract values correctly
- [x] Ensure 100% coverage of TRY macro paths
- [x] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 2: Implement Generic Array ✅ COMPLETE

- [x] Add `ik_talloc_realloc_wrapper` to `src/wrapper.h` and `src/wrapper.c`
- [x] Create `src/array.h` and `src/array.c`
- [x] Implement `ik_array_t` structure (generic, element_size configurable)
- [x] Implement `ik_array_create()` - Create new array (lazy allocation)
- [x] Implement `ik_array_append()` - Add element to end
- [x] Implement `ik_array_insert()` - Insert element at position
- [x] Implement `ik_array_delete()` - Delete element at position
- [x] Implement `ik_array_get()` - Get element at position
- [x] Implement `ik_array_set()` - Set element at position
- [x] Implement `ik_array_clear()` - Remove all elements
- [x] Implement `ik_array_size()` - Get current count
- [x] Implement `ik_array_capacity()` - Get allocated capacity
- [x] Implement growth by doubling capacity (first allocation uses increment)
- [x] Write tests in `tests/unit/array_test.c` for all operations
- [x] Test OOM scenarios using `oom_test_*` functions
- [x] Test contract violations using `tcase_add_test_raise_signal()`
- [x] Test boundary conditions (empty array, single element, etc.)
- [x] Test capacity growth (verify increment then doubling)
- [x] Test security scenarios (use-after-free, underflow, pathological sequences)
- [x] Update Makefile to include array.c in build
- [x] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 3: Implement Byte Array Wrapper ✅ COMPLETE

- [x] Create `src/byte_array.h` and `src/byte_array.c`
- [x] Define `ik_byte_array_t` as typedef of `ik_array_t`
- [x] Implement `ik_byte_array_create()` wrapper
- [x] Implement `ik_byte_array_append()` wrapper
- [x] Implement `ik_byte_array_insert()` wrapper
- [x] Implement `ik_byte_array_delete()` wrapper
- [x] Implement `ik_byte_array_get()` wrapper
- [x] Implement `ik_byte_array_set()` wrapper
- [x] Implement `ik_byte_array_clear()` wrapper
- [x] Implement `ik_byte_array_size()` wrapper
- [x] Implement `ik_byte_array_capacity()` wrapper
- [x] Write tests in `tests/unit/byte_array_test.c`
- [x] Update Makefile to include byte_array.c in build
- [x] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 4: Implement Line Array Wrapper

- [ ] Create `src/line_array.h` and `src/line_array.c`
- [ ] Define `ik_line_array_t` as typedef of `ik_array_t`
- [ ] Implement `ik_line_array_create()` wrapper
- [ ] Implement `ik_line_array_append()` wrapper
- [ ] Implement `ik_line_array_insert()` wrapper
- [ ] Implement `ik_line_array_delete()` wrapper
- [ ] Implement `ik_line_array_get()` wrapper
- [ ] Implement `ik_line_array_set()` wrapper
- [ ] Implement `ik_line_array_clear()` wrapper
- [ ] Implement `ik_line_array_size()` wrapper
- [ ] Implement `ik_line_array_capacity()` wrapper
- [ ] Write tests in `tests/unit/line_array_test.c`
- [ ] Update Makefile to include line_array.c in build
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Final Quality Gates

- [ ] All steps complete with 100% coverage
- [ ] `make check` passes (all tests)
- [ ] `make lint` passes (complexity under threshold)
- [ ] `make coverage` shows 100% coverage (Lines, Functions, Branches)
- [ ] Run `make fmt` before committing

---

**Development Approach**: Strict TDD red/green cycle
1. Red: Write failing test first (verify it fails)
2. Green: Write minimal code to pass the test
3. Verify: Run `make check`, `make lint`, `make coverage`

**Zero Technical Debt**: Fix any deficiencies immediately as discovered.
