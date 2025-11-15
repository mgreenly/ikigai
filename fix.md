# Phase 4 Post-Implementation Fixes

**Status**: Phase 4 was marked "complete" but had critical bugs and missing features discovered during manual testing.

## Issues Found and Fixed

### 1. Critical Bug: Double Screen Clear (FIXED)
**Problem**: Scrollback was invisible - rendered then immediately erased
- `ik_render_scrollback()` cleared screen and rendered scrollback
- Then `ik_render_workspace()` cleared screen AGAIN, erasing scrollback
- User reported: "I see no viewport"

**Fix**: Implemented `ik_render_combined()` (src/render.c:318-473)
- Single atomic write: Clear → Scrollback → Workspace → Cursor
- Updated `ik_repl_render_frame()` to use combined renderer (src/repl.c:220-226)

**Tests Needed**:
- [ ] Test `ik_render_combined()` with scrollback + workspace
- [ ] Test output contains both scrollback and workspace text
- [ ] Test only ONE screen clear in output
- [ ] Test scrollback appears before workspace in output
- [ ] Test cursor positioned correctly (after scrollback rows)

**File**: `tests/unit/render/render_combined_test.c` (needs implementation)

---

### 2. Enter Key Behavior (FIXED)
**Problem**: Enter didn't submit to scrollback
- Enter inserted newline for multi-line editing
- No way to submit workspace content to scrollback

**Fix**: Changed Enter to always submit (src/repl.c:308-327)
- Enter (`IK_INPUT_NEWLINE`) now calls `ik_repl_submit_line()`
- Workspace clears after submit
- Content moves to scrollback

**Tests Needed**:
- [ ] Test Enter key submits workspace to scrollback
- [ ] Test workspace cleared after submit
- [ ] Test scrollback contains submitted text
- [ ] Test auto-scroll to bottom after submit (viewport_offset = 0)

**File**: `tests/unit/repl/repl_submit_test.c` (new file needed)

---

### 3. /pp Command Output (FIXED)
**Problem**: `/pp` command did nothing visible
- Output went to stdout (invisible in alternate screen buffer)

**Fix**: Output to scrollback (src/repl.c:252-258)
- `/pp` now calls `ik_scrollback_append_line()`
- Output appears in scrollback area

**Tests Needed**:
- [ ] Test `/pp` command appends to scrollback
- [ ] Test `/pp` output contains workspace debug info
- [ ] Test `/pp` doesn't crash on empty workspace
- [ ] Test `/pp` then submit moves both to scrollback

**File**: `tests/unit/repl/repl_slash_command_test.c` (update existing)

---

### 4. Visual Separator Line (ADDED)
**Problem**: No visual separation between scrollback and workspace
- User couldn't distinguish scrollback from workspace

**Fix**: Added separator line (src/render.c:440-451)
- Line of dashes (`-`) between scrollback and workspace
- Only shown when scrollback exists
- Cursor position adjusted (+1 row)
- Viewport calculation reserves row (src/repl.c:142-144)

**Tests Needed**:
- [ ] Test separator appears when scrollback exists
- [ ] Test separator is full terminal width
- [ ] Test no separator when scrollback empty
- [ ] Test cursor positioned correctly after separator
- [ ] Test viewport reserves 1 row for separator

**File**: `tests/unit/render/render_separator_test.c` (new file needed)

---

### 5. Multi-line Input Support (ADDED)
**Problem**: No way to create multi-line input after Enter changed to submit
- Users need multi-line editing for prompts, code, etc.

**Fix**: Added Ctrl+J for newline insertion (src/input.c:280-283, src/repl.c:305-307)
- New action: `IK_INPUT_INSERT_NEWLINE`
- **Ctrl+J** → Insert newline without submitting
- **Enter** → Submit to scrollback

