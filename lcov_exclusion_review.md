# LCOV Exclusion Review

This file lists all LCOV_EXCL markers that are NOT for function argument assertions or PANIC statements.

## Review Process

**We work ONE category at a time:**
1. Discuss the category together
2. User makes the final decision
3. Document the decision
4. Move to next category

**Agent does NOT work ahead or make decisions autonomously.**

**Total exclusions to review**: 120 markers across 16 files

**Categories**:
- [x] Category 1: Entire files excluded (cannot be tested) - COMPLETED
- [x] Category 2: OOM defensive error handling (recently added) - COMPLETED
- [x] Category 3: Environmental/IO Errors - COMPLETED
- [x] Category 4: Defensive null/bounds checks (may be testable) - COMPLETED
- [x] Category 5: Invalid UTF-8 handling (may be testable) - COMPLETED
- [x] Category 6: Unreachable code paths (switch defaults, etc.) - COMPLETED
- [x] Category 7: Render/IO defensive checks - COMPLETED
- [x] Category 8: Short-circuit optimizations & rare branches - COMPLETED

---

## Category 1: Entire Files Excluded (Cannot Be Tested)

**Status**: ✅ DECISIONS COMPLETE

**Summary**:
- src/panic.c: ACCEPTED (abort handlers)
- src/wrapper.c: ACCEPTED (test infrastructure)
- src/client.c: ACCEPTED (main entry point)
- src/logger.c: REMOVE ik_log_fatal() (dead code)

**Plan**:
1. ✓ INVESTIGATE src/panic.c - Reviewed, recommend ACCEPT
2. ✓ INVESTIGATE src/wrapper.c - Reviewed, recommend ACCEPT
3. ✓ INVESTIGATE src/client.c - Reviewed, recommend ACCEPT
4. ✓ INVESTIGATE src/logger.c - Reviewed, recommend ACCEPT

**Investigation Results**:

### src/panic.c (entire file)
```
Lines 1-130: LCOV_EXCL_START/STOP
Code: Panic/abort handlers
Reason: All functions call abort() which terminates the process
Analysis: These functions cannot be tested in normal test harness
Recommendation: ACCEPT - Legitimately untestable, functions designed to crash program
User decision: [X] ACCEPTED
```

### src/wrapper.c (entire file)
```
Lines 11-118: LCOV_EXCL_START/STOP (only in debug builds, #ifndef NDEBUG)
Code: Link seam wrappers for talloc, jansson, POSIX calls
Reason:
  - Only compiled in debug/test builds
  - Trivial pass-through functions (e.g., "return talloc_zero_size(ctx, size)")
  - Exist as MOCKABLE weak symbols so tests can override them for failure injection
  - Tests mock these wrappers rather than executing the real implementations
  - Coverage would measure the mock stubs, not the real wrappers
Analysis: These enable testing of error paths in calling code, not meant to be tested themselves
Recommendation: ACCEPT - Test infrastructure, not application code
User decision: [X] ACCEPTED
```

### src/client.c (entire file)
```
Lines 8-36: LCOV_EXCL_START/STOP
Code: main() function entry point
Reason: main() not invoked by unit tests
Analysis: Standard practice to exclude program entry point from unit test coverage
Recommendation: ACCEPT - Would require integration tests to cover
User decision: [X] ACCEPTED
```

### src/logger.c (partial)
```
Lines 115-134: LCOV_EXCL_START/STOP
Code: ik_log_fatal() function
Reason: Function calls abort() at end (line 131), terminates process
Analysis: Dead code - not used anywhere, PANIC() macro has replaced it
Recommendation: REMOVE - Delete ik_log_fatal() entirely
User decision: [X] REMOVE (dead code cleanup)
```

---

## Category 2: OOM Defensive Error Handling (Recently Added)

**Status**: Under review

**Context**: All recently added during OOM refactoring. These check for OOM conditions that now panic before returning errors.

**Analysis**: After the OOM refactoring, allocation functions now PANIC on failure instead of returning errors. This makes downstream error checks unreachable - they're defensive code that can never execute because the program would have already crashed.

**Decision**: ACCEPT with REFACTOR - Convert all multi-line `is_err()` checks to single-line PANIC statements

**Pattern to convert FROM (3 lines, 2 exclusions):**
```c
result_t res = grow_array(array);
if (is_err(&res)) {                // LCOV_EXCL_LINE
    return res;                     // LCOV_EXCL_LINE
}
```

