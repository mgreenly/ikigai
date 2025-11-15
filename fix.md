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

### 10. Bug #4: Incorrect Viewport Model - Separator/Edit Zone Should Scroll (FIXED - 2025-11-14)
**Problem**: Separator and edit zone stayed "sticky" at bottom when scrolling
- Old: Scrollback viewport moved, separator/workspace stayed fixed at bottom
- Expected: Entire document (scrollback + separator + workspace) should scroll as one unit

**Fix**: Implemented unified document scrolling model (src/render.c, src/repl.c)
- Document is now: SCROLLBACK → SEPARATOR → EDITZONE (all scroll together)
- Viewport calculation: Document height = scrollback_lines + 1 (separator) + workspace_lines
- Rendering logic: Calculate which lines of total document are visible
- Render only visible portions (separator/workspace may be off-screen when scrolled up)
- Cursor positioning: Accounts for document scroll position

**Key principle:** Last line of scrollback ALWAYS appears directly above separator

**Files changed:**
- `src/render.c` - `ik_render_combined()` completely rewritten for unified scrolling
- `src/repl.c` - Viewport calculation and cursor positioning updated
- All scrolling tests updated

---

## Known Issues Still To Test

### Bug #5: Terminal Resize Not Immediate - NOT A BUG
**Status**: Manual testing (2025-11-14) confirmed this works correctly
- Terminal resize triggers immediate redraw
- Separator width updates immediately
- Text reflows correctly
- No keypress needed to see changes
- This bug does not exist

---

### Bug #6: No Auto-Scroll on Typing When Scrolled Up (FIXED - 2025-11-14)
**Problem**: Typing while scrolled up doesn't auto-scroll to show cursor
- Current: Scroll position stays where it is, user types "blind"
- Expected: Any typing should auto-scroll to bottom to show cursor/workspace

**What happens:**
1. User scrolls up with Page Up to view scrollback
2. User starts typing
3. Scroll position doesn't change - cursor and workspace remain off-screen
4. User types without seeing what they're typing

**Expected behavior:**
1. User scrolls up with Page Up
2. User starts typing (any insert/delete/navigation action)
3. Viewport immediately scrolls to bottom
4. Cursor and workspace visible, user sees what they're typing

**Root cause:**
- Workspace editing actions didn't trigger viewport reset
- `viewport_offset` stayed at scrolled position
- Needed to detect workspace modifications and reset to `viewport_offset = 0`

**Fix**: Auto-scroll to bottom on workspace actions (src/repl_actions.c)
- Added `repl->viewport_offset = 0;` before all workspace-modifying actions
- Actions that auto-scroll: insert char, insert newline (Ctrl+J), backspace, delete, cursor navigation (arrows, Ctrl+A/E), kill operations (Ctrl+K/U/W)
- Actions that DON'T auto-scroll: Page Up/Down (scrolling controls), Ctrl+C (quit), Enter (already handled by submit_line)

**Test**: Added comprehensive auto-scroll tests (tests/unit/repl/repl_autoscroll_test.c)
- test_autoscroll_on_char_insert - typing characters
- test_autoscroll_on_insert_newline - Ctrl+J multi-line newline
- test_autoscroll_on_backspace/delete - deletion operations
- test_autoscroll_on_cursor_navigation - arrow keys (left/right/up/down)
- test_autoscroll_on_ctrl_shortcuts - Ctrl+A/E/K/U/W
- test_no_autoscroll_on_page_up/down - verify scrolling controls work
- All tests PASS with 100% coverage

---

## Known Issues Still To Test

### Bug #7: Cursor Visible When Workspace Scrolled Off-Screen (FIXED - 2025-11-14)
**Problem**: Cursor renders and overwrites last line of scrollback when scrolled up
- Current: Cursor positioned even when workspace not visible, erases last scrollback line
- Expected: Cursor should be hidden when workspace is scrolled off-screen

**What happens:**
1. User types many lines to populate scrollback
2. User scrolls up with Page Up to view scrollback
3. Cursor remains visible at its calculated position
4. Last visible line of scrollback is overwritten by blank cursor line

**Expected behavior:**
1. User scrolls up with Page Up
2. Cursor is hidden (workspace is off-screen)
3. All scrollback lines remain visible without cursor interference

**Root cause:**
- `src/render.c:470-480` - Cursor positioning escape always written
- Cursor escape written even when `render_workspace = false`
- Should only position cursor when workspace is visible

