# Phase 2 - Detailed Task List

## Phase 2 - Completed Tasks

- ✅ **Task 1**: ik_repl_render_frame() - REPL rendering helper
  - 1.1: Write test - successful render (empty workspace)
  - 1.2: Implement - minimal function signature
  - 1.3: Implement - actual render logic
  - 1.4: Write test - render with multi-line text
  - 1.5: Write test - render with cursor at various positions
  - 1.6: Write test - error handling / UTF-8
  - 1.7: Verify task complete (99c4490)
- ✅ **Task 2**: ik_repl_process_action() - Input action processing
  - 2.1: Write test - CHAR action
  - 2.2: Implement - minimal function signature
  - 2.3: Write tests - basic actions
  - 2.4: Write test - Ctrl+C quit flag
  - 2.5: Write tests - edge cases
  - 2.6: Write test - unknown action
  - 2.7: Fix file size lint error
  - 2.8: Verify task complete (coverage: 1074 lines, 91 functions, 373 branches)
- ✅ **Task 2.5**: Multi-line cursor navigation (cursor_up/down)
  - 2.5.1-2.5.13: Implementation (672df9b)
  - 2.5.14: LCOV exclusion reduction (9aea7ca, -8 markers)
  - 2.5.15: File size fixes (4511c66 - workspace.c → workspace.c + workspace_multiline.c)
  - 2.5.16: UTF-8 contract enforcement with abort()
- ✅ **Task 2.6**: Readline-style editing shortcuts
  - 2.6.1-2.6.2: Input actions for Ctrl+A/E/K/U/W (99a5bf7)
  - 2.6.3-2.6.4: cursor_to_line_start (Ctrl+A)
  - 2.6.4.1: Cursor module refactor (0456140 - workspace-internal, void+assertions, LCOV -10)
  - 2.6.5-2.6.6: cursor_to_line_end (Ctrl+E) (bcdaf48)
  - 2.6.7-2.6.8: kill_to_line_end (Ctrl+K) (5908d58)
  - 2.6.8.1: Coverage gaps fixed (1adc72e, d7fc09e - LCOV +2 → 160 total)
  - 2.6.9-2.6.10: kill_line (Ctrl+U) (df47679 - LCOV +1 → 161 total)

## Phase 2 - Remaining Tasks

### 2.6.9: Kill Line - Write Tests
- [x] Write test: `test_workspace_kill_line_basic()`
  - Setup: "hello\nworld\ntest", cursor in middle of "world"
  - Action: `ik_workspace_kill_line()`
  - Assert: text is "hello\ntest", cursor at start of "test" line
- [x] Write test: `test_workspace_kill_line_first_line()`
- [x] Write test: `test_workspace_kill_line_last_line()`
- [x] Write test: `test_workspace_kill_line_empty_line()`
- [x] **Red**: Tests fail

### 2.6.10: Kill Line - Implementation
- [x] Add to `src/workspace.h`: `res_t ik_workspace_kill_line(ik_workspace_t *ws);`
- [x] Implement in `src/workspace_multiline.c`:
  - Find line start (previous \n or start)
  - Find line end (next \n or end)
  - Delete entire line including \n
  - Position cursor at new line start
- [x] **Green**: Tests pass
- [x] LCOV exclusion: Added 1 marker for assertion (160 → 161 total)

### 2.6.11: Delete Word Backward - Write Tests
- [x] Write test: `test_workspace_delete_word_backward_basic()`
  - Setup: "hello world test", cursor after "test"
  - Action: `ik_workspace_delete_word_backward()`
  - Assert: text is "hello world ", cursor after "world "