**Pattern to convert TO (1 line, 1 exclusion):**
```c
result_t res = grow_array(array);
if (is_err(&res)) PANIC("grow_array failed"); // LCOV_EXCL_LINE
```

### src/array.c
```
Line 78-79: is_err(&res) check after grow_array() in ik_array_append
Line 101-102: is_err(&res) check after grow_array() in ik_array_insert
Decision: [X] REFACTOR to single-line PANIC
```

### src/format.c
```
Line 26-28: is_err(&res) check after ik_byte_array_create in ik_format_buffer_create
Line 65-67: is_err(&res) check in ik_format_append loop
Line 87-88: is_err(&res) check in ik_format_append_str loop
Line 106-107: is_err(&res) check in ik_format_append_indent loop
Line 129-130: is_err(&res) check in ik_format_get_string
Decision: [X] REFACTOR to single-line PANIC
```

### src/workspace.c
```
Line 22-24: is_err(&res) check after ik_byte_array_create in ik_workspace_create
Line 29-31: is_err(&res) check after ik_cursor_create in ik_workspace_create
Line 119-120: is_err(&res) check in ik_workspace_insert_codepoint loop
Line 146-147: is_err(&res) check in ik_workspace_insert_newline
Decision: [X] REFACTOR to single-line PANIC
```

### src/repl.c
```
Line 40-42: is_err(&result) after ik_workspace_create
Line 47-49: is_err(&result) after ik_input_parser_create
Line 54-56: is_err(&result) after ik_scrollback_create
Line 287-288: is_err(&result) after ik_format_buffer_create
Decision: [X] REFACTOR to single-line PANIC
```

---

## Category 3: Environmental/IO Errors

**Status**: ✅ DECISIONS COMPLETE

**Decision**: ADD TEST COVERAGE - Refactor code if needed, add wrappers for mocking, write tests

**Strategy**:
- Add wrappers to wrapper.c/wrapper.h for any system calls not yet wrapped
- Use mock failures to trigger error paths
- Refactor code if necessary to make testable

### Group 3A: Terminal initialization failures (src/repl.c)

```
Line 22-24: is_err(&result) after ik_term_init
Comment: "Environmental failure (no /dev/tty, terminal setup fails)"
Decision: [X] ADD TESTS - Mock terminal init failure
Approach: Mock ik_open_wrapper("/dev/tty") to fail
```

```
Line 33-35: is_err(&result) after ik_render_create
Comment: "Defensive check for corrupted terminal state"
Decision: [X] ADD TESTS - Mock terminal state corruption
Approach: Mock ik_ioctl_wrapper to return invalid dimensions
```

### Group 3B: Filesystem operations (src/config.c)

```
Line 53-58: stat() failure and mkdir() failure
Comment: "directory exists case difficult to test" / "mkdir failure requires permission/disk errors"
Decision: [X] ADD TESTS - Add wrappers for stat/mkdir, mock failures
Approach: Add ik_stat_wrapper and ik_mkdir_wrapper to wrapper.c/h
```

```
Line 70-75: json_dump_file() failure
Comment: "json_dump_file failure requires disk/permission errors"
Decision: [X] ADD TESTS - Investigate if json_dump_file can be wrapped
Approach: May need to refactor to use explicit write calls we can mock
```

```
Line 95-99: Config creation errors
Comment: "create errors tested above, path covered in other tests"
Decision: [X] INVESTIGATE - Verify if actually covered or needs test
Approach: Check coverage data, add test if gap exists
```

### Group 3C: Terminal write failures (src/render.c)

```
Line 316-317: bytes_written < 0 check in write()
Comment: IO error writing to terminal
Decision: [X] ADD TESTS - Mock write failure
Approach: Mock ik_write_wrapper to return -1 (already exists in wrapper.c)
```

---

## Category 4: Defensive Null/Bounds Checks

**Status**: ✅ DECISIONS COMPLETE

**Decision**: ACCEPT - These check internal invariants/code logic, not IO errors. They're defensive fast-fail checks for development.

**Criteria for acceptance**:
- Checks internal consistency (not external/IO failures)
- Indicates programming error if triggered
- Simple checks that help catch bugs during development
- Should be single-line format (refactor if multi-line)

