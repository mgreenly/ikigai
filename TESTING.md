# Manual Testing Guide for Phase 4 REPL

Run `./bin/ikigai` and follow these test steps. Mark each with ✅ (pass) or ❌ (fail with description).

## Test Session 1: Basic Functionality

### Test 1.1: Single Line Submit
```
Action: Type "hello world"
Expected: See "hello world" in workspace (bottom area)
Result: [ ]
```

```
Action: Press Enter
Expected:
  - "hello world" appears in scrollback (top area)
  - Workspace clears
  - Separator line (---) visible between scrollback and workspace
Result: [ ]
```

### Test 1.2: Multiple Line Submits
```
Action: Type "line 1" → Press Enter
Action: Type "line 2" → Press Enter
Action: Type "line 3" → Press Enter
Expected:
  - Scrollback shows:
    line 1
    line 2
    line 3
    ----------------
  - Workspace empty
Result: [ ]
```

### Test 1.3: Empty Line Submit
```
Action: Press Enter (without typing anything)
Expected: Empty line added to scrollback OR nothing happens
Result: [ ]
```

---

## Test Session 2: Multi-line Input (Ctrl+J)

### Test 2.1: Multi-line Creation
```
Action: Type "first"
Action: Press Ctrl+J (hold Ctrl, press J)
Expected: Cursor moves to new line in workspace (no submit)
Result: [ ]
```

```
Action: Type "second"
Action: Press Ctrl+J
Action: Type "third"
Expected: Workspace shows:
  first
  second
  third█
Result: [ ]
```

### Test 2.2: Multi-line Submit
```
Action: Press Enter
Expected: All 3 lines move to scrollback as one entry
Result: [ ]
```

### Test 2.3: Multi-line Navigation
```
Action: Type "line1" → Ctrl+J → Type "line2" → Ctrl+J → Type "line3"
Action: Press Up Arrow
Expected: Cursor moves to "line2"
Result: [ ]
```

```
Action: Press Up Arrow again
Expected: Cursor moves to "line1"
Result: [ ]
```

```
Action: Press Down Arrow
Expected: Cursor moves back to "line2"
Result: [ ]
```

---

## Test Session 3: Scrollback Scrolling

### Test 3.1: Build Large Scrollback
```
Action: Submit 30 lines (type "line X" → Enter, repeat 30 times)
Expected: Scrollback fills, oldest lines scroll off top
Result: [ ]
```

### Test 3.2: Page Up Scrolling
```
Action: Press Page Up
Expected: View scrolls up, showing older lines
Result: [ ]
```

```
Action: Press Page Up 5 more times
Expected: Continue scrolling through history
Result: [ ]
```

### Test 3.3: Page Down Scrolling
```
Action: Press Page Down
Expected: Scroll back down toward bottom
Result: [ ]
```

```
Action: Press Page Down repeatedly until at bottom
Expected: Return to latest view (newest lines visible)
Result: [ ]
```

### Test 3.4: Scroll State While Typing
```
Action: Page Up 3 times (scroll up)
Action: Type "new text" in workspace
Expected: View stays scrolled up OR auto-scrolls to bottom?
Result: [ ]
```

```
Action: Press Enter to submit
Expected: View auto-scrolls to bottom, showing new submission
Result: [ ]
```

---

## Test Session 4: Commands

### Test 4.1: /pp Command
```
Action: Type "/pp"
Action: Press Enter
Expected: Workspace debug output appears in scrollback
Result: [ ]
```

### Test 4.2: Unknown Command
```
Action: Type "/unknown"
Action: Press Enter
Expected: Line "/unknown" appears in scrollback (no error)
Result: [ ]
```

---

## Test Session 5: Editing Features

### Test 5.1: Readline Shortcuts
```
Action: Type "hello world"
Action: Press Ctrl+A
Expected: Cursor jumps to start of line
Result: [ ]
```

```
Action: Press Ctrl+E
Expected: Cursor jumps to end of line
Result: [ ]
```

```
Action: Press Ctrl+K
Expected: Text from cursor to end deleted
Result: [ ]
```

```
Action: Type "test" → Press Ctrl+U
Expected: Entire line deleted
Result: [ ]
```

```
Action: Type "one two three" → Press Ctrl+W
Expected: "three" deleted (last word)
Result: [ ]
```

### Test 5.2: Arrow Key Navigation
```
Action: Type "hello world"
Action: Press Left Arrow 5 times
Expected: Cursor in middle of "world"
Result: [ ]
```

```
Action: Press Right Arrow 2 times
Expected: Cursor moves right
Result: [ ]
```

### Test 5.3: Backspace/Delete
```
Action: Type "hello world"
Action: Press Backspace 5 times
Expected: "world" deleted
Result: [ ]
```

```
Action: Press Left Arrow 3 times (cursor in "hel█lo")
Action: Press Delete
Expected: "l" deleted → "helo"
Result: [ ]
```

---

## Test Session 6: Edge Cases

### Test 6.1: UTF-8 Characters
```
Action: Type "Hello 😀 World"
Action: Press Enter
Expected: Emoji displays correctly in scrollback
Result: [ ]
```

```
Action: Type "日本語テスト"
Action: Press Enter
Expected: Japanese characters display in scrollback
Result: [ ]
```

### Test 6.2: Very Long Lines
```
Action: Type 200 character line (repeat "abcdefghij" 20 times)
Expected: Line wraps at terminal width
Result: [ ]
```

```
Action: Press Enter
Expected: Wrapped line appears in scrollback correctly
Result: [ ]
```

### Test 6.3: Separator Line Width
```
Action: Note terminal width (default 80 chars)
Action: Submit a few lines
Expected: Separator is exactly terminal width (full line of dashes)
Result: [ ]
```

### Test 6.4: Cursor Positioning
```
Action: Type "test"
Expected: Cursor visible immediately after last character
Result: [ ]
```

```
Action: Type multi-line (line1 Ctrl+J line2)
Expected: Cursor on second line after "line2"
Result: [ ]
```

---

## Test Session 7: Exit

### Test 7.1: Clean Exit
```
Action: Press Ctrl+C
Expected:
  - REPL exits cleanly
  - Terminal restored (no garbage, cursor visible, normal mode)
  - No segfault or crash
Result: [ ]
```

---

## Results Summary

After completing tests, fill in:

**Total Tests**: [ ]
**Passed**: [ ]
**Failed**: [ ]

**Critical Bugs Found**:
1.
2.
3.

**Minor Issues**:
1.
2.

**Notes/Observations**:


---

## How to Report Results

For each failed test, note:
- What you did
- What you expected
- What actually happened
- Any error messages or crashes

Example:
```
Test 3.2 FAILED ❌
Action: Pressed Page Up
Expected: Scrollback scrolls up
Actual: Nothing happened, view didn't change
```
