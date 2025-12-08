# Task: Input Layer Trailing Newline Fix

## Target
Feature: UI Polish - Layer Rendering

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/patterns/composite.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/layer_input.c (input layer implementation - has the bug)
- src/layer_separator.c (reference - correctly appends `\r\n`)
- src/layer.h (layer interface)

### Pre-read Tests (patterns)
- tests/unit/layer/input_layer_test.c

## Pre-conditions
- `make check` passes
- `make lint` passes
- separator-unicode-box-drawing task complete

## Task
Fix the input layer to append a trailing `\r\n` after non-empty text so that subsequent layers render on their own line.

**Problem:** When typing in the input buffer, text appears ON the separator line instead of below it because the input layer doesn't output a trailing newline.

**Current behavior in `input_render()`:**
- Empty input: outputs `\r\n` (correct - blank line)
- Non-empty input: outputs just the text, no trailing newline (BUG)

**Expected behavior:**
- Empty input: outputs `\r\n` (1 blank line)
- Non-empty input: outputs text + `\r\n` (text followed by newline)

This matches the contract implied by `get_height()` - if the layer claims N rows, it should output content that occupies exactly N rows with proper line termination.

**Edge case:** Text that already ends with `\n` should NOT get double newlines. The `\n` → `\r\n` conversion already handles internal newlines. Add trailing `\r\n` only if text doesn't end with newline.

## TDD Cycle

### Red
1. Update test in `tests/unit/layer/input_layer_test.c`:
   ```c
   START_TEST(test_input_layer_render_simple_text_has_trailing_newline)
   {
       TALLOC_CTX *ctx = talloc_new(NULL);

       bool visible = true;
       const char *text = "Hello";
       const char *text_ptr = text;
       size_t text_len = 5;

       ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);
       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

       layer->render(layer, output, 80, 0, 1);

       // Should be "Hello\r\n" (7 bytes)
       ck_assert_uint_eq(output->size, 7);
       ck_assert_int_eq(memcmp(output->data, "Hello\r\n", 7), 0);

       talloc_free(ctx);
   }
   END_TEST

   START_TEST(test_input_layer_render_text_ending_with_newline_no_double)
   {
       TALLOC_CTX *ctx = talloc_new(NULL);

       bool visible = true;
       const char *text = "Line1\n";  // Already ends with newline
       const char *text_ptr = text;
       size_t text_len = 6;

       ik_layer_t *layer = ik_input_layer_create(ctx, "input", &visible, &text_ptr, &text_len);
       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 100);

       layer->render(layer, output, 80, 0, 2);

       // Should be "Line1\r\n" (7 bytes) - single \r\n, not double
       ck_assert_uint_eq(output->size, 7);
       ck_assert_int_eq(memcmp(output->data, "Line1\r\n", 7), 0);

       talloc_free(ctx);
   }
   END_TEST
   ```

2. Update existing `test_input_layer_render_simple_text` to expect trailing newline
3. Run `make check` - expect failure

### Green
1. Modify `input_render()` in `src/layer_input.c`:
   ```c
   // After rendering all text characters...

   // Add trailing \r\n if text doesn't end with newline
   if (text_len > 0 && text[text_len - 1] != '\n') {
       ik_output_buffer_append(output, "\r\n", 2);
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Ensure the logic is clean and well-commented
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Input layer outputs trailing `\r\n` after non-empty text
- Text ending with `\n` does not get double newlines
- Visual output shows input on its own line, distinct from separators
