# Task: Tab Completion End-to-End Integration

## Target
Feature: Tab Completion - Full Integration

## Agent
model: opus

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/backlog/readline-features.md (complete specification)

### Pre-read Source (patterns)
- src/repl.h (REPL context, initialization)
- src/repl.c (REPL lifecycle, event loop)
- src/repl_actions.c (all action handlers)
- src/completion.h (all completion functions)
- src/layer_wrappers.h (completion layer)

### Pre-read Tests (patterns)
- tests/integration/* (integration test patterns)

## Pre-conditions
- `make check` passes
- All completion components implemented:
  - TAB input action
  - Completion data structures
  - Command matching
  - Argument matching
  - Completion layer rendering
  - Navigation handlers
- History feature complete (may have interaction)

## Task
Complete end-to-end integration and testing of tab completion feature:

1. **Viewport adjustments**: When completion layer is visible, ensure input buffer and completions remain fully visible (scrollback compresses if needed)

2. **Layer ordering**: Verify completion layer is in correct position (after input, before any bottom elements)

3. **Interaction edge cases**:
   - Completion + history interaction (both use arrows)
   - Completion during multi-line input
   - Rapid TAB presses
   - Completion while scrolling

4. **Escape key integration**: Add IK_INPUT_ESCAPE action if not exists

5. **Polish**:
   - Ensure smooth rendering (no flicker)
   - Proper ANSI code handling
   - Cursor remains visible during completion

6. **End-to-end tests**: Complete user workflows

## TDD Cycle

### Red
1. Add escape key action to `src/input.h` if not exists:
   ```c
   typedef enum {
       // ... existing actions ...
       IK_INPUT_ESCAPE,  // Escape key (dismiss completion)
   } ik_input_action_type_t;
   ```

2. Create `tests/integration/completion_e2e_test.c`:
   ```c
   START_TEST(test_completion_full_workflow)
   {
       // Initialize REPL
       // Type "/m"
       // Press TAB - verify completion shows
       // Press Down - verify selection changes
       // Press TAB - verify "/model" inserted
       // Verify completion dismissed
   }
   END_TEST

   START_TEST(test_completion_argument_workflow)
   {
       // Type "/debug "
       // Press TAB - verify ["off", "on"] shown
       // Press Down - select "on"
       // Press TAB - verify "/debug on" in buffer
   }
   END_TEST

   START_TEST(test_completion_escape_dismisses)
   {
       // Type "/m"
       // Press TAB - completion shows
       // Press Escape
       // Verify completion dismissed
       // Verify input unchanged ("/m")
   }
   END_TEST

   START_TEST(test_completion_no_matches_no_layer)
   {
       // Type "/xyz"
       // Press TAB
       // Verify no completion created
       // Verify layer not visible
   }
   END_TEST

   START_TEST(test_completion_with_history)
   {
       // History has "/clear"
       // Input buffer empty, cursor at 0
       // Press Up - history should work (not completion)
       // Type "/"
       // Press TAB - completion should work
   }
   END_TEST

   START_TEST(test_completion_multiline_input)
   {
       // Input: "line1\nline2\n/m"
       // Cursor at end after "/m"
       // Press TAB
       // Verify: Completion triggered for "/m"
       // Note: This may need special handling
   }
   END_TEST

   START_TEST(test_completion_viewport_adjustment)
   {
       // Fill scrollback with many lines
       // Type "/m"
       // Press TAB - completion shows
       // Verify: Input + completion both fully visible
       // Verify: Scrollback compressed if needed
   }
   END_TEST

   START_TEST(test_completion_render_no_flicker)
   {
       // Type "/m"
       // Press TAB
       // Render multiple frames
       // Verify: Consistent output, no corruption
   }
   END_TEST
   ```

3. Run `make check` - expect failures for missing integration

### Green
1. Add escape key parsing to `src/input.c`:
   ```c
   // In ik_input_parse_byte, handle ESC key (0x1b)
   if (byte == '\x1b' && !parser->in_escape) {
       // Could be start of escape sequence OR standalone ESC
       // Set flag and wait for next byte
       parser->in_escape = true;
       parser->esc_len = 0;
       action_out->type = IK_INPUT_UNKNOWN;
       return;
   }

   // Timeout handling for standalone ESC (in separate function)
   // For now, emit IK_INPUT_ESCAPE on timeout
   ```

2. Add escape handler in `src/repl_actions.c`:
   ```c
   res_t handle_escape(ik_repl_ctx_t *repl) {
       // Dismiss completion if active
       if (repl->completion != NULL) {
           talloc_free(repl->completion);
           repl->completion = NULL;
           return OK(NULL);
       }

       // Otherwise, no action
       return OK(NULL);
   }
   ```

3. Verify layer ordering in `src/repl.c` initialization:
   ```c
   // Layer order (top to bottom):
   // 1. Scrollback
   // 2. Spinner
   // 3. Upper separator
   // 4. Input
   // 5. Completion
   // Ensure layers added in correct order
   ```

4. Add viewport priority logic if needed:
   - Input buffer must be fully visible (priority 1)
   - Completion should be fully visible if possible (priority 2)
   - Scrollback compresses (priority 3)
   - This may already work with current layer cake logic

5. Handle multiline input with completion:
   - Completion trigger should check if input ends with slash command
   - Extract last line for matching

6. Wire all handlers into event loop in `src/repl.c`:
   ```c
   switch (action.type) {
       case IK_INPUT_TAB:
           TRY(handle_tab(repl));
           break;
       case IK_INPUT_ESCAPE:
           TRY(handle_escape(repl));
           break;
       // ... other cases ...
   }
   ```

7. Run `make check` - expect pass

### Refactor
1. Extract completion trigger logic to helper function
2. Consider completion state machine diagram for documentation
3. Optimize rendering (only re-render when completion changes)
4. Add logging for debugging completion lifecycle
5. Document completion keyboard shortcuts
6. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- Tab completion fully integrated into REPL
- Escape key dismisses completion
- Layer ordering is correct
- Viewport handles completion layer properly
- Completion + history don't conflict
- Multiline input works with completion
- All edge cases handled gracefully
- No memory leaks in completion lifecycle
- 100% test coverage maintained

## Success Criteria
The tab completion feature is fully functional:
- ✓ TAB key triggers completion for slash commands
- ✓ Arrow keys navigate through suggestions
- ✓ TAB accepts current selection
- ✓ Escape dismisses suggestions
- ✓ Typing updates suggestions dynamically
- ✓ Argument completion works for /model, /rewind, /debug
- ✓ Completion layer renders correctly below input
- ✓ No conflicts with history navigation
- ✓ Graceful handling of edge cases
- ✓ Smooth user experience

## Combined Feature Success
Both readline features (history + completion) work together:
- ✓ Up/Down arrows: history when cursor at 0, completion when active, normal cursor movement otherwise
- ✓ TAB key: completion trigger and accept
- ✓ Escape: dismiss completion
- ✓ No UI conflicts between features
- ✓ Persistent history across sessions
- ✓ Real-time completion suggestions
