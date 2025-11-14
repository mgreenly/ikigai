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

**Status**: [ ] Not Started

**Task**: Verify that UTF-8 validation assumptions are correct

### Task 5.1: Verify 4-byte UTF-8 Support

**Files**: `src/workspace_multiline.c`, `src/input.c`

- [ ] Check existing tests for 4-byte UTF-8 (emojis: 👍, 🎉, etc.)
- [ ] Verify tests exist in input parser tests
- [ ] Verify tests exist in workspace tests
- [ ] If missing: Add tests for 4-byte UTF-8 characters
- [ ] Verify exclusions are for invalid sequences, not valid 4-byte UTF-8

### Task 5.2: Verify Input Validation

**Files**: `src/workspace.c`, `src/workspace_layout.c`

- [ ] Verify input parser rejects invalid UTF-8 at entry points
- [ ] Check for validation in `ik_input_parse_byte`
- [ ] Verify workspace only receives valid UTF-8
- [ ] Confirm defensive checks in layout are truly unreachable

### Task 5.3: Verify utf8proc Library Guarantees

**File**: `src/workspace_layout.c` line 45

- [ ] Verify utf8proc documentation confirms `char_width >= 0` for valid codepoints
- [ ] Confirm defensive check is based on library guarantee
- [ ] No code changes needed

**Category 5 Verification**:
- [ ] 4-byte UTF-8 test coverage confirmed
- [ ] Input validation confirmed at entry points
- [ ] Library guarantees documented
- [ ] All checks remain single-line format

---

## Category 6: Verify Switch Statement Exhaustiveness

**Status**: [ ] Not Started

### Task 6.1: src/error.h (lines 137-148)

- [ ] Read switch statement on `error_code_t`
- [ ] Verify all enum values have cases
- [ ] Verify default case is single-line PANIC or similar
- [ ] Document which enum values are handled
- [ ] No code changes needed if exhaustive

### Task 6.2: src/repl.c (lines 314-374)

- [ ] Read switch statement on `ik_input_action_type_t`
- [ ] Verify all enum values have cases
- [ ] Verify default case is single-line PANIC (line 374-375)
- [ ] Document which enum values are handled
- [ ] No code changes needed if exhaustive

**Category 6 Verification**:
- [ ] Both switches confirmed exhaustive
- [ ] Default cases are single-line format
- [ ] Default cases handle corrupted enum values
- [ ] Documentation added to review file

---

## Category 7: Render/IO Defensive Checks - Mixed Actions

**Status**: [ ] Not Started

### Task 7.1: Refactor OOM Checks (covered in Category 2)
- [ ] Line 287-288: Already in Category 2, Task 2.4
- [ ] Line 334-335: Already in Category 2, Task 2.4

### Task 7.2: Verify Invariant Checks - Single-Line Format

**src/render.c**
- [ ] Line 227-228: Verify single-line format
- [ ] Line 251-252: Verify single-line format
- [ ] Line 289-291: Verify single-line format (may be 3 lines with cleanup)

**src/repl.c**
- [ ] Line 135-136: Verify single-line format
- [ ] Line 141-142: Verify single-line format
- [ ] Line 152-153: Already PANIC format, verify
- [ ] Line 190-191: Already PANIC format, verify
- [ ] Line 219-220: Verify single-line format
- [ ] Line 250-251: Verify single-line format

### Task 7.3: Investigate Input Validation

**src/repl.c Line 109-110**: Can codepoint validation fail?

- [ ] Read `ik_workspace_insert_codepoint` implementation
- [ ] Check if `encode_utf8` can return 0 (invalid codepoint)
- [ ] Check if input parser validates codepoint range (0-0x10FFFF)
- [ ] If parser validates: ACCEPT as invariant, verify single-line
- [ ] If parser doesn't validate: Write test with codepoint > 0x10FFFF
- [ ] Document findings in review file

**Category 7 Verification**:
- [ ] OOM checks refactored (via Category 2)
- [ ] All invariant checks single-line format
- [ ] Input validation investigated and decision made
- [ ] Tests added if needed
- [ ] Coverage remains 100%

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

**Overall Progress**: 4/8 categories complete

- [X] Category 1: Remove Dead Code
- [X] Category 2: Refactor OOM Checks
- [X] Category 3: Add Environmental/IO Tests
- [X] Category 4: Verify Invariant Format
- [ ] Category 5: Verify UTF-8 Validation
- [ ] Category 6: Verify Switch Exhaustiveness
- [ ] Category 7: Render/IO Mixed Actions
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
