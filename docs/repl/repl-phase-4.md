# REPL Terminal - Phase 4: Viewport and Scrolling

[← Back to REPL Terminal Overview](README.md)

**Goal**: Integrate scrollback with REPL, add viewport calculation and scrolling commands.

**Status**: In Progress (Tasks 4.1-4.4 complete, 4.5-4.6 pending)

## Rationale

Phase 3 provides the scrollback buffer module with layout caching. Phase 4 integrates it into the REPL event loop, adding the ability to view historical output and scroll through it.

This completes the split-buffer terminal interface: scrollback history above, input buffer below, with user control over viewport position.

## Implementation Tasks

### Task 4.1: Integrate Scrollback with REPL ✅ COMPLETE

**Goal**: Add scrollback buffer to REPL context and connect it to rendering.

**Modify** `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;
    ik_render_ctx_t *render;
    ik_input_buffer_t *input_buffer;
    ik_input_parser_t *input_parser;
    ik_scrollback_t *scrollback;      // NEW: Scrollback buffer
    size_t viewport_offset;           // NEW: Physical row offset for scrolling
    bool quit;
} ik_repl_ctx_t;
```

**Implementation**:
- Initialize scrollback in `ik_repl_init()`
- Add lines to scrollback when user submits input (future: when AI responds)
- Track viewport offset for scrolling state

**Test Coverage**:
- Scrollback initialization
- Lines appended to scrollback
- Viewport offset tracking

---

### Task 4.2: Viewport Calculation ✅ COMPLETE

**Goal**: Calculate which portion of scrollback + input buffer to display.

**Add to** `src/repl.c`:
```c
// Calculate viewport boundaries
typedef struct {
    size_t scrollback_start_line;   // First scrollback line to render
    size_t scrollback_lines_count;  // How many scrollback lines visible
    size_t input_buffer_start_row;     // Terminal row where input buffer begins
} ik_viewport_t;

res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out);
```

**Logic**:
- Terminal has N rows total
- Input buffer needs M physical rows (may wrap)
- Scrollback gets remaining N-M rows
- Account for viewport_offset when scrolling up
- When scrollback fits entirely, input buffer at bottom
- When scrollback overflows, enable scrolling

**Test Coverage**:
- Viewport with empty scrollback (input buffer fills screen)
- Viewport with small scrollback (scrollback + input buffer both visible)
- Viewport with large scrollback (scrollback overflows, scrolling needed)
- Viewport calculation after terminal resize
- Viewport offset clamping (don't scroll past top/bottom)

---

### Task 4.3: Scrollback Rendering ✅ COMPLETE

**Goal**: Render visible scrollback lines to terminal.

**Add to** `src/render.h`:
```c
// Render scrollback lines to terminal
res_t ik_render_scrollback(ik_render_ctx_t *ctx,
                                  ik_scrollback_t *scrollback,
                                  size_t start_line,
                                  size_t line_count,
                                  int32_t *rows_used_out);
```

**Implementation**:
- Iterate through visible scrollback lines
- Write each line with proper line wrapping
- Track how many terminal rows consumed
- Return rows used for input buffer positioning

**Test Coverage**:
- Render empty scrollback
- Render single line
- Render multiple lines with wrapping
- Render with UTF-8 content (CJK, emoji)
- Terminal too small for all lines (truncation)

---

### Task 4.4: Combined Frame Rendering ✅ COMPLETE

**Goal**: Render complete frame (scrollback + input buffer).

**Modify** `ik_repl_render_frame()`:
```c
res_t ik_repl_render_frame(ik_repl_ctx_t *repl)
{
    // 1. Calculate viewport
    ik_viewport_t viewport;
    ik_repl_calculate_viewport(repl, &viewport);

    // 2. Render scrollback
    int32_t rows_used = 0;
    ik_render_scrollback(repl->render, repl->scrollback,
                                viewport.scrollback_start_line,
                                viewport.scrollback_lines_count,
                                &rows_used);

    // 3. Render input buffer at correct position
    ik_render_input_buffer(repl->render, ...);

    return OK(repl);
}
```

**Test Coverage**:
- Render frame with empty scrollback
- Render frame with scrollback + input buffer
- Render frame after scrolling
- Frame rendering after terminal resize

---

### Task 4.5: Scrolling Commands

**Goal**: Add Page Up/Down for scrolling through history.

**Add to** `src/input.h`:
```c
typedef enum {
    // ... existing actions ...
    IK_INPUT_PAGE_UP,      // Page Up key
    IK_INPUT_PAGE_DOWN,    // Page Down key
} ik_input_action_type_t;
```

**Parse sequences**:
- Page Up: `\x1b[5~`
- Page Down: `\x1b[6~`

**Add scrolling logic to** `ik_repl_process_action()`:
```c
case IK_INPUT_PAGE_UP:
    // Scroll up by terminal height
    repl->viewport_offset += repl->term->screen_rows;
    // Clamp to max scrollback
    break;

case IK_INPUT_PAGE_DOWN:
    // Scroll down by terminal height
    if (repl->viewport_offset >= repl->term->screen_rows) {
        repl->viewport_offset -= repl->term->screen_rows;
    } else {
        repl->viewport_offset = 0;  // Bottom
    }
    break;
```

**Test Coverage**:
- Parse Page Up/Down sequences
- Scroll up through history
- Scroll down to bottom
- Scroll clamping at top/bottom
- Scrolling with empty scrollback (no-op)

---

### Task 4.6: Auto-Scroll on New Content

**Goal**: Automatically scroll to bottom when new content added to scrollback.

**Logic**:
- When user submits input → append to scrollback → reset `viewport_offset = 0`
- When AI responds (future) → append to scrollback → reset `viewport_offset = 0`
- Only auto-scroll if user was already at bottom (viewport_offset == 0)
- If user scrolled up, don't auto-scroll (let them read history)

**Test Coverage**:
- New content while at bottom → auto-scroll to bottom
- New content while scrolled up → stay scrolled up
- Viewport offset reset on submit

---

## Testing Strategy

### Unit Tests
- Viewport calculation with various scrollback sizes
- Scrollback rendering (empty, small, large)
- Page Up/Down parsing and action processing
- Viewport offset clamping

### Integration Tests
- Full frame rendering with scrollback + input buffer
- Scrolling through large scrollback
- Terminal resize with scrollback visible
- Auto-scroll behavior

### Manual Testing
```bash
# Build and run
make && bin/ikigai

# Test scrolling
1. Type and submit several messages (create scrollback)
2. Press Page Up → verify scroll up
3. Press Page Down → verify scroll down
4. Resize terminal → verify layout recalculates
5. Submit new message while scrolled up → verify stays scrolled up
6. Scroll to bottom, submit → verify auto-scroll works
```

---

## Success Criteria

- ✅ Scrollback integrated with REPL rendering
- ✅ Page Up/Down scrolls through history
- ✅ Viewport calculation handles all edge cases
- ✅ Terminal resize updates viewport correctly
- ✅ Auto-scroll on new content when at bottom
- ✅ 100% test coverage maintained

---

## Notes

**Phase 4 completes the core REPL UI**. After this phase:
- Users can view unlimited scrollback history
- Terminal resizing works correctly
- Full keyboard navigation (arrows, Page Up/Down, editing shortcuts)
- Ready for Phase 5 cleanup and Phase 6 enhancements