**Fix**: Conditional cursor positioning (src/render.c:471-482)
- Wrapped cursor positioning code in `if (render_workspace)` conditional
- Cursor escape only written when workspace is visible on screen
- When scrolled up (workspace off-screen), no cursor positioning occurs
- Last scrollback line remains fully visible without cursor interference

**Test**: Added comprehensive cursor visibility tests (tests/unit/render/render_cursor_visibility_test.c)
- `test_cursor_hidden_when_workspace_off_screen` - verifies no cursor escape when render_workspace=false
- `test_cursor_visible_when_workspace_on_screen` - verifies cursor escape present when render_workspace=true
- `test_last_scrollback_line_visible_when_scrolled_up` - verifies scrollback content not overwritten
- All tests PASS with 100% coverage

---

### Bug #8: Cursor Still Visible at Column Zero When Scrolled Up (FIXED - 2025-11-14)
**Problem**: Cursor remains visible at column zero when workspace is scrolled off-screen
- Bug #7 fixed cursor positioning escape, but cursor is still visible
- Expected: Cursor should be completely hidden when workspace is off-screen
- Entire viewport should display scrollback without any visible cursor

**What happens:**
1. User scrolls up with Page Up to view scrollback
2. Workspace is off-screen (render_workspace = false)
3. Cursor positioning escape is not written (Bug #7 fix working)
4. BUT cursor is still visible at column zero of some row
5. Cursor presence is distracting and shouldn't be visible

**Expected behavior:**
1. User scrolls up with Page Up
2. Cursor is completely hidden (not visible anywhere)
3. Entire viewport displays scrollback content
4. When scrolling back down or typing, cursor reappears

**Root cause:**
- Cursor visibility is not explicitly controlled
- Terminal cursor is still visible even without positioning escape
- Need to use ANSI cursor visibility control sequences

**Fix**: Add ANSI cursor visibility control sequences (src/render.c:470-487)
- Use `\x1b[?25l` to hide cursor when workspace is off-screen
- Use `\x1b[?25h` to show cursor when workspace is visible
- Implemented in `ik_render_combined()` based on `render_workspace` flag
- Cursor visibility escape always written before cursor positioning
- When `render_workspace = true`: show cursor then position it
- When `render_workspace = false`: hide cursor (no positioning needed)

**Test**: Added comprehensive cursor visibility tests (tests/unit/render/render_cursor_visibility_test.c)
- `test_cursor_visibility_escape_hide_when_off_screen` - verifies `\x1b[?25l` when workspace off-screen
- `test_cursor_visibility_escape_show_when_on_screen` - verifies `\x1b[?25h` when workspace on-screen
- Tests validate exact escape sequences in output buffer
- All tests PASS with 100% coverage

---

### Bug #9: Separator Missing When Last Visible Line (FIXED - 2025-11-15)
**Problem**: When the separator should be the last visible line, it was missing
- When scrolled up so separator is at last viewport row, it wasn't rendered
- Expected: Separator visible at bottom of screen when workspace just off-screen
- Actual: Separator appeared briefly then disappeared (too fast to see)

**Root Cause**: Terminal scrolling bug in `ik_render_combined()` (src/render.c)
- Separator was being rendered with trailing `\r\n` (lines 451-452)
- When separator is on the LAST physical row of terminal, adding `\r\n` causes terminal to scroll
- The scroll pushes the separator off the top of the screen
- Separator vanishes immediately after being rendered

**Technical Details**:
When writing to last terminal row:
1. Terminal displays the separator line (80 dashes)
2. `\r\n` advances cursor to next row (beyond terminal height)
3. Terminal scrolls entire screen up by 1 line
4. Separator (which was on last row) moves off-screen
5. Last row becomes blank

**Fix**: Conditional `\r\n` after separator (src/render.c:452-457)
- Only add `\r\n` after separator when `render_workspace` is true
- When workspace is off-screen, separator is last content rendered
- No newline needed after final content
- Prevents terminal scroll when separator is on last row

**Code Change**:
```c
// Old code - always added \r\n:
framebuffer[offset++] = '\r';
framebuffer[offset++] = '\n';

// New code - conditional:
if (render_workspace) {
    framebuffer[offset++] = '\r';
    framebuffer[offset++] = '\n';
}
```

**Test**: `tests/unit/render/render_separator_terminal_scroll_test.c`
- `test_separator_no_trailing_newline_when_last_line` - Verifies no `\r\n` when workspace off-screen
- `test_separator_has_trailing_newline_when_workspace_visible` - Verifies `\r\n` present when workspace visible
- Both tests pass with 100% coverage

**Key Insight**: Terminal scrolling is a side effect of writing beyond screen bounds. Last line content must not have trailing newlines.

---

### Bug #10: Page Up Doesn't Scroll to Show Earlier Scrollback Lines (FIXED - 2025-11-15)
**Problem**: After pressing Page Up, the earliest scrollback line doesn't become visible
- User scenario: 5-row terminal, after typing a,b,c,d,e (each with Enter), then Page Up
- Expected: Should show all lines a,b,c,d,e
- Actual: Only showed b,c,d,e and a blank line (missing 'a')
- The earliest line never became visible after scrolling up

**Root Causes**: Two separate bugs working together:

**Root Cause #1: Empty Workspace Counted as 1 Physical Line**
- `ik_workspace_calculate_layout()` in `src/workspace_layout.c:65` returned 1 for empty workspace
- Should return 0 physical lines when workspace is empty
- This made document height calculation wrong:
  - Actual document: 4 scrollback + 1 separator + 0 workspace = 5 rows
  - Calculated as: 4 scrollback + 1 separator + 1 workspace = 6 rows
- This offset caused viewport calculation to show rows 1-5 instead of 0-4

**Root Cause #2: Terminal Scrolling from Trailing Newlines**
- Similar to Bug #9, but for scrollback lines instead of separator
- When last visible line is a scrollback line AND separator/workspace are off-screen
- Adding `\r\n` after that last scrollback line causes terminal to scroll
- The scroll pushes the top line off-screen

**Fixes Applied:**

**Fix #1: Empty Workspace = 0 Physical Lines** (`src/workspace_layout.c:65`)
```c
// Old code:
if (text == NULL || text_len == 0) {
    workspace->physical_lines = 1;  // WRONG
    ...
}

// New code:
if (text == NULL || text_len == 0) {
    workspace->physical_lines = 0;  // CORRECT (Bug #10 fix)
    ...
}
```

**Fix #2: Conditional Trailing Newlines** (`src/render.c:441-450`)
```c
// Don't add \r\n after last scrollback line if separator/workspace both off-screen
bool is_last_scrollback_line = (i == scrollback_end_line - 1);
bool nothing_after = !render_separator && !render_workspace;
if (!is_last_scrollback_line || !nothing_after) {
    framebuffer[offset++] = '\r';
    framebuffer[offset++] = '\n';
}
```

**Tests Created:**
- `tests/unit/repl/repl_page_up_after_typing_test.c` - Reproduces exact user scenario (type a,b,c,d,e, Page Up, verify all visible)

**Tests Updated** (to match new correct behavior):
- `tests/unit/repl/repl_exact_user_scenario_test.c:64-102` - Empty workspace means entire document fits
- `tests/unit/repl/repl_page_up_scrollback_test.c:79-134` - Adjusted for 0 physical lines for empty workspace
- `tests/unit/workspace/layout_cache_test.c:171` - Changed assertion from `physical_lines == 1` to `physical_lines == 0`

**Key Insight:** Empty workspace should have zero height in the document model. This affects viewport calculation, scrolling behavior, and separator visibility. Both the workspace layout calculation AND the terminal scrolling prevention needed fixes

---

## Known Issues Still To Test

### Scrolling
- [x] Page Up scrolling - Works correctly
- [x] Page Down scrolling - Works correctly
- [x] Scrolling to top (boundary) - Works correctly
- [x] Scrolling to bottom (boundary) - Works correctly
- [x] Scroll state persistence while typing - FAILS (Bug #6)
- [x] Auto-scroll to bottom on submit - Works correctly

### Viewport Calculation
- [x] Viewport with empty scrollback - Works correctly
- [x] Viewport with scrollback smaller than screen - Works correctly
- [x] Viewport with scrollback larger than screen - Works correctly
- [x] Viewport with different terminal sizes - Works correctly
- [x] Viewport after terminal resize - Works correctly (Bug #5 appears to not exist)

### Edge Cases
- [x] Empty workspace submit - Works correctly
- [x] Very long lines (wrapping) - Works correctly
- [x] UTF-8 multi-byte characters in scrollback - Works correctly
- [x] Emoji in scrollback - Works correctly
- [x] Terminal width changes - Works correctly
- [x] Maximum scrollback size - Works correctly
- [x] Scrollback with thousands of lines - Works correctly

### Performance
- [x] Rendering performance with large scrollback - Works correctly (tested with thousands of lines)
- [x] Scrolling performance with large scrollback - Works correctly (tested with thousands of lines)
- [x] Memory usage with large scrollback - Works correctly (reasonable usage, no leaks)

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
