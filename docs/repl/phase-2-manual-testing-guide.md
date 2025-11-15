# Phase 2 Manual Testing Guide

**Purpose**: Verify REPL interactive functionality before Phase 2 completion

**Prerequisites**: `make all` (builds `bin/ikigai`)

**Test Session Info**:
- Date: _____________
- Tester: _____________
- Terminal: _____________ (size: _____×_____)

**Instructions**: For each test, mark [✓] PASS or [✗] FAIL and add notes if needed.

---

## Section 1: Basic Functionality (6 tests)

### 1.1 Launch and Exit
- Run `./bin/ikigai`, verify clean prompt
- Press Ctrl+C, verify terminal restores
- [ ] Result: _____ Notes: _____

### 1.2 Simple Text Entry
- Type `hello world`, verify display
- Exit with Ctrl+C
- [ ] Result: _____ Notes: _____

### 1.3 Newline (Enter)
- Type `line 1`, press Enter
- Type `line 2`, verify both lines visible
- Exit
- [ ] Result: _____ Notes: _____

### 1.4 Backspace
- Type `hello`, press Backspace twice → `hel`
- Type `p` → `help`
- Exit
- [ ] Result: _____ Notes: _____

### 1.5 Delete Key
- Type `hello`, Left Arrow 3 times (at 'l')
- Press Delete → `helo`
- Exit
- [ ] Result: _____ Notes: _____

### 1.6 Arrow Keys (Left/Right)
- Type `test`, Left Arrow 2 times (at 's')
- Type `X` → `teXst`
- Right Arrow 2 times (at end)
- Exit
- [ ] Result: _____ Notes: _____

---

## Section 2: UTF-8 Support (4 tests)

### 2.1 Emoji Input
- Type `Hello 😀 World 👍`
- Verify emoji display, navigate with arrows
- Verify cursor skips emoji as single unit
- [ ] Result: _____ Notes: _____

### 2.2 CJK Characters
- Type `你好 こんにちは 한글 hello`
- Verify display (CJK = 2 cells wide)
- Navigate with arrows, verify cursor positioning
- [ ] Result: _____ Notes: _____

### 2.3 Combining Characters
- Type `café naïve résumé`
- Verify accented chars display correctly
- Navigate, verify cursor positions
- [ ] Result: _____ Notes: _____

### 2.4 Mixed UTF-8 Editing
- Type `Hello 世界 🎉`
- Navigate, insert text, backspace through emoji/CJK
- Verify all operations work correctly
- [ ] Result: _____ Notes: _____

---

## Section 3: Text Wrapping (3 tests)

### 3.1 Basic Wrapping
- Type very long line (>terminal width)
- Verify wraps to next physical line, cursor tracks correctly
- [ ] Result: _____ Notes: _____

### 3.2 Editing Wrapped Text
- Type long line (>80 chars) that wraps
- Navigate to middle, insert `INSERTED`
- Verify wrapping adjusts, delete text, verify re-wraps
- [ ] Result: _____ Notes: _____

### 3.3 Cursor Through Wrap Boundary
- Type line that wraps at column 80
- Position cursor before boundary, Right Arrow to cross
- Verify moves to next physical line, Left Arrow back
- [ ] Result: _____ Notes: _____

---

## Section 4: Multi-line Editing (4 tests)

### 4.1 Arrow Up/Down Navigation
- Type 3 lines: `first line` / `second line` / `third line`
- Arrow Up twice → at first line
- Arrow Down twice → at third line
- [ ] Result: _____ Notes: _____

### 4.2 Column Preservation
- Type: `short` / `this is a much longer line` / `tiny`
- Position cursor at column 10 of long line
- Arrow Up → cursor at end of "short" (clamped)
- Arrow Down → cursor returns to column 10
- [ ] Result: _____ Notes: _____

### 4.3 Boundary Conditions
- Type 2 lines: `first` / `second`
- At first line: Arrow Up (no-op, no crash)
- At last line: Arrow Down (no-op, no crash)
- [ ] Result: _____ Notes: _____

### 4.4 Different Line Lengths
- Type 5 lines of varying lengths (1 char, 10 chars, 3 chars, 40 chars, 1 char)
- Navigate up/down through all, verify cursor positioning
- [ ] Result: _____ Notes: _____

---

## Section 5: Readline Shortcuts (6 tests)

### 5.1 Ctrl+A (Beginning of Line)
- Type `hello world`, press Ctrl+A (cursor at start)
- Type `X` → `Xhello world`
- [ ] Result: _____ Notes: _____

### 5.2 Ctrl+E (End of Line)
- Type `hello world`, Left Arrow 5 times
- Ctrl+E (cursor at end), type `!` → `hello world!`
- [ ] Result: _____ Notes: _____

### 5.3 Ctrl+K (Kill to End)
- Type `hello world`, Left Arrow 5 times (at 'w')
- Ctrl+K → `hello ` (killed "world")
- [ ] Result: _____ Notes: _____

