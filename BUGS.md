# Bugs Found During Manual Testing

## Bug #1: Initial Render - No Separator Line

**Current Behavior:**
- Program starts blank
- Cursor at position 0,0 (top-left)
- No separator line visible

**Expected Behavior:**
- Horizontal separator line visible at row 0 (even with empty scrollback)
- Cursor positioned at row 1, col 0 (below separator, ready for input)

**After first submit:**
- Text appears above separator ✓
- Separator visible ✓
- Cursor below separator ✓

**Root Cause:**
Separator only renders when `scrollback_line_count > 0` (src/render.c:441)

**Fix Required:**
Always render separator line (even with empty scrollback)

---

## Manual Test Results

### ✅ WORKING:
- Scrollback accumulation (multiple lines)
- Scrollback scrolls off top when screen fills
- Input workspace stays at bottom
- Page Up/Down scrolling
- Multi-line input (Ctrl+J)
- Separator line (after first submit)
- /pp command outputs workspace debug info
- Enter submits to scrollback correctly
- UTF-8/Emoji display correctly (😀 tested)
- Readline shortcuts: Ctrl+A, Ctrl+E, Ctrl+K, Ctrl+U, Ctrl+W all work

### ❌ BUGS:
1. Initial render - no separator line, cursor wrong position
2. Slash commands echo to scrollback - should only show output, not command itself
   - Currently: `/pp` shows workspace debug info AND "/pp" text
   - Expected: Only show workspace debug info
3. **CRITICAL: Crash after Ctrl+U (both single and multi-line)**
   - Reproduction:
     1. Type any text (single or multi-line)
     2. Press Ctrl+U (kills current line)
     3. Press any key → CRASH
   - Error: `Assertion 'index <= array->size' failed` in src/array.c:94
   - Root cause: Cursor position not reset correctly after kill_line deletes text
   - Note: Ctrl+K (kill to end) works fine - only Ctrl+U (kill line) crashes
   - Affected function: `ik_workspace_kill_line()` in src/workspace_multiline.c:366

---
