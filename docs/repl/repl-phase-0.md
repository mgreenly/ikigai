# REPL Terminal - Phase 0: Foundation

[← Back to REPL Terminal Overview](README.md)

**Goal**: Clean up existing code and build reusable expandable array before REPL work.

Before building any terminal code, we need to clean up the existing error handling implementation and then build a generic expandable array utility.

## Task 1: Error Handling Cleanup ✅ COMPLETE

Clean up existing code to properly follow the 3 modes of operation (IO operations, contract violations, pure operations) before building new features.

**Completed changes:**
1. ✅ Removed helper functions (`ik_check_null`, `ik_check_range`) - now using asserts for contract violations
2. ✅ Added ~35 missing assertions to all existing functions (config.c, protocol.c, error.h)
3. ✅ Replaced defensive NULL checks with assertions
4. ✅ Fixed `expand_tilde` to return `res_t` (IO operation doing allocation)

**Impact:** The codebase now properly follows the 3 modes of operation philosophy. All tests pass with proper separation of concerns between IO errors, contract violations, and pure operations.

## Task 2: Generic Array Utility ✅ COMPLETE

**See [docs/array.md](array.md) for complete design.**

Build the generic expandable array utility that will be used throughout the REPL implementation.

**Status:**
- Step 1 (TRY macro) ✅ COMPLETE
- Step 2 (generic array) ✅ COMPLETE

**Completed components:**
1. ✅ Generic `ik_array_t` implementation (element_size configurable) - src/array.h, src/array.c
2. ✅ Common textbook operations (append, insert, delete, get, set, clear)
3. ✅ Talloc-based memory management with lazy allocation
4. ✅ Growth by doubling capacity (after initial increment-sized allocation)
5. ✅ Full test coverage via TDD

**Typed wrappers created:**
- ✅ `ik_byte_array_t` - For dynamic zone text (UTF-8 bytes) - src/byte_array.h, src/byte_array.c
- ✅ `ik_line_array_t` - For scrollback buffer (line pointers) - src/line_array.h, src/line_array.c

**Impact:** The generic array utility provides type-safe expandable storage used throughout the REPL implementation. All operations properly follow the 3 modes of operation philosophy with 100% test coverage.
