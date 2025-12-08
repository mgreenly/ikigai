# Task: Completion Dismiss on Accept

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
- src/repl_actions_completion.c (Tab handling - has the bug)
- src/completion.h (completion data structure)
- src/layer_completion.c (completion layer rendering)

### Pre-read Tests (patterns)
- tests/unit/repl/completion_actions_test.c (if exists)
- tests/unit/completion_test.c

## Pre-conditions
- `make check` passes
- `make lint` passes
- completion-prefix-matching task complete

## Task
Fix Tab acceptance to dismiss the completion menu after accepting a selection.

**Problem:** After pressing Tab to accept a completion, the completion menu remains visible with the selected item shown in inverse highlighting.

**Root cause:** `ik_repl_handle_tab_action()` in `src/repl_actions_completion.c` does NOT call `ik_repl_dismiss_completion()` after accepting. Compare with Space key handler (line 206) which correctly dismisses.

**Current flow:**
1. User presses Tab → cycles to next completion → updates input buffer
2. Returns without dismissing
3. `repl->completion` remains non-NULL
4. Completion layer still visible with highlight

**Expected flow:**
1. User presses Tab when already on desired completion → accepts it
2. Input buffer updated with completed text
3. Completion dismissed (`repl->completion = NULL`)
4. Completion layer hidden

**Behavioral decision:** Tab should accept AND dismiss when the user is satisfied with the selection. This matches standard shell behavior where Tab completes and returns to normal input mode.

**Alternative considered:** Some systems use Tab to cycle and Enter to accept. But our current design uses Tab to cycle AND accept simultaneously (each Tab shows next option). To accept the current selection, user can press Space (adds space after command) or just continue typing. Tab accepting should also dismiss.

**Fix:** Add `ik_repl_dismiss_completion(repl)` call at the end of the Tab handling path when completion is active.

## TDD Cycle

### Red
1. Add/update test for Tab acceptance:
   ```c
   START_TEST(test_tab_accept_dismisses_completion)
   {
       // Setup REPL with input "/m"
       // Trigger completion (should show mark, model)
       // repl->completion should be non-NULL

       // Press Tab - cycles to next and accepts
       ik_repl_handle_tab_action(repl);

       // Verify input buffer updated
       size_t len;
       const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);
       // Should have completed text

       // KEY ASSERTION: completion should be dismissed
       ck_assert_ptr_null(repl->completion);
   }
   END_TEST
   ```

2. Run `make check` - expect failure (completion not dismissed)

### Green
1. Modify `ik_repl_handle_tab_action()` in `src/repl_actions_completion.c`:
   ```c
   // If completion is already active, cycle to next
   if (repl->completion != NULL) {
       // ... existing code to set original_input if needed ...

       // Move to next candidate
       ik_completion_next(repl->completion);

       // Update input buffer with new selection
       res_t res = update_input_with_completion_selection(repl);
       if (is_err(&res)) {
           ik_repl_dismiss_completion(repl);
           return res;
       }

       // ADD: Dismiss completion after accepting
       ik_repl_dismiss_completion(repl);

       return OK(NULL);
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider if behavior should be different (Tab cycles vs Tab accepts)
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Pressing Tab when completion is active:
  1. Cycles to next candidate
  2. Updates input buffer
  3. Dismisses completion menu
- Completion layer no longer visible after Tab accept
