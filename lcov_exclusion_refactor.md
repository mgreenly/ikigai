# LCOV Exclusion Refactoring - Implementation Plan

## Introduction

This document contains the implementation plan for refactoring LCOV exclusions based on the decisions made in `lcov_exclusion_review.md`.

**Goal**: Reduce exclusion count from ~120 to ~65 markers while maintaining 100% coverage through:
1. Removing dead code
2. Refactoring multi-line exclusions to single-line format
3. Adding test coverage for testable cases
4. Accepting legitimate exclusions with proper justification

**Working Process**:
- Work through ONE category at a time
- Complete all tasks in a category before moving to next
- Review and verify after each category
- Run `make coverage` to confirm 100% coverage maintained

## Context Documents

- **lcov_exclusion_review.md** - Complete analysis and decisions for all 120 exclusions
- **docs/lcov_exclusion_strategy.md** - Strategy and rules for exclusions
- **AGENTS.md** - TDD principles and quality standards
- **docs/error_handling.md** - Error handling philosophy (Result types, PANIC)

## Coverage Verification Commands

```bash
# Run all quality checks
make fmt check lint coverage check-dynamic

# Find uncovered lines
grep "^DA:" coverage/coverage.info | grep ",0$"

# Find uncovered branches
grep "^BRDA:" coverage/coverage.info | grep ",0$"

# View summary
cat coverage/summary.txt
```

---

## Category 1: Remove Dead Code

**Status**: [X] Complete

### Task 1.1: Remove ik_log_fatal()
- [X] Remove `ik_log_fatal()` declaration from `src/logger.h`
- [X] Remove `ik_log_fatal()` implementation from `src/logger.c` (lines 115-134)
- [X] Remove LCOV_EXCL_START/STOP markers around it
- [X] Update/remove test references in `tests/unit/logger/logger_test.c`
- [X] Update/remove test references in `tests/integration/logger_integration_test.c`
- [X] Run `make check` to verify tests still pass
- [X] Run `make coverage` to verify coverage maintained

**Verification**:
- [X] No references to `ik_log_fatal` remain in codebase
- [X] All tests pass
- [X] Coverage remains at 100%

---

## Category 2: Refactor OOM Checks to Single-Line PANIC

**Status**: [X] Complete

**Pattern**: Convert from 3-line (2 exclusions) to 1-line (1 exclusion):
```c
// FROM:
result_t res = allocate_something();
if (is_err(&res)) {                // LCOV_EXCL_LINE
    return res;                     // LCOV_EXCL_LINE
}

// TO:
result_t res = allocate_something();
if (is_err(&res)) PANIC("allocation failed"); // LCOV_EXCL_LINE
```

### Task 2.1: src/array.c (2 locations)
- [X] Line 78-79: `is_err(&res)` after `grow_array()` in `ik_array_append`
- [X] Line 101-102: `is_err(&res)` after `grow_array()` in `ik_array_insert`
- [X] Run tests: `make build/tests/unit/array/basic_test && ./build/tests/unit/array/basic_test`
- [X] Verify coverage: `make coverage`

### Task 2.2: src/format.c (5 locations)
- [X] Line 26-28: `is_err(&res)` after `ik_byte_array_create` in `ik_format_buffer_create`
- [X] Line 65-67: `is_err(&res)` in `ik_format_appendf` loop
- [X] Line 87-88: `is_err(&res)` in `ik_format_append` loop
- [X] Line 106-107: `is_err(&res)` in `ik_format_indent` loop
- [X] Line 129-130: `is_err(&res)` in `ik_format_get_string`
- [X] Run tests: `make build/tests/unit/format/format_basic_test && ./build/tests/unit/format/format_basic_test`
- [X] Verify coverage: `make coverage`

### Task 2.3: src/workspace.c (4 locations)
- [X] Line 22-24: `is_err(&res)` after `ik_byte_array_create` in `ik_workspace_create`
- [X] Line 29-31: `is_err(&res)` after `ik_cursor_create` in `ik_workspace_create`
- [X] Line 119-120: `is_err(&res)` in `ik_workspace_insert_codepoint` loop
- [X] Line 146-147: `is_err(&res)` in `ik_workspace_insert_newline`
- [X] Run tests: `make build/tests/unit/workspace/insert_test && ./build/tests/unit/workspace/insert_test`
- [X] Verify coverage: `make coverage`

