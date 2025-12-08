# Task: Fix Horizontal Rule Not Rendering

## Target
Bug Fix: Horizontal Rules Around Input Area

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/layer_separator.c (separator layer implementation)
- src/repl_init.c (layer cake initialization)
- src/repl_viewport.c (separator visibility calculation)
- src/layer.c (layer cake rendering)
- src/repl.h (repl context, separator_visible flag)

### Pre-read Tests (patterns)
- tests/unit/layer/separator_layer_test.c
- tests/integration/render_e2e_test.c (if exists)

## Pre-conditions
- `make check` passes

## Task
The horizontal rule (separator) between scrollback and input area is not rendering. Investigate and fix.

**Expected behavior:**
- A horizontal line of dashes appears above the input area
- Separator visible in normal operation

**Possible causes (investigate):**
1. `separator_visible` flag not set correctly in viewport calculation
2. Layer cake not rendering separator layer
3. Separator layer not added to cake
4. Visibility calculation incorrect when scrollback is empty

**Investigation approach:**
1. Add debug logging to trace separator visibility
2. Check viewport calculation when scrollback is empty
3. Verify layer order in layer cake

## TDD Cycle

### Red
1. Add integration test to verify separator renders:
   ```c
   START_TEST(test_separator_renders_on_empty_scrollback)
   {
       // Create REPL with empty scrollback
       // Render frame
       // Output should contain line of dashes
   }
   END_TEST

   START_TEST(test_separator_renders_with_scrollback)
   {
       // Add some scrollback content
       // Render frame
       // Output should contain separator between scrollback and input
   }
   END_TEST
   ```

2. Run test - expect failure (separator not in output)

### Green
1. Trace the bug:
   - Check `repl->separator_visible` after viewport calculation
   - Check if separator layer is in layer cake
   - Check layer cake render output

2. Likely fix in `repl_viewport.c`:
   ```c
   // Current calculation may be wrong when scrollback_rows == 0
   size_t separator_row = scrollback_rows;  // Row 0 when empty

   // Fix: separator should always be visible when in viewport
   viewport_out->separator_visible = separator_row >= first_visible_row &&
                                     separator_row <= last_visible_row;
   ```

3. Or fix in `ik_repl_render_frame()`:
   ```c
   // Ensure separator_visible is passed correctly to layer
   repl->separator_visible = separator_visible;  // This line exists
   // But is the layer actually rendering?
   ```

4. Run `make check` - expect pass after fix

### Refactor
1. Add more explicit tests for separator visibility edge cases
2. Consider adding debug logging for layer rendering (removable)
3. Run `make lint` - verify passes
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Separator (horizontal rule) renders above input area
- Separator visible with empty scrollback
- Separator visible with scrollback content
