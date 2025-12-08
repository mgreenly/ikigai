# Task: Completion End-to-End Test

## Target
Feature: Autocompletion - Integration Test

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- tests/integration/completion_e2e_test.c (existing e2e tests)
- src/repl_actions.c (action processing)

### Pre-read Tests (patterns)
- tests/integration/completion_e2e_test.c

## Pre-conditions
- `make check` passes
- All completion tasks complete

## Task
Create comprehensive end-to-end tests for the new completion behavior:

1. Type `/` - see all commands
2. Type `/m` - see filtered list (mark, model)
3. Tab cycles through options
4. Tab updates input buffer
5. ESC reverts to original
6. Space commits + continues
7. Enter commits + submits

## TDD Cycle

### Red
1. Update/add tests in `tests/integration/completion_e2e_test.c`:
   ```c
   START_TEST(test_typing_triggers_completion)
   {
       // Simulate typing "/"
       simulate_keypress(repl, '/');

       // Completion should be active
       ck_assert_ptr_nonnull(repl->completion);
       ck_assert_uint_eq(repl->completion->count, 7);  // All commands
   }
   END_TEST

   START_TEST(test_typing_filters_completion)
   {
       // Type "/m"
       simulate_keypress(repl, '/');
       simulate_keypress(repl, 'm');

       ck_assert_ptr_nonnull(repl->completion);
       ck_assert_uint_eq(repl->completion->count, 2);  // mark, model
   }
   END_TEST

   START_TEST(test_tab_cycles_through_options)
   {
       // Type "/m"
       simulate_keypress(repl, '/');
       simulate_keypress(repl, 'm');

       // First selection (index 0)
       const char *first = ik_completion_get_current(repl->completion);

       // Tab to next
       simulate_tab(repl);

       // Selection changed
       const char *second = ik_completion_get_current(repl->completion);
       ck_assert_str_ne(first, second);

       // Completion still active
       ck_assert_ptr_nonnull(repl->completion);
   }
   END_TEST

   START_TEST(test_tab_updates_input_buffer)
   {
       // Type "/m"
       simulate_keypress(repl, '/');
       simulate_keypress(repl, 'm');

       // Tab to select first option
       simulate_tab(repl);

       // Input buffer should contain full command
       size_t len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);
       // Should be "/mark" or "/model" (first by score)
       ck_assert(text[0] == '/');
       ck_assert(len > 2);
   }
   END_TEST

   START_TEST(test_esc_reverts_input)
   {
       // Type "/m", Tab to "/mark", ESC
       simulate_keypress(repl, '/');
       simulate_keypress(repl, 'm');
       simulate_tab(repl);  // Input now "/mark"
       simulate_escape(repl);

       // Input should revert to "/m"
       size_t len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);
       ck_assert_uint_eq(len, 2);
       ck_assert(strncmp(text, "/m", 2) == 0);
   }
   END_TEST

   START_TEST(test_space_commits_and_continues)
   {
       // Type "/m", Tab to select, Space
       simulate_keypress(repl, '/');
       simulate_keypress(repl, 'm');
       simulate_tab(repl);
       simulate_keypress(repl, ' ');

       // Completion dismissed
       ck_assert_ptr_null(repl->completion);

       // Input has space at end
       size_t len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);
       ck_assert(text[len-1] == ' ');
   }
   END_TEST

   START_TEST(test_enter_commits_and_submits)
   {
       // Type "/m", Tab to select, Enter
       simulate_keypress(repl, '/');
       simulate_keypress(repl, 'm');
       simulate_tab(repl);
       simulate_enter(repl);

       // Should have submitted (check submit flag or scrollback)
       // Completion dismissed
       ck_assert_ptr_null(repl->completion);
   }
   END_TEST
   ```

2. Run `make check` - verify all tests pass

### Green
All implementation should be complete from prior tasks. If tests fail:
1. Debug and fix the specific failing behavior
2. Run `make check` until all pass

### Refactor
1. Add edge case tests:
   - Empty completion list (no matches)
   - Single item completion
   - Tab wrap-around
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- All completion behaviors verified end-to-end
- Tab cycling works correctly
- ESC revert works correctly
- Space/Enter commit works correctly
