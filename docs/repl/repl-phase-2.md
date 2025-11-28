# REPL Terminal - Phase 2: Complete REPL Event Loop

[← Back to REPL Terminal Overview](README.md)

**Goal**: Full interactive REPL with input buffer only (no scrollback).

Complete the Phase 1 functionality from the original plan with direct rendering. Build working REPL event loop that validates all the fundamentals before adding scrollback complexity.

## Implementation Tasks

### Task 1: Render Frame Helper

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

### Task 2: Process Input Action Helper

**Function**: `ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)`

**Action Processing**:
- `IK_INPUT_CHAR` → `ik_input_buffer_insert_codepoint()`
- `IK_INPUT_NEWLINE` → `ik_input_buffer_insert_newline()`
- `IK_INPUT_BACKSPACE` → `ik_input_buffer_backspace()`
- `IK_INPUT_DELETE` → `ik_input_buffer_delete()`
- `IK_INPUT_ARROW_LEFT` → `ik_input_buffer_cursor_left()`
- `IK_INPUT_ARROW_RIGHT` → `ik_input_buffer_cursor_right()`
- `IK_INPUT_ARROW_UP` → `ik_input_buffer_cursor_up()` (added in Task 2.5)
- `IK_INPUT_ARROW_DOWN` → `ik_input_buffer_cursor_down()` (added in Task 2.5)
- `IK_INPUT_CTRL_A` → `ik_input_buffer_cursor_to_line_start()` (added in Task 2.6)
- `IK_INPUT_CTRL_E` → `ik_input_buffer_cursor_to_line_end()` (added in Task 2.6)
- `IK_INPUT_CTRL_K` → `ik_input_buffer_kill_to_line_end()` (added in Task 2.6)
- `IK_INPUT_CTRL_U` → `ik_input_buffer_kill_line()` (added in Task 2.6)
- `IK_INPUT_CTRL_W` → `ik_input_buffer_delete_word_backward()` (added in Task 2.6)
- `IK_INPUT_CTRL_C` → set quit flag

**Test Coverage**:
- Each action type
- Edge cases (backspace at start, delete at end, etc.)
- Quit flag setting

### Task 2.5: Multi-line Cursor Movement

**Goal**: Enable up/down arrow keys for cursor movement within multi-line input buffer.

**Add to** `src/input.h`:
- Input actions already exist: `IK_INPUT_ARROW_UP`, `IK_INPUT_ARROW_DOWN`
- No changes needed to input module

**Add to** `src/input_buffer.h`:
```c
// Move cursor up one line (within input buffer)
res_t ik_input_buffer_cursor_up(ik_input_buffer_t *ws);

// Move cursor down one line (within input buffer)
res_t ik_input_buffer_cursor_down(ik_input_buffer_t *ws);
```

**Implementation Logic**:
- Track cursor as (byte_offset, grapheme_offset) - existing
- Need to calculate current (row, col) position from cursor
- Up: Find start of previous line, move cursor to same column (or end if line shorter)
- Down: Find start of next line, move cursor to same column (or end if line shorter)
- Handle newlines and wrapped lines correctly
- Remember preferred column when moving vertically

**Test Coverage**:
- Move up/down in multi-line text
- Move up from first line (no-op)
- Move down from last line (no-op)
- Column preservation when moving through lines of different lengths
- Movement through wrapped lines
- Movement through UTF-8 text (emoji, CJK)
- Edge cases: empty lines, cursor at start/end

### Task 2.6: Readline-Style Editing Shortcuts

**Goal**: Add common readline-style keyboard shortcuts for efficient editing.

**Add to** `src/input.h`:
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

**Update** `src/input.c`:
- Parse Ctrl+A, Ctrl+E, Ctrl+K, Ctrl+U, Ctrl+W
- Return appropriate action types

**Add to** `src/input_buffer.h`:
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
- All operations must be UTF-8 aware

**Test Coverage**:
- Each function with various cursor positions
- Edge cases: cursor at start/end, empty lines
- Multi-line text handling
- UTF-8 word boundaries
- Combination of operations

### Task 3: Main Event Loop

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

### Task 4: Main Entry Point

**Update** `src/main.c`:
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

**Rename**: `src/client.c` → `src/main.c` (if not already done)

### Task 5: Manual Testing and Polish

**Manual Testing Checklist** (run `./ikigai`):
- [ ] Launch and basic operation
- [ ] UTF-8 handling (emoji, combining chars, CJK)
- [ ] Cursor movement through multi-byte chars
- [ ] Text wrapping at terminal boundary
- [ ] Backspace/delete through wrapped text
- [ ] Insert in middle of wrapped line
- [ ] Multi-line input with newlines
- [ ] Arrow up/down cursor movement in multi-line text
- [ ] Column preservation when moving up/down
- [ ] Ctrl+A (beginning of line)
- [ ] Ctrl+E (end of line)
- [ ] Ctrl+K (kill to end of line)
- [ ] Ctrl+U (kill entire line)
- [ ] Ctrl+W (delete word backward)
- [ ] Ctrl+C exit and clean terminal restoration

**Polish**:
- Run `make fmt`
- Run all quality checks
- Fix any issues discovered during manual testing

## What We Validate

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

## What We Defer

- Scrollback buffer (comes in Phase 3)
- Viewport scrolling (comes in Phase 4)
- Separator line (comes in Phase 4)
- Page Up/Down input for scrollback navigation (comes in Phase 4)
- Line submission to history (comes in Phase 4)
- Command history with Up/Down navigation (comes in Phase 4)

## Phase 2 Complete When

- [ ] Render frame helper implemented with tests
- [ ] Process action helper implemented with tests
- [ ] Multi-line cursor movement implemented with tests (Task 2.5)
- [ ] Readline-style shortcuts implemented with tests (Task 2.6)
- [ ] Main event loop implemented with tests
- [ ] main.c entry point updated
- [ ] 100% test coverage maintained
- [ ] Manual testing checklist passes
- [ ] `make check && make lint && make coverage` all pass

## Development Approach

Strict TDD with 100% coverage requirement.