### src/workspace.c
```
Line 237-238: cursor_pos >= data_len in find_prev_char_start
Comment: "defensive: caller checks cursor < data_len"
Analysis: Internal invariant - caller guarantees cursor bounds
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 266: end_pos > data_len check
Comment: "defensive: well-formed UTF-8 won't exceed buffer"
Analysis: Internal invariant - UTF-8 parsing should stay in bounds
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 318-319: text == NULL check in ik_workspace_cursor_left
Comment: "defensive: cursor > 0 implies text != NULL"
Analysis: Internal invariant - cursor position implies text exists
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 342: text == NULL || cursor >= text_len in ik_workspace_cursor_right
Comment: "defensive: text NULL only when cursor at 0"
Analysis: Internal invariant - text/cursor consistency
Decision: [X] ACCEPT (verify single-line format)
```

### src/workspace_layout.c
```
Line 21-22: text == NULL || len == 0
Comment: "defensive: text is never NULL when len > 0"
Analysis: Internal invariant - len implies text exists
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 70: text == NULL || text_len == 0
Comment: "defensive: text is NULL only when text_len is 0"
Analysis: Internal invariant - same as above
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 96: terminal_width > 0
Comment: "defensive: terminal_width is always > 0"
Analysis: Internal invariant - validated at terminal creation
Decision: [X] ACCEPT (verify single-line format)
```

### src/workspace_pp.c
```
Line 26: workspace->text != NULL
Comment: "defensive: text always allocated in create"
Analysis: Internal invariant - create() guarantees text allocation
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 35: workspace->cursor != NULL
Comment: "defensive: cursor always allocated in create"
Analysis: Internal invariant - create() guarantees cursor allocation
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 43: text_len > 0 && text != NULL
Comment: "defensive: text always non-NULL when text_len > 0"
Analysis: Internal invariant - len implies text exists
Decision: [X] ACCEPT (verify single-line format)
```

**Action**: Verify all are single-line format during implementation phase

---

## Category 5: Invalid UTF-8 Handling

**Status**: ✅ DECISIONS COMPLETE

**Decision**: ACCEPT - These must be true internal invariants (input validation guarantees valid UTF-8)

**Verification requirements** (must verify during implementation):
1. ✓ Input validation actually rejects invalid UTF-8 at entry points
2. ✓ 4-byte UTF-8 support works correctly (emojis, etc.)
3. ✓ These defensive checks are truly unreachable with valid input
4. ✓ All are single-line format

### src/workspace_multiline.c
```
Line 85, 88-89: 4-byte UTF-8 character handling (abort on invalid)
Line 133, 136-137: Same in different function
Analysis: Must verify 4-byte UTF-8 (emojis) works correctly and only invalid sequences abort
VERIFY: 4-byte UTF-8 test coverage exists (emojis like 👍, 🎉, etc.)
Decision: [X] ACCEPT (must verify 4-byte UTF-8 is tested and works)
```

### src/workspace.c
```
Line 254, 258-261: Invalid UTF-8 lead byte handling
Comment: "Invalid UTF-8 lead byte - never occurs with valid input"
Analysis: Input validation must guarantee only valid UTF-8 reaches this code
VERIFY: Input validation rejects invalid UTF-8 sequences
Decision: [X] ACCEPT (must verify input validation exists)
```

### src/workspace_layout.c
```
Line 34-40: bytes_read <= 0 from utf8proc
Comment: "defensive: workspace only contains valid UTF-8"
Analysis: Input validation must guarantee only valid UTF-8 in workspace
VERIFY: Workspace entry points validate UTF-8
Decision: [X] ACCEPT (must verify validation at entry points)
```

```
Line 45: char_width >= 0 check
Comment: "defensive: utf8proc_charwidth returns >= 0 for all valid codepoints"
Analysis: Library guarantee - utf8proc returns >= 0 for valid codepoints
Decision: [X] ACCEPT (library contract)
```

**Critical verification tasks**:
- Verify existing tests cover 4-byte UTF-8 characters (emojis)
- Verify input parser rejects invalid UTF-8 sequences
- If validation gaps exist, add tests for input validation

---

## Category 6: Unreachable Code Paths

**Status**: ✅ DECISIONS COMPLETE

**Decision**: ACCEPT - Default cases that represent broken invariants are acceptable

**Criteria for acceptance**:
- Default case catches corrupted/invalid enum values (broken invariant)
- Indicates data structure corruption or programming error
- Should never be reached in normal execution
- Must be single-line format

