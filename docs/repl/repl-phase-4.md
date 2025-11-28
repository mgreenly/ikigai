# REPL Terminal - Phase 4: Viewport and Scrolling

[← Back to REPL Terminal Overview](README.md)

**Goal**: Integrate scrollback with REPL, add viewport calculation and scrolling commands.

This phase integrated the scrollback buffer (from Phase 3) into the REPL event loop, adding the ability to view historical output and scroll through it. This completes the split-buffer terminal interface: scrollback history above, input buffer below, with user control over viewport position.

## Implementation

### Scrollback Integration with REPL

**Modified** `src/repl.h`:
```c
typedef struct ik_repl_ctx_t {
    ik_term_ctx_t *term;
    ik_render_ctx_t *render;
    ik_input_buffer_t *input_buffer;
    ik_input_parser_t *input_parser;
    ik_scrollback_t *scrollback;      // Scrollback buffer
    size_t viewport_offset;           // Physical row offset for scrolling
    bool quit;
} ik_repl_ctx_t;
```

**Implementation**:
- Scrollback initialized in `ik_repl_init()`
- Lines added to scrollback when user submits input
- Viewport offset tracked for scrolling state

### Viewport Calculation

**Added to** `src/repl.c`:
```c
// Calculate viewport boundaries
typedef struct {
    size_t scrollback_start_line;   // First scrollback line to render
    size_t scrollback_lines_count;  // How many scrollback lines visible
    size_t input_buffer_start_row;  // Terminal row where input buffer begins
} ik_viewport_t;

res_t ik_repl_calculate_viewport(ik_repl_ctx_t *repl, ik_viewport_t *viewport_out);
```

**Logic**:
- Terminal has N rows total
- Input buffer needs M physical rows (may wrap)
- Scrollback gets remaining N-M rows
- Accounts for viewport_offset when scrolling up
- When scrollback fits entirely, input buffer at bottom
- When scrollback overflows, enable scrolling

**Test Coverage**:
- Viewport with empty scrollback (input buffer fills screen)
- Viewport with small scrollback (scrollback + input buffer both visible)
- Viewport with large scrollback (scrollback overflows, scrolling needed)
- Viewport calculation after terminal resize
- Viewport offset clamping (don't scroll past top/bottom)

### Scrollback Rendering

**Added to** `src/render.h`:
```c
// Render scrollback lines to terminal
res_t ik_render_scrollback(ik_render_ctx_t *ctx,
                           ik_scrollback_t *scrollback,
                           size_t start_line,
                           size_t line_count,
                           int32_t *rows_used_out);
```

**Implementation**:
- Iterates through visible scrollback lines
- Writes each line with proper line wrapping
- Tracks how many terminal rows consumed
- Returns rows used for input buffer positioning

**Test Coverage**:
- Render empty scrollback
- Render single line
- Render multiple lines with wrapping
- Render with UTF-8 content (CJK, emoji)
- Terminal too small for all lines (truncation)

### Combined Frame Rendering

**Modified** `ik_repl_render_frame()`:
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

### Scrolling Commands

**Added to** `src/input.h`:
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

**Scrolling logic in** `ik_repl_process_action()`:
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

### Auto-Scroll on New Content

**Logic**:
- When user submits input → append to scrollback → reset `viewport_offset = 0`
- Only auto-scroll if user was already at bottom (viewport_offset == 0)
- If user scrolled up, don't auto-scroll (let them read history)

**Test Coverage**:
- New content while at bottom → auto-scroll to bottom
- New content while scrolled up → stay scrolled up
- Viewport offset reset on submit

## Final Metrics

**Coverage**: 100%
- 1,648 lines
- 128 functions
- 510 branches

**Quality gates**: All passing
- fmt ✅
- check ✅
- lint ✅
- coverage ✅
- check-dynamic (ASan/UBSan/TSan) ✅

## What Was Validated

- Scrollback integrated with REPL rendering
- Page Up/Down scrolls through history
- Viewport calculation handles all edge cases
- Terminal resize updates viewport correctly
- Auto-scroll on new content when at bottom
- 100% test coverage maintained

## Notes

Phase 4 completes the core REPL UI. After this phase:
- Users can view unlimited scrollback history
- Terminal resizing works correctly
- Full keyboard navigation (arrows, Page Up/Down, editing shortcuts)
- Ready for Phase 5 cleanup and optional Phase 6 enhancements
