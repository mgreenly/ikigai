# Task: Completion Accept Cursor Position

## Target
Feature: Autocompletion - Behavior Fix

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/repl_actions_completion.c (`update_input_with_completion_selection()` - calls set_text)
- src/input_buffer/core.c (`ik_input_buffer_set_text()` - resets cursor to 0)
- src/input_buffer/core.h (input buffer API)
- src/input_buffer/multiline.c (`ik_input_buffer_cursor_to_line_end()` - existing function)

### Pre-read Tests (patterns)
- tests/unit/input_buffer/core_test.c
- tests/unit/repl/completion_actions_test.c (if exists)

## Pre-conditions
- `make check` passes
- `make lint` passes
- completion-dismiss-on-accept task complete

## Task
Fix cursor position after Tab completion acceptance. Currently the cursor moves to column 0 instead of the end of the completed text.

**Problem:**
- Before Tab: `/mod` with cursor at position 4
- After Tab (actual): `/model` with cursor at position 0
- After Tab (expected): `/model` with cursor at position 6

**Root cause:** `ik_input_buffer_set_text()` in `src/input_buffer/core.c` unconditionally resets cursor to position 0:
```c
input_buffer->cursor_byte_offset = 0;
input_buffer->target_column = 0;
input_buffer->cursor->byte_offset = 0;
input_buffer->cursor->grapheme_offset = 0;
```

This is correct for history browsing (Escape revert) but wrong for completion acceptance.

**Fix approach (Option C - least invasive):** After calling `ik_input_buffer_set_text()` in `update_input_with_completion_selection()`, explicitly move cursor to end using `ik_input_buffer_cursor_to_line_end()`.

The function `ik_input_buffer_cursor_to_line_end()` already exists in `src/input_buffer/multiline.c` and moves the cursor to the end of the current line.

**Alternative considered:** Adding a parameter to `set_text()` for cursor positioning. This is more invasive and affects other callers.

## TDD Cycle

### Red
1. Add test for cursor position after completion:
   ```c
   START_TEST(test_completion_accept_cursor_at_end)
   {
       // Setup REPL with input "/m"
       // Trigger completion (shows mark, model)
       // Tab to accept first completion

       // Get cursor position
       size_t byte_pos, grapheme_pos;
       ik_input_buffer_get_cursor_position(repl->input_buffer, &byte_pos, &grapheme_pos);

       // Get text length
       size_t text_len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &text_len);

       // Cursor should be at end of text
       ck_assert_uint_eq(byte_pos, text_len);
   }
   END_TEST
   ```

2. Run `make check` - expect failure (cursor at 0)

### Green
1. Modify `update_input_with_completion_selection()` in `src/repl_actions_completion.c`:
   ```c
   static res_t update_input_with_completion_selection(ik_repl_ctx_t *repl)
   {
       assert(repl != NULL);
       assert(repl->completion != NULL);

       char *replacement = build_completion_buffer_text(repl, repl->completion, NULL);
       if (replacement == NULL) {
           return OK(NULL);
       }

       // Replace input buffer with selected completion
       res_t res = ik_input_buffer_set_text(repl->input_buffer, replacement, strlen(replacement));
       talloc_free(replacement);
       if (is_err(&res)) {
           return res;
       }

       // ADD: Move cursor to end of completed text
       res = ik_input_buffer_cursor_to_line_end(repl->input_buffer);
       if (is_err(&res)) {
           return res;
       }

       return OK(NULL);
   }
   ```

2. Add include for multiline.h if needed (or it may already be available via core.h)
3. Run `make check` - expect pass

### Refactor
1. Consider if this should also apply to Space commit (`ik_repl_handle_completion_space_commit`)
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- After Tab completion acceptance, cursor is at end of completed text
- User can immediately continue typing after completion