- [x] Write test: `test_workspace_delete_word_backward_at_word_boundary()`
- [x] Write test: `test_workspace_delete_word_backward_multiple_spaces()`
- [x] Write test: `test_workspace_delete_word_backward_punctuation()`
- [x] Write test: `test_workspace_delete_word_backward_utf8()`
- [x] Write test: `test_workspace_delete_word_backward_at_start()`
- [x] Write test: `test_workspace_delete_word_backward_null_workspace_asserts()`
- [x] Add function declaration to `src/workspace.h`
- [x] Add stub implementation to `src/workspace.c` (returns OK, does nothing)
- [x] **Red**: Tests fail (5 failures: 70% pass rate, stub doesn't delete anything)

### 2.6.12: Delete Word Backward - Implementation
- [x] Implement in `src/workspace.c`:
  - Scan backward from cursor
  - Skip trailing whitespace
  - Find word boundary (whitespace, punctuation, newline)
  - Delete from boundary to cursor
  - Must be UTF-8 aware
  - Added `is_word_char()` helper function
  - Two-phase algorithm: skip non-word chars, then skip word chars
- [x] **Green**: Tests pass (all 8 tests pass)
- [x] Added 2 coverage tests: mixed_case_digits, only_punctuation
- [x] Split line_editing_test.c into 3 files (exceeded 500 line limit):
  - kill_to_line_end_test.c (5 tests)
  - kill_line_test.c (5 tests)
  - delete_word_backward_test.c (8 tests)
- [x] LCOV exclusion: Added 1 marker for assertion (161 → 162 total)

### 2.6.13: Integrate - Add to Process Action
- [ ] Add to `ik_repl_process_action()`:
  - `IK_INPUT_CTRL_A` → `ik_workspace_cursor_to_line_start()`
  - `IK_INPUT_CTRL_E` → `ik_workspace_cursor_to_line_end()`
  - `IK_INPUT_CTRL_K` → `ik_workspace_kill_to_line_end()`
  - `IK_INPUT_CTRL_U` → `ik_workspace_kill_line()`
  - `IK_INPUT_CTRL_W` → `ik_workspace_delete_word_backward()`
- [ ] Write integration tests in repl_test.c for each action
- [ ] **Green**: All tests pass

### 2.6.14: Verify Task 2.6 Complete
- [ ] Run: `make check` (all tests pass)
- [ ] Run: `make lint` (complexity checks pass)
- [ ] Run: `make coverage` (100% coverage)
- [ ] Check: no uncovered lines or branches

---

## Task 3: Main Event Loop

**Note**: The event loop logic already exists in src/client.c:main() (lines 140-176). We need to move this into `ik_repl_run()`.

### 3.1: Design - Event Loop Testing Strategy
- [ ] Review src/client.c:main() event loop (lines 140-176)
- [ ] Design: How to test event loop with mocked TTY input?
  - Option A: Make read() mockable via wrapper
  - Option B: Accept that event loop is integration-tested via client.c
  - Option C: Create test helper that injects bytes into input_parser
- [ ] Decision: Document chosen approach
- [ ] Note: Current client.c demonstrates the loop works end-to-end

### 3.2: Implement - Basic Event Loop
- [ ] Implement `ik_repl_run()` in `src/repl.c`:
  - Copy event loop logic from src/client.c:main() (lines 131-176)
  - Initial render: `ik_repl_render_frame(repl)`
  - Main loop:
    - Read byte from terminal: `read(repl->term->tty_fd, &byte, 1)`
    - Parse byte: `ik_input_parse_byte(repl->input_parser, byte, &action)`
    - Process action: `ik_repl_process_action(repl, &action)`
    - Check quit flag
    - Re-render if action != UNKNOWN: `ik_repl_render_frame(repl)`
  - Error handling for read, parse, process, render
  - Return OK on clean exit, ERR on failures
- [ ] Build: `make build/ikigai`
- [ ] **Green**: Compiles successfully

### 3.3: Write Tests - Event Loop Components (if mockable)
- [ ] **If** using mockable read wrapper:
  - Write test: `test_repl_run_single_keystroke()`
  - Write test: `test_repl_run_multiple_keystrokes()`
  - Write test: `test_repl_run_read_error()`
  - Write test: `test_repl_run_render_error()`
- [ ] **Else**: Document that event loop is integration-tested via manual testing
- [ ] Note: Full coverage of REPL event loop may require wrapper.h additions

### 3.4: Alternative - Component Testing
- [ ] If full event loop testing is deferred:
  - Test `ik_repl_render_frame()` in isolation (Task 1)
  - Test `ik_repl_process_action()` in isolation (Task 2)
  - Integration test via manual testing with actual terminal
- [ ] Document: Event loop is thin glue code over tested components
- [ ] Verify: No complex logic in event loop (just calls to tested functions)

### 3.7: Verify Task 3 Complete
- [ ] Run: `make check` (all tests pass)
- [ ] Run: `make lint` (complexity checks pass)
- [ ] Run: `make coverage` (100% coverage)
- [ ] Check: no uncovered lines or branches

---

## Task 4: Main Entry Point

**Note**: src/client.c already exists with a working main() function (lines 72-182). After Tasks 1-3 are complete, we'll simplify it to use the REPL module.

### 4.1: Review Current State
- [x] src/client.c exists with complete working implementation
- [ ] Review src/client.c:main() (lines 72-182)
- [ ] Identify what stays vs what moves to REPL:
  - **Stays in main.c**: root talloc context creation, error reporting
  - **Moves to repl module**: terminal init, render init, workspace init, input parser init, event loop
  - **Already in repl**: ik_repl_init() does terminal/render/workspace/input setup
  - **Already in repl**: ik_repl_run() will do the event loop

### 4.2: Simplify main.c to Use REPL Module
- [ ] Once Tasks 1-3 are complete, simplify `src/client.c` to:
  ```c
  #include "repl.h"
  #include "error.h"
  #include <stdio.h>
  #include <stdlib.h>
  #include <talloc.h>

  int main(void) {
      // Create root talloc context
      void *root_ctx = talloc_new(NULL);
      if (!root_ctx) {
          fprintf(stderr, "Failed to create talloc context\n");
          return EXIT_FAILURE;
      }

      // Initialize REPL
      ik_repl_ctx_t *repl = NULL;
      res_t result = ik_repl_init(root_ctx, &repl);
      if (is_err(&result)) {
          error_fprintf(stderr, result.err);
          talloc_free(root_ctx);
          return EXIT_FAILURE;
      }

      // Run REPL event loop
      result = ik_repl_run(repl);

      // Cleanup
      ik_repl_cleanup(repl);
      talloc_free(root_ctx);

      return is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  ```
- [ ] Note: This removes ~120 lines and delegates to REPL module

### 4.3: Optionally Rename to main.c
- [ ] Decide: Keep as client.c or rename to main.c?
- [ ] If renaming: `git mv src/client.c src/main.c`
- [ ] Update Makefile if needed (CLIENT_SRC → MAIN_SRC)
- [ ] Note: plan.md mentions renaming, but it's not critical

### 4.4: Test Build
- [ ] Run: `make clean`
- [ ] Run: `make build/ikigai`
- [ ] Verify: executable built successfully
- [ ] Run: `./build/ikigai` (quick smoke test: type a char, Ctrl+C to exit)

### 4.5: Verify Task 4 Complete
- [ ] Run: `make check` (all tests pass)
- [ ] Run: `make lint` (complexity checks pass)
- [ ] Run: `make coverage` (100% coverage)
- [ ] Build succeeds: `make build/ikigai`
- [ ] Executable runs and exits cleanly

---

## Task 5: Manual Testing and Polish

### 5.1: Basic Functionality Testing
- [ ] Launch: `./build/ikigai`
- [ ] Test: Type simple text
- [ ] Test: Press Enter (newline)
- [ ] Test: Backspace and delete
- [ ] Test: Left/right arrow keys
- [ ] Test: Exit with Ctrl+C
- [ ] Verify: Terminal restores cleanly

### 5.2: UTF-8 Testing
- [ ] Test: Type emoji (😀, 👍, 🎉)
- [ ] Test: Type combining characters (é, ñ, ü via combining)
- [ ] Test: Type CJK characters (你好, こんにちは, 한글)
- [ ] Test: Cursor movement through multi-byte chars (left/right)
- [ ] Verify: All display correctly, cursor positions correct

### 5.3: Text Wrapping Testing
- [ ] Test: Type long line that wraps at terminal boundary
- [ ] Test: Insert text in middle of wrapped line
- [ ] Test: Delete text in wrapped line
- [ ] Test: Backspace through wrapped text
- [ ] Test: Cursor left/right through wrapped boundary
- [ ] Verify: Wrapping behavior correct, cursor positions correct

### 5.4: Multi-line Testing
- [ ] Test: Type multiple lines with newlines
- [ ] Test: Arrow up/down through lines
- [ ] Test: Arrow up/down preserves column position
- [ ] Test: Arrow up at first line (no-op)
- [ ] Test: Arrow down at last line (no-op)
- [ ] Test: Move through lines of different lengths
- [ ] Verify: Cursor movement correct, column preservation works

### 5.5: Readline Shortcuts Testing
- [ ] Test: Ctrl+A (beginning of line) from various positions
- [ ] Test: Ctrl+E (end of line) from various positions
- [ ] Test: Ctrl+K (kill to end of line)
- [ ] Test: Ctrl+U (kill entire line) in multi-line text
- [ ] Test: Ctrl+W (delete word backward) with multiple words
- [ ] Test: Ctrl+W with punctuation and spaces
- [ ] Verify: All shortcuts work as expected

### 5.6: Edge Cases and Stress Testing
- [ ] Test: Empty workspace operations
- [ ] Test: Very long lines (> 500 chars)
- [ ] Test: Many newlines (> 50 lines)
- [ ] Test: Rapid typing
- [ ] Test: Rapid cursor movement
- [ ] Test: Rapid backspace/delete
- [ ] Verify: No crashes, no visual glitches

### 5.7: Terminal Restoration Testing
- [ ] Test: Exit with Ctrl+C (normal)
- [ ] Test: Resize terminal while running
- [ ] Test: Switch to another terminal and back
- [ ] Verify: Terminal restores to normal mode cleanly

### 5.8: Document Manual Test Results
- [ ] Create file: `phase-2-manual-test-results.md`
- [ ] Document: All test results with date/time
- [ ] Document: Any issues found
- [ ] Document: Terminal emulator used (for reference)

### 5.9: Polish - Code Formatting
- [ ] Run: `make fmt`
- [ ] Review: Any formatting changes
- [ ] Commit: Formatting changes if any

### 5.10: Polish - Final Quality Checks
- [ ] Run: `make check` (all tests must pass)
- [ ] Run: `make lint` (all complexity checks must pass)
- [ ] Run: `make coverage` (100% coverage required)
- [ ] Run: `make check-dynamic` (ASan, UBSan, TSan)
- [ ] Fix: Any issues found, re-run all checks

---

## Phase 2 Completion Checklist

### Code Complete
- [ ] All Tasks 1-5 subtasks completed
- [ ] Render frame helper implemented with tests
- [ ] Process action helper implemented with tests
- [ ] Multi-line cursor movement implemented with tests
- [ ] Readline-style shortcuts implemented with tests
- [ ] Main event loop implemented with tests
- [ ] main.c entry point updated

### Quality Gates
- [ ] `make fmt` - code formatted
- [ ] `make check` - all tests pass (100%)
- [ ] `make lint` - complexity checks pass
- [ ] `make coverage` - 100% coverage (lines, functions, branches)
- [ ] `make check-dynamic` - all sanitizer checks pass
- [ ] No uncovered lines: `grep "^DA:" coverage/coverage.info | grep ",0$"` returns nothing
- [ ] No uncovered branches: `grep "^BRDA:" coverage/coverage.info | grep ",0$"` returns nothing

### Manual Testing
- [ ] All manual tests from Task 5 completed and documented
- [ ] No crashes, visual glitches, or unexpected behavior
- [ ] Terminal restoration verified

### Documentation
- [ ] Manual test results documented
- [ ] Any deviations from plan documented
- [ ] Update docs/repl/repl-phase-2.md if needed

### Ready for Commit
- [ ] All quality checks pass
- [ ] Manual testing complete
- [ ] Code reviewed for:
  - Security issues (injection, buffer overflows)
  - Memory leaks (talloc hierarchy)
  - Error handling completeness
  - Code style consistency