### Task 2.4: src/repl.c (5 locations - note: plan said 6 but only 5 OOM checks found)
- [X] Line 40-42: `is_err(&result)` after `ik_workspace_create`
- [X] Line 47-49: `is_err(&result)` after `ik_input_parser_create`
- [X] Line 54-56: `is_err(&result)` after `ik_scrollback_create`
- [X] Line 287-288: `is_err(&result)` after `ik_format_buffer_create`
- [X] Line 334-335: `is_err(&result)` after `ik_repl_handle_slash_command`
- [X] Run tests: All repl tests pass via `make check`
- [X] Verify coverage: `make coverage`

**Category 2 Verification**:
- [X] All multi-line OOM checks converted to single-line PANIC
- [X] All tests pass: `make check`
- [X] Coverage remains 100%: `make coverage`
- [X] Exclusion markers reduced: ~20-24 LCOV_EXCL_LINE markers removed

---

## Category 3: Add Test Coverage for Environmental/IO Errors

**Status**: [X] Complete

### Task 3.1: Add System Call Wrappers

**File**: `src/wrapper.h` and `src/wrapper.c`

- [X] Add `ik_stat_wrapper(const char *pathname, struct stat *statbuf)`
- [X] Add `ik_mkdir_wrapper(const char *pathname, mode_t mode)`
- [X] Ensure wrappers are `MOCKABLE` and only compiled in debug builds (`#ifndef NDEBUG`)
- [X] Update wrapper.c with implementations
- [X] Run `make check` to verify no breakage

### Task 3.2: Terminal Initialization Failures (src/repl.c)

**Line 22-24**: Mock `ik_term_init` failure

- [X] Write test: Mock `ik_open_wrapper("/dev/tty")` to return -1
- [X] Verify error is properly handled and returned
- [X] Remove LCOV_EXCL_LINE markers
- [X] Test file: Created new file `tests/unit/repl/repl_init_test.c`

**Line 33-35**: Mock `ik_render_create` failure

- [X] Write test: Mock `ik_ioctl_wrapper` to return invalid terminal dimensions
- [X] Verify error is properly handled
- [X] Remove LCOV_EXCL_LINE markers

### Task 3.3: Filesystem Operations (src/config.c)

**Line 53-58**: stat/mkdir failures

- [X] Write test: Mock `ik_stat_wrapper` to return -1 (ENOENT)
- [X] Write test: Mock `ik_mkdir_wrapper` to return -1 (EACCES)
- [X] Verify proper error handling
- [X] Remove LCOV_EXCL_LINE markers
- [X] Test file: Created new file `tests/unit/config/filesystem_test.c`

**Line 70-75**: json_dump_file failure

- [X] Deferred - json_dump_file cannot be easily wrapped without significant refactoring
- [X] Will revisit during JSON library migration (yyjson)
- [X] Markers remain for now (documented as deferred)

**Line 95-99**: Config creation errors

- [X] Verified covered by Task 3.3 tests
- [X] Markers remain (create errors tested above, path covered in other tests)

### Task 3.4: Terminal Write Failures (src/render.c)

**Line 316-317**: write() failure (bytes_written < 0)

- [X] Write test: Mock `ik_write_wrapper` to return -1
- [X] Verify error is properly handled
- [X] Remove LCOV_EXCL_LINE markers
- [X] Test file: Modified `tests/unit/render/render_scrollback_test.c`

**Category 3 Verification**:
- [X] All wrappers added and working
- [X] All IO error paths tested (except json_dump_file - deferred)
- [X] All tests pass: `make check`
- [X] Coverage remains 100%: `make coverage`
- [X] 6 exclusion markers removed (299 → 293)

---

## Category 4: Verify Invariant Checks - Single-Line Format

**Status**: [X] Complete

**Task**: Verify all defensive invariant checks are single-line format

### Task 4.1: src/workspace.c
- [X] Line 227: Refactored to single-line format (was 227-228)
- [X] Line 245: Refactored to single-line format (was 248-251 EXCL_START/STOP)
- [X] Line 301: Refactored to single-line format (was 308-309)
- [X] Other locations already in correct single-line format

### Task 4.2: src/workspace_layout.c
- [X] Line 21: Refactored to single-line format (was 21-22)
- [X] Lines 33-34: Refactored to single-line format with BR_LINE + LINE (was 33-40 EXCL_START/STOP)
- [X] Other locations already in correct single-line format

### Task 4.3: src/workspace_pp.c
- [X] All locations already in correct single-line format (no changes needed)

