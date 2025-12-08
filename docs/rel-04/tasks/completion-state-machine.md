# Task: Completion State Machine

## Target
Feature: Autocompletion - Tab Cycling Behavior

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/completion.h (completion data structure)
- src/repl_actions_completion.c (current TAB handling)
- src/repl.h (repl context)

### Pre-read Tests (patterns)
- tests/unit/completion/matching_test.c
- tests/unit/repl/completion_navigation_test.c

## Pre-conditions
- `make check` passes
- fzy integration complete (completion-fzy-integrate task)

## Task
Implement the completion state machine with Tab cycling behavior:

**Current behavior (broken):**
- Tab opens completion, immediately accepts first match
- No cycling through options

**Required behavior:**
1. Typing `/` triggers completion display (shows all commands)
2. Typing more characters filters/re-scores the list
3. Tab cycles to next match (updates input buffer, highlights selection)
4. Tab wraps around at end of list
5. ESC reverts to original typed text
6. Space commits selection + continues editing
7. Enter commits selection + submits

**New state:**
- `original_input`: stores text before first Tab press (for ESC revert)
- Completion stays active while cycling (not dismissed on Tab)

## TDD Cycle

### Red
1. Add `original_input` field to `ik_completion_t` in `src/completion.h`:
   ```c
   typedef struct {
       char **candidates;
       size_t count;
       size_t current;
       char *prefix;
       char *original_input;  // For ESC revert (what user typed before Tab)
   } ik_completion_t;
   ```

2. Add tests in `tests/unit/repl/completion_state_test.c`:
   ```c
   START_TEST(test_tab_cycles_to_next)
   {
       // Setup: input "/m", completion active with ["mark", "model"]
       // Tab should cycle current from 0 to 1
       // Tab again should cycle from 1 to 0
   }
   END_TEST

   START_TEST(test_tab_updates_input_buffer)
   {
       // Setup: input "/m", completion shows ["mark", "model"]
       // Tab should update input buffer to "/mark"
       // Tab again should update to "/model"
   }
   END_TEST

   START_TEST(test_esc_reverts_to_original)
   {
       // Setup: input "/m", Tab to "/mark"
       // ESC should revert input buffer to "/m"
       // Completion should remain visible
   }
   END_TEST

   START_TEST(test_space_commits_selection)
   {
       // Setup: input "/m", Tab to "/mark"
       // Space should set input to "/mark " and dismiss completion
   }
   END_TEST

   START_TEST(test_enter_commits_and_submits)
   {
       // Setup: input "/m", Tab to "/mark"
       // Enter should submit "/mark"
   }
   END_TEST
   ```

3. Run `make check` - expect failures

### Green
1. Modify `ik_repl_handle_tab_action()` in `src/repl_actions_completion.c`:
   ```c
   res_t ik_repl_handle_tab_action(ik_repl_ctx_t *repl)
   {
       // If completion active: cycle to next
       if (repl->completion != NULL) {
           ik_completion_next(repl->completion);

           // Update input buffer with current selection
           const char *selected = ik_completion_get_current(repl->completion);
           char *replacement = talloc_asprintf(repl, "/%s", selected);
           if (replacement == NULL) PANIC("OOM");

           res_t res = ik_input_buffer_set_text(repl->input_buffer,
                                                 replacement, strlen(replacement));
           talloc_free(replacement);
           if (is_err(&res)) return res;

           // Keep completion active (don't dismiss)
           return OK(NULL);
       }

       // No completion active - trigger completion
       size_t text_len = 0;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &text_len);

       if (text_len == 0 || text[0] != '/') {
           return OK(NULL);
       }

       // Store original input for ESC revert
       char *original = talloc_strndup(repl, text, text_len);
       if (original == NULL) PANIC("OOM");

       // Create completion
       ik_completion_t *comp = ik_completion_create_for_commands(repl, original);

       if (comp != NULL) {
           comp->original_input = original;
           repl->completion = comp;
       } else {
           talloc_free(original);
       }

       return OK(NULL);
   }
   ```

2. Add ESC handler to revert:
   ```c
   // In ik_repl_process_action(), IK_INPUT_ESCAPE case:
   if (repl->completion != NULL && repl->completion->original_input != NULL) {
       // Revert to original input
       ik_input_buffer_set_text(repl->input_buffer,
                                repl->completion->original_input,
                                strlen(repl->completion->original_input));
   }
   ik_repl_dismiss_completion(repl);
   ```

3. Add Space commit handler (new action or in IK_INPUT_CHAR):
   ```c
   // When space pressed and completion active:
   if (repl->completion != NULL && action->codepoint == ' ') {
       const char *selected = ik_completion_get_current(repl->completion);
       char *committed = talloc_asprintf(repl, "/%s ", selected);
       ik_input_buffer_set_text(repl->input_buffer, committed, strlen(committed));
       talloc_free(committed);
       ik_repl_dismiss_completion(repl);
       return OK(NULL);
   }
   ```

4. Run `make check` - expect pass

### Refactor
1. Extract common "commit selection" logic into helper function
2. Ensure original_input is properly freed with completion context
3. Run `make lint` - verify passes
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Tab cycles through completion options
- Tab updates input buffer to show current selection
- ESC reverts to original typed text
- Space commits selection and continues editing
- Enter commits selection and submits
- Completion stays active during Tab cycling
