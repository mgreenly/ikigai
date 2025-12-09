# Task: Implement Dirty Region Tracking for Differential Updates

## Target
Render Performance Optimization - Reduce terminal output by only updating changed lines

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/repl_viewport.c (current full-screen clear approach, lines 242-258)
- src/repl.h (ik_repl_ctx_t structure)
- src/layer.c (layer rendering)
- src/scrollback.c (scrollback structure)

## Pre-read Tests (patterns)
- tests/unit/repl/*.c (existing repl test patterns)

## Pre-conditions
- `make check` passes
- Every render clears entire screen with `\x1b[2J\x1b[H`
- render-buffer-reuse.md task is complete (reusable buffers exist)

## Task
Implement differential screen updates by tracking dirty regions. Instead of clearing and redrawing the entire screen on every keystroke, compare the new frame against the previous frame and only update changed lines.

Current approach:
```
Every frame: Clear screen → Redraw everything → Position cursor
```

New approach:
```
First frame: Clear screen → Render → Store as "previous frame"
Subsequent: Render → Compare to previous → Update only changed lines → Store as previous
```

For single-character input on a large scrollback, this can reduce terminal output by 90%+.

## TDD Cycle

### Red
1. Add previous-frame tracking to `ik_repl_ctx_t` in `src/repl.h`:
   ```c
   // Dirty region tracking for differential updates
   char **prev_frame_lines;      // Array of previous frame line contents
   size_t prev_frame_line_count;
   size_t prev_frame_capacity;
   bool force_full_redraw;       // Set on resize or first render
   ```

2. Create `tests/unit/repl/dirty_region_test.c`:
   - Test first render does full redraw
   - Test unchanged lines are not re-sent
   - Test changed line is updated with cursor positioning
   - Test resize triggers full redraw
   - Test scrolling triggers appropriate updates
   - Test cursor-only movement doesn't redraw content

3. Run `make check` - expect failures

### Green
1. Create `src/render_diff.c` with helper functions:
   ```c
   // Split rendered content into lines for comparison
   void ik_render_split_lines(const char *content, size_t len,
                              char ***lines_out, size_t *count_out);

   // Compare two line arrays, return which lines differ
   void ik_render_diff_lines(char **prev, size_t prev_count,
                             char **curr, size_t curr_count,
                             bool *dirty_flags);

   // Build differential output (only changed lines with positioning)
   size_t ik_render_build_diff(char *framebuffer, size_t capacity,
                               char **curr_lines, size_t curr_count,
                               bool *dirty_flags);
   ```

2. Modify `ik_repl_render_frame()` to:
   - On first render or resize: full redraw, store lines
   - On subsequent renders: render to buffer, diff, emit only changes
   - Use `\x1b[row;1H` to position cursor for each dirty line
   - Store current frame as previous for next comparison

3. Run `make check` - expect pass

### Refactor
1. Optimize line comparison (early exit on first difference)
2. Consider hash-based comparison for very long lines
3. Verify no memory leaks
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- First render and resize do full screen redraw
- Subsequent renders only update changed lines
- Previous frame is stored for comparison
- `force_full_redraw` flag exists for explicit full redraws
- Terminal output reduced for typical editing operations