### src/error.h
```
Line 137: switch (code) on error code
Line 148: default case
Analysis: Default catches invalid error_code_t values (corrupted enum)
VERIFY: Switch handles all valid error_code_t enum values
VERIFY: Default case is single-line (e.g., PANIC or return sentinel)
Decision: [X] ACCEPT (must verify exhaustive + single-line format)
```

### src/repl.c
```
Line 314: switch (action->type) in ik_repl_process_action
Line 374: default case
Analysis: Default catches invalid ik_input_action_type_t values (corrupted enum)
VERIFY: Switch handles all valid ik_input_action_type_t enum values
VERIFY: Default case is single-line format
Decision: [X] ACCEPT (must verify exhaustive + single-line format)
```

**Verification requirements**:
- Confirm all enum values are handled in each switch
- Confirm default cases are single-line format
- Confirm defaults handle invariant violations (corrupted data)

---

## Category 7: Render/IO Defensive Checks

**Status**: ✅ DECISIONS COMPLETE (mixed)

### Group 7A: OOM Checks - REFACTOR to single-line PANIC

**src/repl.c**
```
Line 287-288: is_err after ik_format_buffer_create in ik_repl_handle_slash_command
Analysis: OOM check (format buffer creation can fail on allocation)
Decision: [X] REFACTOR to single-line PANIC
```

```
Line 334-335: is_err after ik_repl_handle_slash_command
Comment: "OOM defensive check"
Analysis: Propagates OOM from ik_format_buffer_create
Decision: [X] REFACTOR to single-line PANIC (or remove if 287-288 fixed)
```

### Group 7B: Invariant Checks - ACCEPT (verify single-line)

**src/render.c**
```
Line 227-228: is_err after ik_scrollback_ensure_layout
Line 251-252: is_err after ik_scrollback_get_line_text (bounds pre-validated)
Line 289-291: is_err after ik_scrollback_get_line_text (bounds pre-validated)
Analysis: These functions only return OK or check already-validated bounds
Decision: [X] ACCEPT (verify single-line format)
```

**src/repl.c**
```
Line 135-136: is_err after ik_workspace_ensure_layout
Line 141-142: is_err after ik_scrollback_ensure_layout
Analysis: ensure_layout functions only return OK (no possible errors)
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 152-153: workspace_rows > terminal_rows (already PANIC)
Comment: "should never occur - invariant"
Analysis: Already uses PANIC - workspace should never exceed terminal
Decision: [X] ACCEPT (already correct format)
```

```
Line 190-191: is_err after ik_scrollback_find_logical_line_at_physical_row (already PANIC)
Analysis: Already uses PANIC for corrupted data structure
Decision: [X] ACCEPT (already correct format)
```

```
Line 219-220: is_err after ik_repl_calculate_viewport
Analysis: Only calls ensure_layout (OK only) and find_logical_line (PANICs on error)
Decision: [X] ACCEPT (verify single-line format)
```

```
Line 250-251: is_err after ik_render_scrollback
Analysis: Only fails on out-of-range which can't happen with correct viewport calculation
Decision: [X] ACCEPT (verify single-line format)
```

### Group 7C: User Input Validation - INVESTIGATE & TEST

**src/repl.c**
```
Line 109-110: is_err after ik_repl_process_action
Comment: "Defensive check, input parser validates codepoints"
Analysis: Can return INVALID_ARG if codepoint > 0x10FFFF
  - Input decoder can produce invalid codepoints from malformed UTF-8
  - 4-byte UTF-8 can encode up to 0x1FFFFF but Unicode max is 0x10FFFF
Question: Does input parser validate codepoint range after decoding?
Decision: [X] INVESTIGATE - Verify if parser validates, then either:
  - If parser validates → ACCEPT as invariant check
  - If parser doesn't validate → ADD TEST for invalid codepoints
```

---

## Category 8: Short-Circuit Optimizations & Rare Branches

**Status**: ✅ DECISIONS COMPLETE

**Decision**: ADD TEST COVERAGE - All of these are possible edge cases that must be tested

### Group 8A: Empty/Zero Cases - ADD TESTS

**src/workspace_cursor.c**
```
Line 108: grapheme_count > 0 ? grapheme_count - 1 : 0
Analysis: Boundary case - empty text or cursor at start
Decision: [X] ADD TEST - Test with empty workspace, grapheme_count == 0
```

**src/workspace_layout.c**
```
Line 94: display_width == 0
Comment: "rare: only zero-width characters"
Analysis: Zero-width characters exist (combining marks, ZWJ, etc.)
Decision: [X] ADD TEST - Test with zero-width joiners and combining characters
```