**Tests Needed**:
- [ ] Test Ctrl+J (0x0A) parsed as `IK_INPUT_INSERT_NEWLINE`
- [ ] Test Enter (0x0D) parsed as `IK_INPUT_NEWLINE`
- [ ] Test `IK_INPUT_INSERT_NEWLINE` calls `ik_workspace_insert_newline()`
- [ ] Test `IK_INPUT_NEWLINE` calls `ik_repl_submit_line()`
- [ ] Test multi-line workspace renders correctly
- [ ] Test cursor navigation in multi-line workspace (up/down arrows)
- [ ] Test submitting multi-line content to scrollback

**Files**:
- `tests/unit/input/multiline_test.c` (new file needed)
- `tests/unit/repl/repl_multiline_test.c` (new file needed)

---

### 6. Bug #3: Ctrl+U Crash (FIXED - 2025-11-14)
**Problem**: Crash after Ctrl+U when trying to insert a character
- Type text, press Ctrl+U (kill line), press any key → CRASH
- Error: `Assertion 'index <= array->size' failed` in src/array.c:94
- Root cause: `workspace->cursor_byte_offset` not updated after kill_line

**Fix**: Updated cursor_byte_offset in kill operations (src/workspace_multiline.c)
- `ik_workspace_kill_line()` now updates `workspace->cursor_byte_offset` after deletion (line 403)
- `ik_workspace_kill_to_line_end()` now updates `workspace->cursor_byte_offset` (line 355)
- Added bounds check to ensure cursor doesn't exceed text length

**Test**: Added `test_workspace_kill_line_then_insert` (tests/unit/workspace/kill_line_test.c:232-288)
- Reproduces the exact crash scenario: kill_line followed by insert
- Test FAILED before fix (crashed with SIGABRT)
- Test PASSES after fix

---

### 7. Bug #1: Initial Render - No Separator Line (FIXED - 2025-11-14)
**Problem**: No separator line visible on startup
- Program starts with blank screen, no separator
- Cursor at position 0,0 instead of below separator at 1,0
- Separator only rendered when `scrollback_line_count > 0`

**Fix**: Always render separator line (src/render.c, src/repl.c)
- `ik_render_combined()` now always renders separator (line 445-454)
- Removed conditional check that only showed separator with scrollback
- Updated buffer size calculation to always include separator (line 373-374)
- Updated viewport to always reserve 1 row for separator (src/repl.c:142-143)
- Always use `ik_render_combined()` instead of `ik_render_workspace()` (src/repl.c:218-225)

**Test**: Enhanced `test_render_frame_empty_scrollback` (tests/unit/repl/repl_combined_render_test.c:86-111)
- Verifies separator appears even with empty scrollback
- Looks for at least 10 consecutive dashes in output
- Test FAILED before fix (no separator found)
- Test PASSES after fix

---

### 8. Bug #2: Slash Command Order in Scrollback (FIXED - 2025-11-14)
**Problem**: Slash command output appeared BEFORE the command text
- User types "/pp" and presses Enter
- Scrollback showed: PP output, then "/pp" text
- Expected: "/pp" text, then PP output

**Root Cause**: In `ik_repl_process_action()` for IK_INPUT_NEWLINE:
- Called `ik_repl_handle_slash_command()` first (appends output to scrollback)
- Then called `ik_repl_submit_line()` (appends command text to scrollback)
- Wrong order!

**Fix**: Reversed the order (src/repl.c:303-333)
- Extract command text BEFORE submit (since submit clears workspace)
- Call `ik_repl_submit_line()` first (appends "/pp" to scrollback)
- Then call `ik_repl_handle_slash_command()` (appends PP output to scrollback)
- Now order is correct: command text, then output

**Test**: Added `test_pp_command_order_in_scrollback` (tests/unit/repl/repl_slash_command_test.c:207-237)
- Verifies first line in scrollback is "/pp"
- Verifies second line contains PP output
- Test FAILED before fix (PP output was first)
- Test PASSES after fix