### 5.4 Ctrl+U (Kill Line)
- Type 3 lines, navigate to middle line
- Ctrl+U → middle line cleared, others remain
- [ ] Result: _____ Notes: _____

### 5.5 Ctrl+W (Delete Word Backward)
- Type `the quick brown fox`
- Ctrl+W → `the quick brown `
- Ctrl+W → `the quick `
- [ ] Result: _____ Notes: _____

### 5.6 Ctrl+W with Punctuation
- Type `hello-world_test.txt`
- Ctrl+W → `hello-world_test.` (deleted "txt")
- Ctrl+W → `hello-world_test` (deleted ".")
- [ ] Result: _____ Notes: _____

---

## Section 6: Edge Cases (6 tests)

### 6.1 Empty Input Buffer
- Launch, without typing try: arrows, backspace, delete, Ctrl+A/E/K/U/W
- Verify no crashes, all no-ops work
- [ ] Result: _____ Notes: _____

### 6.2 Very Long Line
- Type/paste line >500 characters
- Verify wraps correctly, navigate through, edit middle
- [ ] Result: _____ Notes: _____

### 6.3 Many Lines
- Type/paste >50 lines
- Verify displays (scrolling not yet implemented, shouldn't crash)
- Navigate up/down
- [ ] Result: _____ Notes: _____

### 6.4 Rapid Typing
- Type very quickly without pauses
- Verify all characters captured, no visual glitches
- [ ] Result: _____ Notes: _____

### 6.5 Rapid Arrow Keys
- Type `hello world`
- Hold Left Arrow, then hold Right Arrow
- Verify cursor moves correctly, no crashes
- [ ] Result: _____ Notes: _____

### 6.6 Rapid Backspace
- Type long text
- Hold Backspace until empty
- Verify clean deletion, no crashes
- [ ] Result: _____ Notes: _____

---

## Section 7: Terminal Restoration (3 tests)

### 7.1 Normal Exit
- Launch, type text, Ctrl+C
- Verify terminal normal mode, test with `echo "test"`
- [ ] Result: _____ Notes: _____

### 7.2 Terminal Resize
- Launch, type text, resize window
- Verify no crash (behavior may be undefined)
- Exit, verify restoration
- [ ] Result: _____ Notes: _____

### 7.3 Terminal Switch
- Launch, type text, switch to another window (Alt+Tab)
- Switch back, verify display intact, continue typing
- Exit
- [ ] Result: _____ Notes: _____

---

## Summary

**Total Tests**: 32
**Passed**: 32 / 32 ✅
**Failed**: 0 / 32
**Pass Rate**: 100%

**All Issues Fixed** (2025-11-11):
1. **Test 6.1 - FIXED**: Empty input buffer crashes (commit 9b32cff)
   - Error: `Assertion 'text != NULL' failed` in `src/input_buffer_multiline.c:17`
   - Fix: Added NULL/empty checks to all 6 navigation/readline functions
   - Status: ✅ FIXED

2. **Test 4.2 - FIXED**: Column preservation in multi-line navigation (commit 3c226d3)
   - Issue: Cursor returned to clamped position instead of original column
   - Fix: Added `target_column` field to input buffer structure
   - Status: ✅ FIXED

3. **Test 5.6 - FIXED**: Ctrl+W punctuation handling (commits 3c226d3, 4f38c6b)
   - Issue: Deleted "test." together instead of treating "." as separate boundary
   - Fix: Added character class system (word/whitespace/punctuation)
   - Status: ✅ FIXED

**Terminal Compatibility Notes**:
- Tested on: _____________ (terminal emulator)
- Terminal size: _____________
- All tests run on 2025-11-11

**Overall Assessment**:
- [X] READY FOR PHASE 3 (all tests pass)
- [ ] NEEDS FIXES (no failures)

**Test Results by Section** (Post-Fix):
- Section 1 (Basic): 6/6 ✅
- Section 2 (UTF-8): 4/4 ✅
- Section 3 (Wrapping): 3/3 ✅
- Section 4 (Multi-line): 4/4 ✅ (bug #2 fixed)
- Section 5 (Readline): 6/6 ✅ (bug #3 fixed)
- Section 6 (Edge Cases): 6/6 ✅ (bug #1 fixed)
- Section 7 (Restoration): 3/3 ✅

**Sign-off**: _____________ (Date: 2025-11-11)

---

## Quick Test Reference

**Fast smoke test** (5 min):
- Tests: 1.1, 1.2, 1.3, 1.4, 1.6, 2.1, 4.1, 5.1, 5.5, 7.1

**Full validation** (20-30 min):
- All 32 tests above

**Focus areas**:
- UTF-8: Critical for international users (Section 2)
- Wrapping: Terminal width handling (Section 3)
- Shortcuts: Productivity features (Section 5)
- Restoration: Terminal hygiene (Section 7)
