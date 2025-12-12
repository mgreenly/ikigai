# Task: Fix Layer Positioning When Viewport Is Full

## Target

User Story: docs/rel-05/user-stories/48-full-viewport-separator-bug.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/patterns/composite.md

## Pre-read Docs
- docs/rel-05/user-stories/48-full-viewport-separator-bug.md
- docs/architecture.md (layer system)

## Pre-read Source (patterns)
- src/render.c (layer positioning and composition)
- src/layer_scrollback.c (scrollback layer rendering)
- src/layer_separator.c (separator layer rendering)
- src/layer_input.c (input layer rendering)
- src/repl.h (layer cake structure)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Layer system works correctly when scrollback doesn't fill viewport
- Bug present: when scrollback fills viewport, extra blank line appears and bottom separator missing

## Task

Fix the layer positioning bug that occurs when scrollback content fills the entire viewport.

**Root cause:** Layer positioning calculation incorrectly computes vertical offsets when scrollback occupies most/all of the terminal height. This causes:
1. Input layer to be positioned one line too low (creating blank line)
2. Bottom separator to be positioned off-screen (invisible)

**Solution:** Correct the layer positioning logic to maintain proper spacing regardless of scrollback size.

## TDD Cycle

### Red

1. Create test in `tests/unit/render/render_full_viewport_test.c` (or add to existing render tests):
   - Setup REPL with terminal height = 20 lines
   - Fill scrollback with 15+ lines (enough to fill viewport)
   - Render frame
   - Verify no blank line between top separator and input cursor
   - Verify bottom separator is visible (within terminal height bounds)
   - Verify input layer is at correct Y position (top_separator_y + 1)
   - Verify bottom separator is at correct Y position (input_y + input_height)

2. Alternatively, add test to existing layer composition tests:
   - Test case: "layer positions correct when scrollback fills viewport"
   - Assert each layer's Y coordinate is sequential with no gaps

3. Run `make check` - expect test to FAIL (blank line exists, bottom separator off-screen)

### Green

1. Locate layer positioning logic in `src/render.c`
   - Find where Y positions are calculated for each layer
   - Look for viewport height calculations

2. Identify the off-by-one error:
   - Check if scrollback viewport calculation adds extra line
   - Check if separator/input positioning skips a line
   - Check if bottom separator calculation exceeds terminal height

3. Common patterns to investigate:
   ```c
   // WRONG - might be creating gap
   int32_t input_y = scrollback_lines + separator_height + 1;  // Extra +1?

   // CORRECT
   int32_t input_y = scrollback_lines + separator_height;
   ```

4. Fix the calculation to ensure:
   ```c
   // Pseudo-code for correct layout
   int32_t y = 0;

   // Scrollback occupies lines [0, scrollback_visible_lines)
   y += scrollback_visible_lines;

   // Top separator at y
   int32_t top_sep_y = y;
   y += 1;  // Separator is 1 line

   // Input at y
   int32_t input_y = y;
   y += input_height;

   // Bottom separator at y
   int32_t bottom_sep_y = y;
   y += 1;

   // Total should not exceed term_height
   assert(y <= term_height);
   ```

5. If the issue is viewport calculation, ensure scrollback doesn't render beyond available space:
   ```c
   // Reserve space for separators and input
   int32_t available_for_scrollback = term_height - 2 - input_height;
   int32_t scrollback_visible_lines = min(scrollback_total, available_for_scrollback);
   ```

6. Run `make check` - expect PASS

### Verify

1. Run `make check` - all tests pass

2. Run `make lint` - complexity checks pass

3. Manual test:
   - Run `bin/ikigai` with small terminal (resize to ~20 lines)
   - Generate enough scrollback to fill screen (paste lorem ipsum)
   - Verify no blank line above cursor
   - Verify bottom separator is visible
   - Resize terminal larger and verify layout still correct
   - Resize terminal smaller and verify layout still correct

## Post-conditions

- `make check` passes
- Layer positioning correct when viewport is full
- No extra blank line before cursor
- Bottom separator always visible
- Layout correct at all terminal sizes
- Test exists verifying layer positions when viewport is full
- Working tree is clean (all changes committed)