**Category 4 Verification**:
- [X] All checks are single-line with LCOV_EXCL_LINE or LCOV_EXCL_BR_LINE
- [X] All have clear comments explaining the invariant
- [X] Tests pass: `make check`
- [X] Coverage remains 100%: `make coverage`
- [X] Exclusion markers reduced: 293 → 288 (5 markers removed)

---

## Category 5: Verify UTF-8 Validation

**Status**: [X] Complete

**Task**: Verify that UTF-8 validation assumptions are correct

### Task 5.1: Verify 4-byte UTF-8 Support

**Files**: `src/workspace_multiline.c`, `src/input.c`

- [X] Check existing tests for 4-byte UTF-8 (emojis: 👍, 🎉, etc.)
- [X] Verify tests exist in input parser tests
- [X] Verify tests exist in workspace tests
- [X] Verify exclusions are for invalid sequences, not valid 4-byte UTF-8

**Findings**:
- Comprehensive 4-byte UTF-8 test coverage confirmed:
  - `tests/unit/input/utf8_test.c:test_input_parse_utf8_4byte` - tests parsing 🎉 (U+1F389)
  - Multiple workspace tests use 4-byte emoji characters
  - Cursor movement, insertion, deletion all tested with emoji
- Coverage data confirms execution:
  - `src/input.c:70` (len == 4 decode path): executed 5 times
  - `src/workspace_multiline.c:85-86` (4-byte in count_graphemes): executed 2 times
  - `src/workspace_multiline.c:133-134` (4-byte in grapheme_to_byte_offset): executed 1 time
- LCOV_EXCL_BR_LINE markers are LEGITIMATE:
  - They exclude the **false branch** of the condition, not the line itself
  - The false branch leads to error/abort paths that are unreachable with valid input
  - The true branch (4-byte UTF-8 handling) is fully covered

### Task 5.2: Verify Input Validation

**Files**: `src/workspace.c`, `src/workspace_layout.c`

- [X] Verify input parser rejects invalid UTF-8 at entry points
- [X] Check for validation in `ik_input_parse_byte`
- [X] Verify workspace only receives valid UTF-8
- [X] Confirm defensive checks in layout are truly unreachable

**Findings**:
- Input validation chain confirmed:
  1. **Input Parser** (`src/input.c:decode_utf8_sequence`):
     - Validates UTF-8 byte sequences
     - Rejects overlong encodings
     - Rejects surrogates (0xD800-0xDFFF)
     - Rejects out-of-range codepoints (> 0x10FFFF)
     - Returns U+FFFD for invalid sequences
  2. **Workspace Insert** (`src/workspace.c:ik_workspace_insert_codepoint`):
     - Validates codepoint range via `encode_utf8()`
     - Returns error for invalid codepoints (> 0x10FFFF)
     - Only inserts valid UTF-8 bytes
  3. **Workspace Newline** (`src/workspace.c:ik_workspace_insert_newline`):
     - Inserts ASCII '\n' which is always valid UTF-8
- Text modification analysis:
  - Only insertion points: `ik_workspace_insert_codepoint()` and `ik_workspace_insert_newline()`
  - All delete operations only remove existing bytes
  - No direct byte array access from external code
- **Conclusion**: Precondition in `workspace_multiline.c` is VALID - workspace text always contains valid UTF-8

### Task 5.3: Verify utf8proc Library Guarantees

**File**: `src/workspace_layout.c` line 38

- [X] Verify utf8proc documentation confirms `char_width >= 0` for all codepoints
- [X] Confirm defensive check is based on library guarantee
- [X] No code changes needed

**Findings**:
- utf8proc documentation confirms: **`utf8proc_charwidth()` NEVER returns negative values**
- Unlike POSIX `wcwidth()` which returns -1 for non-printable characters
- `utf8proc_charwidth()` returns 0 for non-printable/zero-width characters instead
- Return values: 0 (non-printable/zero-width), 1 (normal), 2 (wide/CJK)
- LCOV_EXCL_BR_LINE on line 38 is LEGITIMATE - the false branch is impossible

**Category 5 Verification**:
- [X] 4-byte UTF-8 test coverage confirmed (comprehensive)
- [X] Input validation confirmed at entry points (complete validation chain)
- [X] Library guarantees documented (utf8proc_charwidth always >= 0)
- [X] All checks remain single-line format
- [X] No exclusion markers removed (all are legitimate defensive checks)
- [X] No code changes needed

