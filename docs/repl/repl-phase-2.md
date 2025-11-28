# REPL Terminal - Phase 2: Complete REPL Event Loop

[← Back to REPL Terminal Overview](README.md)

**Goal**: Full interactive REPL with input buffer only (no scrollback).

This phase completed the functional REPL event loop, building on the direct rendering from Phase 1. It validates all the fundamentals before adding scrollback complexity.

## Implementation

### Render Frame Helper

**Function**: `ik_repl_render_frame(ik_repl_ctx_t *repl)`

**Logic**:
1. Get input buffer text and cursor position
2. Call `ik_render_input_buffer()` to render to terminal
3. Error handling

**Test Coverage**:
- Successful render
- Render with empty input buffer
- Render with multi-line text
- Render with cursor at various positions
- Error handling (write failure)

### Process Input Action Helper

**Function**: `ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)`

**Action Processing**:
- `IK_INPUT_CHAR` → `ik_input_buffer_insert_codepoint()`
- `IK_INPUT_NEWLINE` → `ik_input_buffer_insert_newline()`
- `IK_INPUT_BACKSPACE` → `ik_input_buffer_backspace()`
- `IK_INPUT_DELETE` → `ik_input_buffer_delete()`
- `IK_INPUT_ARROW_LEFT` → `ik_input_buffer_cursor_left()`
- `IK_INPUT_ARROW_RIGHT` → `ik_input_buffer_cursor_right()`
- `IK_INPUT_ARROW_UP` → `ik_input_buffer_cursor_up()`
- `IK_INPUT_ARROW_DOWN` → `ik_input_buffer_cursor_down()`
- `IK_INPUT_CTRL_A` → `ik_input_buffer_cursor_to_line_start()`
- `IK_INPUT_CTRL_E` → `ik_input_buffer_cursor_to_line_end()`
- `IK_INPUT_CTRL_K` → `ik_input_buffer_kill_to_line_end()`
- `IK_INPUT_CTRL_U` → `ik_input_buffer_kill_line()`
- `IK_INPUT_CTRL_W` → `ik_input_buffer_delete_word_backward()`
- `IK_INPUT_CTRL_C` → set quit flag

**Test Coverage**:
- Each action type
- Edge cases (backspace at start, delete at end, etc.)
- Quit flag setting

### Multi-line Cursor Movement

**Input Buffer Functions**:
```c
// Move cursor up one line (within input buffer)
res_t ik_input_buffer_cursor_up(ik_input_buffer_t *ws);

// Move cursor down one line (within input buffer)
res_t ik_input_buffer_cursor_down(ik_input_buffer_t *ws);
```

**Implementation Logic**:
- Tracks cursor as (byte_offset, grapheme_offset)
- Calculates current (row, col) position from cursor
- Up: Finds start of previous line, moves cursor to same column (or end if line shorter)
- Down: Finds start of next line, moves cursor to same column (or end if line shorter)
- Remembers preferred column when moving vertically
- Handles newlines and wrapped lines correctly

**Test Coverage**:
- Move up/down in multi-line text
- Move up from first line (no-op)
- Move down from last line (no-op)
- Column preservation when moving through lines of different lengths
- Movement through wrapped lines
- Movement through UTF-8 text (emoji, CJK)
- Edge cases: empty lines, cursor at start/end

### Readline-Style Editing Shortcuts

**Input Actions Added** (`src/input.h`):
```c
typedef enum {
    // ... existing actions ...
    IK_INPUT_CTRL_A,      // Move to beginning of line
    IK_INPUT_CTRL_E,      // Move to end of line
    IK_INPUT_CTRL_K,      // Kill to end of line
    IK_INPUT_CTRL_U,      // Kill entire line
    IK_INPUT_CTRL_W,      // Delete word backward
} ik_input_action_type_t;
```

**Input Buffer Functions** (`src/input_buffer.h`):
```c
// Move cursor to beginning of current line
res_t ik_input_buffer_cursor_to_line_start(ik_input_buffer_t *ws);

// Move cursor to end of current line
res_t ik_input_buffer_cursor_to_line_end(ik_input_buffer_t *ws);

// Delete from cursor to end of current line
res_t ik_input_buffer_kill_to_line_end(ik_input_buffer_t *ws);

// Delete entire current line
res_t ik_input_buffer_kill_line(ik_input_buffer_t *ws);

// Delete word backward from cursor
res_t ik_input_buffer_delete_word_backward(ik_input_buffer_t *ws);
```

