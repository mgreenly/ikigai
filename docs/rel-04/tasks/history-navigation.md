# Task: History Navigation with Up/Down Arrows

## Target
Feature: Command History - User Interaction

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/patterns/state-machine.md

### Pre-read Docs
- docs/backlog/readline-features.md (navigation behavior specification)

### Pre-read Source (patterns)
- src/input.h (ik_input_action_t, arrow key actions)
- src/repl_actions.h (action handlers)
- src/repl_actions.c (existing arrow key handlers)
- src/input_buffer/core.h (cursor position queries)
- src/history.h (history navigation functions)

### Pre-read Tests (patterns)
- tests/unit/repl/actions_test.c (action handler test patterns)
- tests/unit/input_buffer/cursor_test.c (cursor position tests)

## Pre-conditions
- `make check` passes
- History data structures exist
- History load/save functions exist
- Input buffer cursor operations work
- Arrow key input actions exist (IK_INPUT_ARROW_UP, IK_INPUT_ARROW_DOWN)

## Task
Modify Up/Down arrow key handlers to support history navigation when cursor is at position 0 (start of input buffer). Otherwise, maintain existing multi-line cursor movement behavior.

**Behavior specification**:
- **Up arrow**:
  - If cursor at byte offset 0 AND not already browsing: Start browsing with current input as pending, load last history entry
  - If cursor at byte offset 0 AND browsing: Navigate to previous entry
  - Otherwise: Normal cursor up (existing behavior)
- **Down arrow**:
  - If cursor at byte offset 0 AND browsing: Navigate to next entry (or restore pending input)
  - Otherwise: Normal cursor down (existing behavior)
- When loading history entry, replace entire input buffer contents
- Preserve cursor at position 0 after loading entry

## TDD Cycle

### Red
1. Create `tests/unit/repl/history_navigation_test.c`:
   ```c
   START_TEST(test_history_up_from_empty_input)
   {
       // Setup: Empty input buffer, cursor at 0
       // History has entries: "first", "second", "third"
       // Press Up
       // Verify: Input buffer now contains "third", cursor at 0
   }
   END_TEST

   START_TEST(test_history_up_multiple_times)
   {
       // Setup: Empty input, history ["a", "b", "c"]
       // Press Up -> expect "c"
       // Press Up -> expect "b"
       // Press Up -> expect "a"
       // Press Up -> expect "a" (at beginning, no change)
   }
   END_TEST

   START_TEST(test_history_down_restores_pending)
   {
       // Setup: Input "my text", cursor at 0, history ["a", "b"]
       // Press Up -> expect "b", pending = "my text"
       // Press Down -> expect "my text" restored
   }
   END_TEST

   START_TEST(test_history_up_with_cursor_not_at_zero)
   {
       // Setup: Input "hello", cursor at byte 3
       // Press Up -> should do normal cursor movement, NOT history
   }
   END_TEST

   START_TEST(test_history_navigation_with_multiline)
   {
       // Setup: Input "line1\nline2", cursor at 0
       // History has "prev"
       // Press Up -> expect "prev" (history takes precedence)
   }
   END_TEST

   START_TEST(test_history_empty)
   {
       // Setup: Empty input, empty history
       // Press Up -> no change (graceful no-op)
   }
   END_TEST
   ```

2. Run `make check` - expect test failures

### Green
1. Modify `src/repl_actions.c`, find arrow key handlers:

2. Modify `handle_arrow_up()`:
   ```c
   res_t handle_arrow_up(ik_repl_ctx_t *repl) {
       size_t cursor_byte, cursor_grapheme;
       TRY(ik_input_buffer_get_cursor_position(repl->input_buffer,
                                                &cursor_byte,
                                                &cursor_grapheme));

       // History navigation only if cursor at position 0
       if (cursor_byte == 0) {
           if (!ik_history_is_browsing(repl->history)) {
               // Start browsing - save current input as pending
               size_t len;
               const char *current = ik_input_buffer_get_text(repl->input_buffer, &len);
               TRY(ik_history_start_browsing(repl->history, current));
           }

           // Navigate to previous entry
           const char *entry = ik_history_prev(repl->history);
           if (entry != NULL) {
               ik_input_buffer_clear(repl->input_buffer);
               // Insert entry text into buffer
               for (const char *p = entry; *p; p++) {
                   if (*p == '\n') {
                       ik_input_buffer_insert_newline(repl->input_buffer);
                   } else {
                       ik_input_buffer_insert_codepoint(repl->input_buffer, *p);
                   }
               }
               // Reset cursor to start
               while (ik_input_buffer_cursor_left(repl->input_buffer).ok) {}
           }
           return OK(NULL);
       }

       // Normal multi-line navigation
       return ik_input_buffer_cursor_up(repl->input_buffer);
   }
   ```

3. Modify `handle_arrow_down()`:
   ```c
   res_t handle_arrow_down(ik_repl_ctx_t *repl) {
       size_t cursor_byte, cursor_grapheme;
       TRY(ik_input_buffer_get_cursor_position(repl->input_buffer,
                                                &cursor_byte,
                                                &cursor_grapheme));

       // History navigation only if cursor at position 0 AND browsing
       if (cursor_byte == 0 && ik_history_is_browsing(repl->history)) {
           const char *entry = ik_history_next(repl->history);
           if (entry != NULL) {
               ik_input_buffer_clear(repl->input_buffer);
               // Insert entry text
               for (const char *p = entry; *p; p++) {
                   if (*p == '\n') {
                       ik_input_buffer_insert_newline(repl->input_buffer);
                   } else {
                       ik_input_buffer_insert_codepoint(repl->input_buffer, *p);
                   }
               }
               // Reset cursor to start
               while (ik_input_buffer_cursor_left(repl->input_buffer).ok) {}
           } else {
               // Reached end, stop browsing
               ik_history_stop_browsing(repl->history);
           }
           return OK(NULL);
       }

       // Normal multi-line navigation
       return ik_input_buffer_cursor_down(repl->input_buffer);
   }
   ```

4. When user starts typing (any character inserted), stop browsing:
   - Modify character insertion handler to call `ik_history_stop_browsing()`

5. Run `make check` - expect pass

### Refactor
1. Consider extracting "load history entry into buffer" to helper function (avoid duplication)
2. Ensure UTF-8 handling is correct for history entries
3. Test edge cases: very long history entries, Unicode in history
4. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- Up arrow at cursor position 0 navigates to previous history entry
- Down arrow at cursor position 0 (while browsing) navigates to next entry
- Pending input is preserved and restored when browsing past end
- Normal multi-line cursor movement works when cursor not at position 0
- Starting to type exits history browsing mode
- Empty history is handled gracefully (no crashes)
- 100% test coverage maintained