**Side Effects**: Updated 4 tests to reflect Phase 4 behavior (Enter always submits):
- `test_pp_command_clears_workspace` - added scrollback creation
- `test_pp_command_with_args` - added scrollback creation
- `test_empty_workspace_newline` - workspace is now empty after Enter (not "\n")
- `test_slash_in_middle_not_command` - workspace is now empty after Enter (not "hello\n")

---

### 9. Bug #2.1: Cursor Position After /pp Command (FIXED - 2025-11-14)
**Problem**: Cursor appeared inside scrollback area after `/pp` command
- After executing `/pp`, cursor was 5-6 lines above where it should be
- Cursor appeared inside the PP output text instead of in workspace area
- Expected: cursor should be in workspace area (below separator)

**Root Cause**: PP output was appended as a SINGLE line with embedded `\n` characters
- `ik_pp_workspace` generates multi-line output like:
  ```
  ik_workspace_t @ 0x...
    text_len: 0
    ik_cursor_t @ 0x...
      byte_offset: 0
      grapheme_offset: 0
  ```
- This was appended as ONE line to scrollback via `ik_scrollback_append_line`
- Scrollback layout didn't correctly handle embedded newlines
- Cursor position calculation counted it as 1 line instead of 7+ lines

**Fix**: Split PP output by newlines before appending to scrollback (src/repl.c:251-268)
- Loop through output string and find each `\n`
- Call `ik_scrollback_append_line` for each individual line
- Now each line of PP output is a separate scrollback line
- Cursor position calculation now correctly accounts for all lines

**Lesson**: There should be ONE standard way to add text to scrollback
- Always append line-by-line, not multi-line strings
- Prevents cursor positioning and layout calculation bugs

---

## Known Issues Still To Test

### 10. Bug #4: Incorrect Viewport Model - Separator/Edit Zone Should Scroll (FOUND - 2025-11-14)
**Problem**: Separator and edit zone stay "sticky" at bottom when scrolling
- Current: Scrollback viewport moves, separator/workspace stay fixed at bottom
- Expected: Entire document (scrollback + separator + workspace) should scroll as one unit

**Current (WRONG) behavior:**
```
[Scrollback lines 50-70]  ← viewport scrolls this region
------------------------  ← separator fixed at bottom
Edit zone here           ← workspace fixed at bottom
```

**Correct behavior:**
```
The document is: SCROLLBACK → SEPARATOR → EDITZONE
The screen is a viewport into this document.

When scrolled to bottom (offset=0):
[Scrollback lines 85-100]
------------------------
Edit zone here

When scrolled up (offset=30):
[Scrollback lines 55-70]
[Scrollback lines 71-85]
[Scrollback lines 86-100]
------------------------  ← separator scrolled off screen
Edit zone scrolled off    ← workspace scrolled off screen
```

**Key principle:** Last line of scrollback ALWAYS appears directly above separator

**What needs to change:**
1. Viewport calculation: Document height = scrollback_lines + 1 (separator) + workspace_lines
2. Rendering logic: Calculate which lines of total document are visible
3. Render only visible portions (separator/workspace may be off-screen)
4. Cursor positioning: Account for document scroll, not just scrollback scroll

**Files affected:**
- `src/render.c` - `ik_render_combined()` needs complete rewrite
- `src/repl.c` - Viewport calculation and cursor positioning
- All scrolling tests need updates

---

### 11. Bug #5: Terminal Resize Not Immediate (FOUND - 2025-11-14)
**Problem**: Terminal resize doesn't trigger immediate redraw
- Current: Resize happens, but display doesn't update until next keypress
- Separator line keeps original width until keypress
- Expected: Immediate redraw on resize with updated separator width

**What happens:**
1. User resizes terminal window (drag edge or maximize)
2. Terminal size changes but display stays stale
3. Press any key → display updates to new size

