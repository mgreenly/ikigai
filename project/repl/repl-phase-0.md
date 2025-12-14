# REPL Terminal - Phase 0: Foundation

[← Back to REPL Terminal Overview](README.md)

**Goal**: Clean up existing code and build reusable infrastructure before REPL development.

Before building terminal code, the existing error handling was cleaned up to properly follow the 3 modes of operation philosophy, and a generic expandable array utility was created for use throughout the REPL implementation.

## Task 1: Error Handling Cleanup

The existing code was cleaned up to properly follow the 3 modes of operation (IO operations, contract violations, pure operations).

**Changes made:**
1. Removed helper functions (`ik_check_null`, `ik_check_range`) - now using asserts for contract violations
2. Added ~35 missing assertions to all existing functions (config.c, protocol.c, error.h)
3. Replaced defensive NULL checks with assertions
4. Fixed `expand_tilde` to return `res_t` (IO operation doing allocation)

**Impact:** The codebase now properly follows the 3 modes of operation philosophy. All tests pass with proper separation of concerns between IO errors, contract violations, and pure operations.

## Task 2: Generic Array Utility

**See [project/array.md](../array.md) for complete design.**

A generic expandable array utility was built for use throughout the REPL implementation.

**Components:**
1. Generic `ik_array_t` implementation (element_size configurable) - src/array.h, src/array.c
2. Common textbook operations (append, insert, delete, get, set, clear)
3. Talloc-based memory management with lazy allocation
4. Growth by doubling capacity (after initial increment-sized allocation)
5. Full test coverage via TDD

**Typed wrappers created:**
- `ik_byte_array_t` - For dynamic zone text (UTF-8 bytes) - src/byte_array.h, src/byte_array.c
- `ik_line_array_t` - For scrollback buffer (line pointers) - src/line_array.h, src/line_array.c

**Impact:** The generic array utility provides type-safe expandable storage used throughout the REPL implementation. All operations properly follow the 3 modes of operation philosophy with 100% test coverage.