**Implementation Logic**:
- Line start/end: Find previous/next newline, position cursor
- Kill to end: Delete from cursor to next newline (or end of text)
- Kill line: Delete entire line (from start to newline)
- Delete word backward: Scan back to find word boundary (whitespace/punctuation), delete
- All operations are UTF-8 aware
- Character class system (word/whitespace/punctuation) for proper word boundaries

**Test Coverage**:
- Each function with various cursor positions
- Edge cases: cursor at start/end, empty lines
- Multi-line text handling
- UTF-8 word boundaries
- Combination of operations

### Main Event Loop

**Function**: `ik_repl_run(ik_repl_ctx_t *repl)`

**Logic**:
1. Initial render
2. Main loop:
   - Read bytes from terminal
   - Parse bytes into actions
   - Process each action
   - Re-render frame
   - Check quit flag
3. Return OK or error

**Test Coverage**:
- Full event loop with mocked TTY input
- Multiple keystrokes in sequence
- Exit via Ctrl+C
- Error handling (read failure, render failure)

### Main Entry Point

**File**: `src/main.c`

```c
int main(void) {
    ik_repl_ctx_t *repl = NULL;
    res_t result = ik_repl_init(NULL, &repl);
    if (is_err(&result)) {
        fprintf(stderr, "Failed to initialize REPL\n");
        return 1;
    }

    result = ik_repl_run(repl);
    ik_repl_cleanup(repl);

    return is_ok(&result) ? 0 : 1;
}
```

### Manual Testing and Bugs Fixed

32 manual tests were performed covering:
- Launch and basic operation
- UTF-8 handling (emoji, combining chars, CJK)
- Cursor movement through multi-byte chars
- Text wrapping at terminal boundary
- Backspace/delete through wrapped text
- Insert in middle of wrapped line
- Multi-line input with newlines
- Arrow up/down cursor movement
- Column preservation
- Readline shortcuts (Ctrl+A/E/K/U/W)
- Ctrl+C exit and clean terminal restoration

**Bugs Found and Fixed**:
1. **CRITICAL** (commit 9b32cff): Empty input buffer crashes
   - Error: `Assertion 'text != NULL' failed`
   - Fix: Added NULL/empty checks to all 6 navigation/readline functions
2. **MEDIUM** (commit 3c226d3): Column preservation in multi-line navigation
   - Issue: Cursor returned to clamped position instead of original column
   - Fix: Added `target_column` field to input buffer structure
3. **LOW** (commits 3c226d3, 4f38c6b): Ctrl+W punctuation handling
   - Issue: Deleted "test." together instead of treating "." as separate boundary
   - Fix: Added character class system (word/whitespace/punctuation)

### Code Review and Refactoring

- Security/memory/error handling analysis (Grade: A-)
- 0 CRITICAL, 0 HIGH issues
- 2 MEDIUM issues documented (theoretical overflows, commit 025491e)
- Excellent security: no unsafe functions, proper UTF-8 validation
- Cursor module made input buffer-internal (void+assertions, -10 LCOV markers)
- Input buffer split: `input_buffer.c` + `input_buffer_multiline.c` (file size lint compliance)
- Test files modularized for maintainability

## Final Metrics

**Coverage**: 100%
- 1315 lines
- 105 functions
- 525 branches

**LCOV exclusions**: 162/164 (within limit)

**Quality gates**: All passing
- fmt ✅
- check ✅
- lint ✅
- coverage ✅
- check-dynamic (ASan/UBSan/TSan) ✅

## What Was Validated

- Complete REPL event loop with direct rendering
- Terminal raw mode and alternate screen
- UTF-8/grapheme handling (emoji, combining chars, CJK)
- Cursor position tracking through text edits
- Text insertion/deletion at arbitrary positions
- Multi-line text via newlines and wrapping
- Arrow key cursor movement (left/right/up/down)
- Readline-style editing shortcuts (Ctrl+A/E/K/U/W)
- Line-based navigation and deletion operations
- Word-aware deletion
- Clean terminal restoration on exit
