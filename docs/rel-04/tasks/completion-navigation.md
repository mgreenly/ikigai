# Task: Completion Navigation and Interaction

## Target
Feature: Tab Completion - User Interaction

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/patterns/state-machine.md

### Pre-read Docs
- docs/backlog/readline-features.md (interaction specification)

### Pre-read Source (patterns)
- src/input.h (IK_INPUT_TAB, arrow key actions)
- src/repl_actions.h (action handlers)
- src/repl_actions.c (existing action handlers)
- src/completion.h (completion navigation functions)

### Pre-read Tests (patterns)
- tests/unit/repl/actions_test.c (action handler tests)

## Pre-conditions
- `make check` passes
- IK_INPUT_TAB action exists
- Completion data structures exist
- Completion layer renders
- Command matching works

## Task
Implement completion interaction workflow:

**TAB key behavior**:
- If input starts with `/` and no completion active: Create completion, show layer
- If completion active: Accept current selection, insert into input buffer, dismiss completion
- If input doesn't start with `/`: No action (future: could insert actual tab)

**Arrow key behavior** (when completion active):
- Up: Move selection to previous candidate (with wraparound)
- Down: Move selection to next candidate (with wraparound)
- Other arrows: Dismiss completion (user moving cursor away)

**Escape key**: Dismiss completion without accepting

**Typing behavior**: As user types, update completion matching:
- If still matches prefix: Update candidate list
- If no longer matches: Dismiss completion

**Edge cases**:
- Empty input: TAB does nothing
- Single match: TAB accepts immediately
- No matches: TAB does nothing (or shows "no matches" briefly)

## TDD Cycle

### Red
1. Create `tests/unit/repl/completion_navigation_test.c`:
   ```c
   START_TEST(test_tab_triggers_completion)
   {
       // Setup: Input buffer "/m", cursor at end
       // Press TAB
       // Verify: completion created with ["mark", "model"]
       // Verify: completion layer visible
   }
   END_TEST

   START_TEST(test_tab_accepts_selection)
   {
       // Setup: Completion active with ["mark", "model"], current=0
       // Press TAB
       // Verify: Input buffer now "/mark"
       // Verify: Completion dismissed (NULL)
   }
   END_TEST

   START_TEST(test_arrow_up_changes_selection)
   {
       // Setup: Completion with ["mark", "model"], current=0
       // Press Up
       // Verify: current=1 (wraparound to last)
       // Press Up again
       // Verify: current=0
   }
   END_TEST

   START_TEST(test_arrow_down_changes_selection)
   {
       // Setup: Completion with ["mark", "model"], current=0
       // Press Down
       // Verify: current=1
       // Press Down again
       // Verify: current=0 (wraparound)
   }
   END_TEST

   START_TEST(test_escape_dismisses_completion)
   {
       // Setup: Completion active
       // Press Escape
       // Verify: Completion dismissed (NULL)
       // Verify: Input buffer unchanged
   }
   END_TEST

   START_TEST(test_typing_updates_completion)
   {
       // Setup: Input "/m", completion with ["mark", "model"]
       // Type 'o' -> input now "/mo"
       // Verify: Completion updated to ["model"] only
   }
   END_TEST

   START_TEST(test_typing_dismisses_on_no_match)
   {
       // Setup: Input "/m", completion with ["mark", "model"]
       // Type 'x' -> input now "/mx"
       // Verify: Completion dismissed (no matches)
   }
   END_TEST

   START_TEST(test_left_right_arrow_dismisses)
   {
       // Setup: Completion active
       // Press Left arrow
       // Verify: Completion dismissed
       // Verify: Cursor moved left (normal behavior)
   }
   END_TEST

   START_TEST(test_tab_on_empty_input_no_op)
   {
       // Setup: Empty input buffer
       // Press TAB
       // Verify: No completion created
   }
   END_TEST

   START_TEST(test_tab_on_non_slash_no_op)
   {
       // Setup: Input "hello"
       // Press TAB
       // Verify: No completion created
   }
   END_TEST
   ```

