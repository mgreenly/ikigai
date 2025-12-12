# Task: Fix Cursor Position When Viewport Has One Blank Line

## Target

User Story: docs/rel-05/user-stories/50-cursor-position-separator-bug.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/rel-05/user-stories/50-cursor-position-separator-bug.md
- docs/architecture.md (layer system, cursor rendering)

## Pre-read Source (patterns)
- src/layer_input.c (input layer and cursor rendering)
- src/render.c (cursor position calculation)
- src/repl.h (layer structure)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Cursor renders correctly when scrollback doesn't fill viewport
- Bug present: when viewport has exactly one blank line at bottom, cursor renders on separator instead of input line

## Task

Fix the cursor positioning bug that occurs when scrollback content leaves exactly one blank line at the bottom of the viewport.

**Root cause:** Cursor Y position calculation is off by one when viewport is nearly full. The cursor is being placed on the bottom separator line instead of the input line where the text is being typed.

**Solution:** Correct the cursor Y position calculation to always place cursor on the same line as the input text, regardless of viewport fullness.

## TDD Cycle

### Red

1. Create test in `tests/unit/layer/input_cursor_position_test.c` (or add to existing input layer tests):
   - Setup terminal height = 20 lines
   - Fill scrollback to leave exactly 1 blank line at bottom
   - Add text to input buffer: "/clear"
   - Render frame
   - Get cursor position (cursor_y, cursor_x)
   - Get input layer position (input_y)
   - Verify cursor_y == input_y (same line as input text)
   - Verify cursor is NOT on separator line (cursor_y != separator_y)

2. Add test case for different viewport states:
   - Test: cursor position when viewport is full
   - Test: cursor position when viewport is half full
   - Test: cursor position when viewport is nearly empty
   - All should place cursor on input line

3. Run `make check` - expect test to FAIL (cursor on separator line)

### Green

1. Locate cursor rendering in `src/layer_input.c`:
   - Find where cursor Y position is calculated
   - Find where cursor is rendered to framebuffer

2. Common patterns causing off-by-one:
   ```c
   // WRONG - cursor displaced below input
   int32_t cursor_y = input->y + input->height;  // Beyond input layer

   // WRONG - adds extra line
   int32_t cursor_y = input->y + line_offset + 1;  // Off by one

   // CORRECT - cursor on same line as text
   int32_t cursor_y = input->y + line_offset;  // Correct line
   ```

3. Verify cursor X calculation is correct:
   ```c
   // Cursor X should be after last character
   int32_t cursor_x = prompt_width + text_width;
   ```

4. Check if the issue is in multi-line input handling:
   - For single-line input: cursor_y = input_y
   - For multi-line input: cursor_y = input_y + current_line_index
   - Never cursor_y = input_y + input_height (that's beyond input layer)

5. Fix the calculation:
   ```c
   // Ensure cursor is on input line, not separator
   int32_t cursor_y = repl->input_layer_y + repl->input->cursor_row;
   int32_t cursor_x = repl->input_layer_x + repl->input->cursor_col;

   // NOT:
   // int32_t cursor_y = repl->input_layer_y + repl->input->height;  // WRONG
   ```

6. Run `make check` - expect PASS

### Verify

1. Run `make check` - all tests pass

2. Run `make lint` - complexity checks pass

3. Manual test:
   - Run `bin/ikigai`
   - Resize terminal to small height (~20 lines)
   - Paste enough text to fill scrollback leaving 1 blank line
   - Type "/clear" slowly, watching cursor position
   - Verify cursor appears immediately after each character typed
   - Verify cursor never appears on separator line
   - Resize terminal and verify cursor still correct at different sizes

## Post-conditions

- `make check` passes
- Cursor always renders on input line with text
- Cursor never renders on separator line
- Cursor position correct at all viewport fill levels
- Test exists verifying cursor position when viewport is constrained
- Working tree is clean (all changes committed)
