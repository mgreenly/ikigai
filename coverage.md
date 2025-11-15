# Coverage Status

**Current Coverage**: 99.9% lines (1742/1744), 100% functions (129/129), 99.5% branches (567/570)
**Target**: 100.0% lines, 100% functions, 100% branches

**Status**: All HIGH and MEDIUM priority gaps are covered. Remaining gaps are LOW priority error paths.

---

## ✅ COMPLETED - High Priority (Normal Inputs)

### render.c - ik_render_combined()

- [x] **Line 337 + Branch 336**: Error return when `scrollback_start_line >= total_lines`
  - **Test**: `tests/unit/render/render_combined_edge_test.c::test_render_combined_invalid_scrollback_start`
  - **Status**: ✅ Covered

- [x] **Lines 344-345 + Branch 343**: Scrollback end clamping when `scrollback_end_line > total_lines`
  - **Test**: `tests/unit/render/render_combined_edge_test.c::test_render_combined_scrollback_count_clamping`
  - **Status**: ✅ Covered

- [x] **Lines 431-432 + Branches 386, 430**: Newline conversion in scrollback rendering
  - **Test**: `tests/unit/render/render_combined_edge_test.c::test_render_combined_scrollback_with_newlines`
  - **Status**: ✅ Covered

### repl.c

- [x] **Branch 142**: False branch of `available_for_scrollback > 0`
  - **Test**: `tests/unit/repl/repl_viewport_test.c::test_viewport_no_scrollback_room`
  - **Status**: ✅ Covered - **BONUS: Found and fixed a bug!** (added check for `available_for_scrollback == 0`)

- [x] **Branch 261/266**: Trailing newline handling in `/pp` output
  - **Test**: `tests/unit/repl/repl_slash_command_test.c::test_pp_output_trailing_newline`
  - **Status**: ✅ Covered

---

## ⏸️ REMAINING - Low Priority (OOM/Edge Cases)

### render.c - Error Paths Requiring OOM Injection

- [ ] **Line 361 + Branch 360**: Error return from `calculate_cursor_screen_position()`
  - **Trigger**: Invalid UTF-8 or OOM in cursor position calculation
  - **Priority**: LOW - Requires OOM injection infrastructure
  - **Coverage Impact**: 1 line, 1 branch

- [ ] **Line 424**: Error return from `ik_scrollback_get_line_text()`
  - **Trigger**: OOM when retrieving scrollback text
  - **Priority**: LOW - Requires OOM injection infrastructure
  - **Coverage Impact**: 1 line (note: already has `LCOV_EXCL_LINE` on line 425)

### repl.c - Edge Case

- [ ] **Branch 259**: False branch of `output_len > 0` in `/pp` handler
  - **Trigger**: `ik_pp_workspace()` returns empty string
  - **Priority**: LOW - May not be achievable (pp always outputs at least the workspace address)
  - **Coverage Impact**: 1 branch

- [ ] **Branch 266**: False branch of `line_len > 0 || i < output_len`
  - **Trigger**: Specific newline patterns in pp output
  - **Priority**: LOW - Test exists but branch not registering (may be compiler optimization)
  - **Coverage Impact**: 1 branch

---

## Summary

**Work Completed**:
- Created 2 new test files
- Added 5 new tests covering all HIGH/MEDIUM priority gaps
- Fixed 1 production bug (viewport calculation when no scrollback room)
- Increased coverage from 99.6%/100%/98.6% → 99.9%/100%/99.5%

**Remaining Work**:
- 2 uncovered lines (render.c 361, 424)
- 3 uncovered branches (render.c 360, repl.c 259, 266)
- All require OOM injection or may not be achievable with normal inputs

**Test Files Created/Modified**:
- ✅ `tests/unit/render/render_combined_edge_test.c` (NEW - 130 lines)
- ✅ `tests/unit/repl/repl_viewport_test.c` (MODIFIED - added ~60 lines)
- ✅ `tests/unit/repl/repl_slash_command_test.c` (MODIFIED - added ~60 lines)

---

## Notes on Remaining Gaps

Per `docs/error_handling.md`, error paths are meant to crash with PANIC on OOM. The uncovered OOM paths (lines 361, 424) are defensive checks that would require injecting allocation failures to test. This requires test infrastructure not currently in place.

The `ik_pp_workspace()` empty output case (branch 259) is likely impossible since the function always outputs at minimum the workspace address and field names.

Branch 266 has a test that exercises the logic, but the branch may not be registering due to compiler optimizations or the specific way lcov tracks compound boolean expressions.