2. Run `make check` - expect test failures

### Green
1. Create handler for TAB action in `src/repl_actions.c`:
   ```c
   res_t handle_tab(ik_repl_ctx_t *repl) {
       size_t len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);

       if (repl->completion != NULL) {
           // Completion active - accept current selection
           const char *selected = ik_completion_get_current(repl->completion);

           // Replace input buffer with full command
           ik_input_buffer_clear(repl->input_buffer);
           ik_input_buffer_insert_codepoint(repl->input_buffer, '/');
           for (const char *p = selected; *p; p++) {
               ik_input_buffer_insert_codepoint(repl->input_buffer, *p);
           }

           // Dismiss completion
           talloc_free(repl->completion);
           repl->completion = NULL;
           return OK(NULL);
       }

       // No completion active - try to create one
       if (len == 0 || text[0] != '/') {
           return OK(NULL);  // No action for non-slash input
       }

       // Create completion
       repl->completion = ik_completion_create_for_commands(repl, text);
       // If NULL (no matches), that's fine - just no completion shown

       return OK(NULL);
   }
   ```

2. Modify arrow key handlers in `src/repl_actions.c`:
   ```c
   res_t handle_arrow_up(ik_repl_ctx_t *repl) {
       // If completion active, navigate
       if (repl->completion != NULL) {
           ik_completion_prev(repl->completion);
           return OK(NULL);
       }

       // Otherwise, use existing history/cursor logic
       // ... existing code ...
   }

   res_t handle_arrow_down(ik_repl_ctx_t *repl) {
       // If completion active, navigate
       if (repl->completion != NULL) {
           ik_completion_next(repl->completion);
           return OK(NULL);
       }

       // Otherwise, use existing history/cursor logic
       // ... existing code ...
   }
   ```

3. Add dismiss logic for left/right arrows:
   ```c
   res_t handle_arrow_left(ik_repl_ctx_t *repl) {
       // Dismiss completion if active
       if (repl->completion != NULL) {
           talloc_free(repl->completion);
           repl->completion = NULL;
       }
       return ik_input_buffer_cursor_left(repl->input_buffer);
   }

   res_t handle_arrow_right(ik_repl_ctx_t *repl) {
       // Dismiss completion if active
       if (repl->completion != NULL) {
           talloc_free(repl->completion);
           repl->completion = NULL;
       }
       return ik_input_buffer_cursor_right(repl->input_buffer);
   }
   ```

4. Add update logic when user types (in character insertion handler):
   ```c
   res_t handle_char(ik_repl_ctx_t *repl, uint32_t codepoint) {
       // If completion active, dismiss and re-evaluate after insertion
       bool had_completion = (repl->completion != NULL);
       if (had_completion) {
           talloc_free(repl->completion);
           repl->completion = NULL;
       }

       // Insert character
       TRY(ik_input_buffer_insert_codepoint(repl->input_buffer, codepoint));

       // Re-create completion if it was active
       if (had_completion) {
           size_t len;
           const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);
           if (len > 0 && text[0] == '/') {
               repl->completion = ik_completion_create_for_commands(repl, text);
           }
       }

       return OK(NULL);
   }
   ```

5. Wire up handlers in main event loop

6. Run `make check` - expect pass

### Refactor
1. Consider extracting completion dismiss logic to helper function
2. Ensure cursor position is correct after accepting completion
3. Test rapid TAB presses (shouldn't crash)
4. Verify memory cleanup (no leaks when dismissing)
5. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- TAB triggers completion for slash commands
- TAB accepts current selection when completion active
- Arrow Up/Down navigate through completions
- Arrow Left/Right dismiss completion
- Escape dismisses completion
- Typing updates completion dynamically
- No matches dismisses completion
- Empty input or non-slash input ignores TAB
- No memory leaks in completion lifecycle
- 100% test coverage maintained