**Expected behavior:**
1. User resizes terminal
2. SIGWINCH signal caught
3. Update terminal dimensions
4. Invalidate layout cache (scrollback reflow needed)
5. Immediate redraw with new separator width

**Root causes:**
1. Not listening to SIGWINCH, or not handling it properly
2. Separator width calculated once, not recalculated on resize
3. No immediate render triggered on resize

**Files to check:**
- `src/repl.c` - Event loop, SIGWINCH handling
- `src/render.c` - Separator width calculation
- `src/scrollback.c` - Layout cache invalidation on width change

**Test cases needed:**
- [ ] SIGWINCH triggers terminal dimension update
- [ ] SIGWINCH invalidates scrollback layout cache
- [ ] SIGWINCH triggers immediate redraw
- [ ] Separator width matches new terminal width
- [ ] Text reflows correctly to new width
- [ ] Cursor remains visible after resize

---

### Scrolling
- [x] Page Up scrolling - Works but uses wrong viewport model (Bug #4)
- [x] Page Down scrolling - Works but uses wrong viewport model (Bug #4)
- [ ] Scrolling to top (boundary)
- [ ] Scrolling to bottom (boundary)
- [ ] Scroll state persistence while typing
- [ ] Auto-scroll to bottom on submit

### Viewport Calculation
- [ ] Viewport with empty scrollback
- [ ] Viewport with scrollback smaller than screen
- [ ] Viewport with scrollback larger than screen
- [ ] Viewport with different terminal sizes
- [ ] Viewport after terminal resize

### Edge Cases
- [ ] Empty workspace submit
- [ ] Very long lines (wrapping)
- [ ] UTF-8 multi-byte characters in scrollback
- [ ] Emoji in scrollback
- [ ] Terminal width changes
- [ ] Maximum scrollback size
- [ ] Scrollback with thousands of lines

### Performance
- [ ] Rendering performance with large scrollback
- [ ] Scrolling performance with large scrollback
- [ ] Memory usage with large scrollback

---

## Test Files to Create/Update

### New Test Files Needed:
1. `tests/unit/render/render_combined_test.c` - Combined scrollback+workspace rendering
2. `tests/unit/render/render_separator_test.c` - Separator line rendering
3. `tests/unit/repl/repl_submit_test.c` - Workspace submission to scrollback
4. `tests/unit/repl/repl_multiline_test.c` - Multi-line workspace editing
5. `tests/unit/input/multiline_test.c` - Ctrl+J vs Enter parsing
6. `tests/unit/repl/repl_scrolling_test.c` - Page Up/Down scrolling

### Existing Test Files to Update:
1. `tests/unit/repl/repl_slash_command_test.c` - Update /pp tests for scrollback output
2. `tests/unit/repl/repl_combined_render_test.c` - Fix mock issues, add proper assertions
3. `tests/unit/repl/repl_scrollback_test.c` - Add viewport + separator tests

---

## Coverage Impact

**Before fixes**: Tests passed but didn't validate visible output
**After fixes**: ~300 new lines of code with 0% test coverage

**Estimated new test code needed**: ~500-800 lines
- Combined rendering: ~150 lines
- Separator line: ~100 lines
- Multi-line input: ~150 lines
- Scrollback submission: ~100 lines
- Scrolling behavior: ~200 lines
- Edge cases: ~100-200 lines

---

## Next Steps

1. **Manual Testing Session** - Identify remaining broken functionality
2. **Document Bugs** - Add to this file as discovered
3. **Prioritize Fixes** - Critical bugs first
4. **Write Tests** - TDD for each fix going forward
5. **Achieve 100% Coverage** - All new code must be tested

---

## Lessons Learned

- **Mock limitations**: Tests passed but didn't validate actual output (mocks weren't working)
- **Manual testing critical**: Automated tests missed major visible bugs
- **Coverage != Correctness**: 100% coverage doesn't mean functionality works
- **Integration testing needed**: Unit tests missed rendering pipeline issues
