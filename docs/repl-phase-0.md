# REPL Terminal - Phase 0: Foundation

[← Back to REPL Terminal Overview](repl-terminal.md)

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

## Task 2: Generic Array Utility ← CURRENT

**See [docs/array.md](array.md) for complete design.**

Build the generic expandable array utility that will be used throughout the REPL implementation.

**Status:**
- Step 1 (TRY macro) ✅ COMPLETE
- Step 2 (generic array) ← IN PROGRESS

**Key components:**
1. Generic `ik_array_t` implementation (element_size configurable)
2. Common textbook operations (append, insert, delete, get, set)
3. Talloc-based memory management
4. Growth by doubling capacity
5. Full test coverage via TDD

**Typed wrappers to create:**
- `ik_byte_array_t` - For dynamic zone text (UTF-8 bytes)
- `ik_line_array_t` - For scrollback buffer (line pointers)

**Development approach**: Strict TDD red/green cycle with 100% coverage.

**Why second?** Both dynamic zone text and scrollback buffer need expandable storage. Building this utility on a clean foundation ensures consistency.