### Task 5.4: Error Handling Consistency Refactor

**Status**: [X] Complete

During Category 5 verification, discovered error handling inconsistency where invariant checks used `assert()` instead of `PANIC()`.

**Issue**: Three functions violated error handling philosophy from `docs/error_handling.md`:
- **Assert**: For preconditions (caller's responsibility), compiles out in release
- **PANIC**: For invariants (our responsibility), always present

**Files Changed**:

1. **src/input.c - decode_utf8_sequence (lines 50-85)**:
   - BEFORE: `assert(len >= 1 && len <= 4)` + if/else chain
   - AFTER: Converted to switch statement with PANIC default
   - Markers: 3 → 3 (no change, but better structure)
   - Reasoning: Invalid length is an invariant violation, not precondition

2. **src/workspace_multiline.c - count_graphemes (lines 79-89)**:
   - BEFORE: `fprintf() + abort()` for invalid UTF-8
   - AFTER: Single `PANIC("invalid UTF-8 in workspace text")`
   - Markers: 3 → 2 (saved 1 marker)
   - Removed includes: stdio.h, stdlib.h
   - Added include: panic.h

3. **src/workspace_multiline.c - grapheme_to_byte_offset (lines 125-135)**:
   - BEFORE: `fprintf() + abort()` for invalid UTF-8
   - AFTER: Single `PANIC("invalid UTF-8 in workspace text")`
   - Markers: 3 → 2 (saved 1 marker)

**Results**:
- ✅ Exclusion count: 288 → 286 (saved 2 markers)
- ✅ Coverage: 100.0% maintained (lines, functions, branches)
- ✅ Philosophy: Consistent with docs/error_handling.md
- ✅ Clarity: No confusing "unreachable else after assert"
- ✅ Safety: Works correctly in both debug AND release builds

---

## Category 6: Verify Switch Statement Exhaustiveness

**Status**: [X] Complete

### Task 6.1: src/error.h (lines 137-150)

- [X] Read switch statement on `err_code_t`
- [X] Verify all enum values have cases
- [X] Verify default case is single-line PANIC or similar
- [X] Document which enum values are handled
- [X] No code changes needed if exhaustive

**Findings**:
- Enum `err_code_t` has 5 values: OK, ERR_INVALID_ARG, ERR_OUT_OF_RANGE, ERR_IO, ERR_PARSE
- Switch covers all 5 values with explicit cases
- Default case: single-line PANIC "Invalid error code" (lines 148-149)
- **EXHAUSTIVE** - handles corrupted enum values

### Task 6.2: src/repl.c (lines 303-363)

- [X] Read switch statement on `ik_input_action_type_t`
- [X] Verify all enum values have cases
- [X] Verify default case is single-line PANIC (lines 361-362)
- [X] Document which enum values are handled
- [X] No code changes needed if exhaustive

**Findings**:
- Enum `ik_input_action_type_t` has 15 values (IK_INPUT_CHAR through IK_INPUT_UNKNOWN)
- Switch covers all 15 values with explicit cases:
  - IK_INPUT_CHAR, IK_INPUT_NEWLINE, IK_INPUT_BACKSPACE, IK_INPUT_DELETE
  - IK_INPUT_ARROW_LEFT, IK_INPUT_ARROW_RIGHT, IK_INPUT_ARROW_UP, IK_INPUT_ARROW_DOWN
  - IK_INPUT_CTRL_A, IK_INPUT_CTRL_C, IK_INPUT_CTRL_E, IK_INPUT_CTRL_K
  - IK_INPUT_CTRL_U, IK_INPUT_CTRL_W, IK_INPUT_UNKNOWN
- Default case: single-line PANIC "Invalid input action type" (lines 361-362)
- **EXHAUSTIVE** - handles corrupted enum values

**Category 6 Verification**:
- [X] Both switches confirmed exhaustive
- [X] Default cases are single-line format
- [X] Default cases handle corrupted enum values (memory corruption scenarios)
- [X] No exclusion markers removed (verification-only category)
- [X] No code changes needed

---

## Category 7: Render/IO Defensive Checks - REFACTORED to Void Return

**Status**: [X] Complete (via void refactor, not invariant format)

### Task 7.1: Refactor OOM Checks (covered in Category 2)
- [X] Line 287-288: Already in Category 2, Task 2.4 (Complete)
- [X] Line 334-335: Already in Category 2, Task 2.4 (Complete)

**Note**: These OOM checks were already refactored to single-line PANIC format in Category 2.

### Task 7.2: Verify Invariant Checks - Single-Line Format

**src/render.c**
- [X] Line 227: Refactored to single-line format `if (is_err(&result)) return result; /* LCOV_EXCL_LINE */`
- [X] Line 249: Refactored to single-line format `if (is_err(&result)) return result; /* LCOV_EXCL_LINE */`
- [X] Line 285: Refactored to single-line format with cleanup `if (is_err(&result)) { talloc_free(framebuffer); return result; } /* LCOV_EXCL_LINE */`

**src/repl.c**
- [X] Line 100: Refactored to single-line format (defensive check for input validation)
- [X] Line 124: Refactored to single-line format (workspace layout)
- [X] Line 128: Refactored to single-line format (scrollback layout)
- [X] Line 143-144: Already in correct PANIC format (workspace height invariant)
- [X] Line 181-182: Already in correct PANIC format (logical line lookup)
- [X] Line 204: Refactored to single-line format (viewport calculation)
- [X] Line 233: Refactored to single-line format (scrollback rendering)

### Task 7.3: Investigate Input Validation

**src/repl.c Line 100**: Can codepoint validation fail?

- [X] Read `ik_workspace_insert_codepoint` implementation
- [X] Check if `encode_utf8` can return 0 (invalid codepoint)
- [X] Check if input parser validates codepoint range (0-0x10FFFF)
- [X] Parser validates: ACCEPT as invariant, verified single-line format
- [X] No test needed - parser guarantees valid codepoints
- [X] Documented findings below

**Findings**:
- Input parser (`src/input.c:decode_utf8_sequence`) validates all UTF-8 and returns:
  - Valid codepoints (0-0x10FFFF, excluding surrogates)
  - `0xFFFD` (replacement character) for invalid sequences
- Workspace (`src/workspace.c:encode_utf8`) accepts codepoints 0-0x10FFFF
- **Conclusion**: Input parser ALWAYS produces valid codepoints, so `encode_utf8` will never return 0 for parser output. The check on repl.c:100 is a **legitimate defensive invariant** and the LCOV_EXCL_LINE marker is correct.

**Category 7 DISCOVERY & REFACTOR**:

During investigation, discovered that error checks for `ik_workspace_ensure_layout` and `ik_scrollback_ensure_layout` were **not legitimate exclusions**:

1. **Initial approach was wrong**: Tried to exclude `if (is_err(&result)) return result;` as invariants
2. **User challenged this**: Correctly pointed out these are error returns, not PANIC invariants
3. **Root cause analysis**: Both functions:
   - Only do arithmetic (no allocations, no I/O)
   - Always return OK()
   - **Can never actually fail**

**Solution: Change return type to `void`**

Refactored both functions from `res_t` to `void`:
- `void ik_workspace_ensure_layout(ik_workspace_t *workspace, int32_t terminal_width)`
- `void ik_scrollback_ensure_layout(ik_scrollback_t *scrollback, int32_t terminal_width)`

**Impact**:
- Removed all fake error checks (5 call sites in production code, ~10 in tests)
- **Eliminated 3 LCOV exclusion markers** (down from 277 → 274)
- Made code clearer - type system now tells the truth
- No performance impact

**Remaining exclusions from Category 7**:
- [X] repl.c:100 - Valid defensive invariant (input validation)
- [X] repl.c:143-144 - Valid PANIC (workspace height invariant)
- [X] repl.c:181-182 - Valid PANIC (logical line lookup)
- [X] repl.c:204 - Valid defensive invariant (viewport calculation)
- [X] repl.c:233 - Valid defensive invariant (scrollback rendering)
- [X] render.c:248, 284 - Valid defensive invariants (line text retrieval)

**Category 7 Verification**:
- [X] Type system refactor completed
- [X] All tests pass
- [X] Coverage remains 100.0%
- [X] Exclusion count: 277 → 274 (saved 3 markers via void refactor)

---

## Category 8: Add Test Coverage for Edge Cases

**Status**: [ ] Not Started

### Task 8.1: Empty/Zero Cases

**src/workspace_cursor.c Line 108**: grapheme_count == 0

- [ ] Write test: Empty workspace, verify grapheme_count == 0 case
- [ ] Remove LCOV_EXCL_LINE marker
- [ ] Test file: `tests/unit/workspace/workspace_cursor_test.c`

**src/workspace_layout.c Line 94**: Zero-width characters

- [ ] Write test: Input with zero-width joiners (U+200D)
- [ ] Write test: Input with combining characters
- [ ] Verify display_width == 0 case is covered
- [ ] Remove LCOV_EXCL_LINE marker
- [ ] Test file: `tests/unit/workspace/workspace_layout_test.c`

**src/workspace_layout.c Line 101**: Physical lines edge case

- [ ] Investigate what condition triggers this else branch
- [ ] Write test to cover the case
- [ ] Remove LCOV_EXCL_LINE marker

**src/format.c Line 144**: Null-termination check

- [ ] Write test: Buffer already null-terminated
- [ ] Write test: Buffer not null-terminated
- [ ] Verify both branches covered
- [ ] Remove LCOV_EXCL_LINE marker
- [ ] Test file: `tests/unit/format/format_test.c`

### Task 8.2: 4-byte UTF-8 Characters (CRITICAL)

**src/input.c Line 70**: len == 4

- [ ] Write test: Input emoji 👍 (U+1F44D, 4-byte UTF-8)
- [ ] Write test: Input emoji 🎉 (U+1F389, 4-byte UTF-8)
- [ ] Write test: Other 4-byte characters
- [ ] Verify decode path is covered
- [ ] Remove LCOV_EXCL_LINE marker
- [ ] Test file: `tests/unit/input/input_parser_test.c`

**Note**: This is critical for verifying 4-byte UTF-8 support works correctly

### Task 8.3: Escape Sequence Variations

**src/input.c Line 213**: Uppercase escape codes

- [ ] Research if terminals send uppercase escape sequences
- [ ] If valid: Write test with uppercase escape code
- [ ] If invalid: Document why and keep exclusion
- [ ] Test file: `tests/unit/input/input_parser_test.c`

### Task 8.4: Scrollback Edge Cases

**src/scrollback.c Lines 253, 267**: Physical row not found

- [ ] Write test: Call with invalid physical row number
- [ ] Write test: Physical row beyond scrollback range
- [ ] Verify error return path is covered
- [ ] Remove LCOV_EXCL_LINE markers
- [ ] Test file: `tests/unit/scrollback/scrollback_test.c`

**Category 8 Verification**:
- [ ] All edge cases tested
- [ ] 4-byte UTF-8 support verified working
- [ ] All tests pass: `make check`
- [ ] Coverage remains 100%: `make coverage`
- [ ] ~10 exclusion markers removed

---

## Final Verification

**Status**: [ ] Not Started

### Task: Complete Quality Check

- [ ] Run `make fmt` - Code formatted
- [ ] Run `make check` - All tests pass (100%)
- [ ] Run `make lint` - All complexity checks pass
- [ ] Run `make coverage` - Coverage at 100.0% (lines, functions, branches)
- [ ] Run `make check-dynamic` - All sanitizer checks pass

### Task: Count Exclusions

```bash
# Count remaining exclusions
grep -r "LCOV_EXCL" src/ | wc -l
```

- [ ] Document final exclusion count
- [ ] Verify ~65 exclusions remaining (down from ~120)
- [ ] Verify ~46% reduction achieved

### Task: Update Documentation

- [ ] Mark lcov_exclusion_review.md as complete
- [ ] Update this file with completion status
- [ ] Document any deviations from plan
- [ ] Document lessons learned

---

## Progress Tracking

**Overall Progress**: 7/8 categories complete

- [X] Category 1: Remove Dead Code
- [X] Category 2: Refactor OOM Checks
- [X] Category 3: Add Environmental/IO Tests
- [X] Category 4: Verify Invariant Format
- [X] Category 5: Verify UTF-8 Validation
- [X] Category 6: Verify Switch Exhaustiveness
- [X] Category 7: Render/IO Mixed Actions
- [ ] Category 8: Add Edge Case Tests

**Final Verification**: [ ] Not Started

---

## Notes

- Work one category at a time
- Run tests after each file change
- Verify coverage remains 100% throughout
- Document any unexpected findings
- Ask for review/clarification when needed

### Deferred Items

**src/config.c lines 70-75**: json_dump_file failure testing
- Cannot easily mock jansson's `json_dump_file()` without significant refactoring
- LCOV_EXCL markers remain (4 markers)
- **TODO**: Revisit during JSON library migration to yyjson (see docs/jansson_to_yyjson_proposal.md)
- With yyjson, can use custom allocator and wrapper for file operations