```
Line 101: physical_lines += 1 (in else branch)
Analysis: Edge case in physical line calculation
Decision: [X] ADD TEST - Investigate what condition triggers this, add test
```

**src/format.c**
```
Line 144: len > 0 && array[len-1] == '\0'
Comment: "short-circuit branch"
Analysis: Tests if buffer already null-terminated
Decision: [X] ADD TEST - Test both null-terminated and non-terminated buffers
```

### Group 8B: 4-byte UTF-8 - ADD TESTS

**src/input.c**
```
Line 70: len == 4 (4-byte UTF-8)
Analysis: 4-byte UTF-8 is valid and common (emojis: 👍, 🎉, etc.)
Decision: [X] ADD TEST - Test with emoji and other 4-byte UTF-8 characters
Note: This is critical - verifies 4-byte UTF-8 support works correctly
```

### Group 8C: Escape Sequence Variations - ADD TESTS

**src/input.c**
```
Line 213: byte <= 'Z' (uppercase letter in escape sequence)
Analysis: Some terminals may send uppercase escape codes
Decision: [X] ADD TEST - Test escape sequences with uppercase letters if valid
```

### Group 8D: Scrollback Edge Cases - ADD TESTS

**src/scrollback.c**
```
Line 253: for loop over scrollback->count
Line 267: ERR return after loop
Analysis: Error case when physical row not found in scrollback
Decision: [X] ADD TEST - Test with invalid/out-of-range physical row numbers
```

---

## Review Process

For each exclusion:

1. **Understand the context**: Read the surrounding code
2. **Determine category**:
   - ACCEPT: Legitimately untestable (panics, aborts, main entry)
   - TESTABLE: Can write a test to cover this
   - REFACTOR: Code should be restructured to be testable
   - INVESTIGATE: Need more information
3. **Document decision**: Add rationale
4. **Take action**:
   - Write test if testable
   - Remove exclusion if test added
   - Keep exclusion if legitimately untestable
   - Refactor code if needed

---

## Final Summary

**✅ ALL CATEGORIES REVIEWED - DECISIONS COMPLETE**

### Decisions by Action Type

**ACCEPT (Keep Exclusions)**: ~65 exclusions
- Entire files: panic.c, wrapper.c, client.c (legitimate)
- OOM defensive checks: ~13 after refactoring to single-line PANIC
- Invariant checks: ~40 defensive checks for internal consistency
- Switch defaults: 2 exhaustive switch default cases

**REMOVE (Dead Code)**: 1 item
- logger.c: ik_log_fatal() function (replaced by PANIC macro)

**REFACTOR (Change Format)**: ~25 exclusions
- Category 2: Convert multi-line OOM checks to single-line PANIC
- Category 7: Convert 2 OOM checks to single-line PANIC
- Verify all accepted exclusions are single-line format

**ADD TESTS**: ~30 exclusions to eliminate
- Category 3: Environmental/IO errors (~15 tests)
- Category 7: Input validation investigation (1 test)
- Category 8: Edge cases and rare branches (~10 tests)

### Implementation Phases

**Phase 1: Code Cleanup**
1. Remove ik_log_fatal() from logger.c/logger.h
2. Refactor all multi-line OOM checks to single-line PANIC (~25 locations)
3. Verify all accepted exclusions use single-line format

**Phase 2: Add Test Coverage**
1. Environmental/IO errors (Category 3):
   - Add stat/mkdir wrappers, test filesystem failures
   - Mock terminal init failures
   - Test write failures
2. Input validation (Category 7):
   - Investigate codepoint validation in input parser
   - Add test if validation missing
3. Edge cases (Category 8):
   - Zero-width characters, empty cases
   - 4-byte UTF-8 (emojis) - CRITICAL
   - Escape sequence variations
   - Scrollback boundary cases

**Phase 3: Verification**
1. Verify all invariant assumptions hold
2. Verify UTF-8 validation at entry points
3. Verify switch statements are exhaustive
4. Confirm format buffer creation always succeeds or panics

### Expected Impact

**Before**: ~120 exclusion markers
**After**:
- ~65 legitimate exclusions (single-line format)
- ~30 new tests eliminating exclusions
- ~25 refactored to reduce marker count

**Net reduction**: ~55 exclusion markers removed (~46% reduction)
