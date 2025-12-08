# Task: Completion Display Trigger on Typing

## Target
Feature: Autocompletion - Display Trigger

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/repl_actions_completion.c (ik_repl_update_completion_after_char)
- src/repl_actions.c (IK_INPUT_CHAR handling)
- src/completion.h

### Pre-read Tests (patterns)
- tests/unit/repl/completion_navigation_test.c

## Pre-conditions
- `make check` passes
- Completion state machine complete (completion-state-machine task)

## Task
Modify completion behavior so typing triggers the completion display (not Tab).

**Current behavior:**
- Tab triggers completion

**Required behavior:**
- Typing `/` shows all commands (completion display appears)
- Typing more characters filters the list (e.g., `/m` shows mark, model)
- Tab cycles through the options (doesn't trigger display)

The `ik_repl_update_completion_after_char()` function already exists but only updates existing completion. Extend it to create completion when typing `/`.

## TDD Cycle

### Red
1. Add tests in `tests/unit/repl/completion_trigger_test.c`:
   ```c
   START_TEST(test_typing_slash_triggers_completion)
   {
       // Type "/" - completion should activate with all commands
       ik_input_buffer_insert_codepoint(repl->input_buffer, '/');
       ik_repl_update_completion_after_char(repl);

       ck_assert_ptr_nonnull(repl->completion);
       ck_assert_uint_eq(repl->completion->count, 7);  // All commands
   }
   END_TEST

   START_TEST(test_typing_m_after_slash_filters)
   {
       // Type "/m" - completion should show mark, model
       ik_input_buffer_insert_codepoint(repl->input_buffer, '/');
       ik_repl_update_completion_after_char(repl);
       ik_input_buffer_insert_codepoint(repl->input_buffer, 'm');
       ik_repl_update_completion_after_char(repl);

       ck_assert_ptr_nonnull(repl->completion);
       ck_assert_uint_eq(repl->completion->count, 2);
   }
   END_TEST

   START_TEST(test_typing_regular_text_no_completion)
   {
       // Type "hello" - no completion (doesn't start with /)
       ik_input_buffer_set_text(repl->input_buffer, "hello", 5);
       ik_repl_update_completion_after_char(repl);

       ck_assert_ptr_null(repl->completion);
   }
   END_TEST

   START_TEST(test_backspace_refilters)
   {
       // Type "/ma" then backspace to "/m"
       // Completion should re-filter to show mark, model
   }
   END_TEST
   ```

2. Run `make check` - expect failures (typing doesn't trigger completion)

### Green
1. Modify `ik_repl_update_completion_after_char()` in `src/repl_actions_completion.c`:
   ```c
   void ik_repl_update_completion_after_char(ik_repl_ctx_t *repl)
   {
       assert(repl != NULL);

       size_t text_len = 0;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &text_len);

       // Check if input starts with '/'
       if (text_len > 0 && text[0] == '/') {
           // Create null-terminated string for prefix
           char *prefix = talloc_strndup(repl, text, text_len);
           if (prefix == NULL) PANIC("OOM");

           // Create new completion with current prefix
           ik_completion_t *new_comp = ik_completion_create_for_commands(repl, prefix);

           // Preserve original_input from existing completion (for ESC revert)
           char *original = NULL;
           if (repl->completion != NULL && repl->completion->original_input != NULL) {
               original = talloc_strdup(repl, repl->completion->original_input);
           } else if (new_comp != NULL) {
               // First time - store current input as original
               original = talloc_strdup(new_comp, prefix);
           }

           // Replace old completion
           ik_repl_dismiss_completion(repl);

           if (new_comp != NULL) {
               new_comp->original_input = original;
               repl->completion = new_comp;
           } else {
               talloc_free(original);
           }

           talloc_free(prefix);
       } else {
           // Input doesn't start with '/' - dismiss completion
           ik_repl_dismiss_completion(repl);
       }
   }
   ```

2. Ensure backspace also calls `ik_repl_update_completion_after_char()` in `ik_repl_process_action()`

3. Run `make check` - expect pass

### Refactor
1. Consider extracting "create/update completion" logic into separate function
2. Ensure no memory leaks with original_input handling
3. Run `make lint` - verify passes
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Typing `/` triggers completion display
- Typing more characters filters completion list
- Backspace re-filters completion list
- Regular text (not starting with `/`) has no completion
- Tab still cycles through options (doesn't trigger)
