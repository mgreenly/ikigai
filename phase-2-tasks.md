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
  - 2.6.11-2.6.12: delete_word_backward (Ctrl+W) (e9e5404 - LCOV +1 → 162 total)
  - 2.6.13: REPL integration (326098a - added 5 action handlers + repl_readline_test.c)
  - 2.6.14: Verified complete (coverage: 1257 lines, 103 functions, 467 branches, 162 LCOV)

---

## Task 3: Main Event Loop

**Goal**: Move event loop from client.c into testable `ik_repl_run()` function in repl module.

**Architecture Decision**:
- client.c should contain ONLY main() with coordination logic (no helpers)
- All testable logic belongs in repl.c module
- main() will be excluded from coverage via LCOV_EXCL_START/STOP markers
- Event loop will be unit tested via mockable read() wrapper (Option A)

**Current State**:
- Event loop exists in src/client.c:main() (lines 140-176)
- Two static helpers in client.c duplicate repl.c functions:
  - `process_action()` → already exists as `ik_repl_process_action()` (tested)
  - `render_frame()` → already exists as `ik_repl_render_frame()` (tested)

### 3.1: Add ik_read_wrapper() to wrapper.h
- [ ] Add declaration to `src/wrapper.h`:
  - `MOCKABLE ssize_t ik_read_wrapper(int fd, void *buf, size_t count);`
  - Follow existing pattern (see ik_write_wrapper for reference)
  - Add both NDEBUG inline definition and weak symbol declaration
- [ ] Add implementation to `src/wrapper.c`:
  - Non-NDEBUG implementation that calls `read()`
- [ ] Build: `make clean && make build/ikigai`
- [ ] **Green**: Compiles successfully

### 3.2: Implement ik_repl_run() - Red Step
- [ ] Write test first: `tests/unit/repl/repl_run_test.c`
  - Test: `test_repl_run_simple_char_input()`
    - Mock read to inject "a\x03" (char 'a' + Ctrl+C)
    - Verify workspace contains "a" after run
    - Verify quit flag is true
  - Create test helper for mocking read:
    ```c
    static const char *mock_input = NULL;
    static size_t mock_input_pos = 0;
    ssize_t ik_read_wrapper(int fd, void *buf, size_t count) { ... }
    ```
- [ ] Build and run test: `make build/tests/unit/repl/repl_run_test && ./build/tests/unit/repl/repl_run_test`
- [ ] **Red**: Test fails (ik_repl_run still returns stub)

### 3.3: Implement ik_repl_run() - Green Step
- [ ] Implement `ik_repl_run()` in `src/repl.c`:
  - Initial render: `ik_repl_render_frame(repl)`
  - Main loop while (!repl->quit):
    - Read byte: `ik_read_wrapper(repl->term->tty_fd, &byte, 1)`
    - Check for EOF/error (n <= 0)
    - Parse: `ik_input_parse_byte(repl->input_parser, byte, &action)`
    - Process: `ik_repl_process_action(repl, &action)`
    - Re-render if action != UNKNOWN: `ik_repl_render_frame(repl)`
  - Error handling: return immediately on any error
  - Return: OK(NULL) on clean exit
- [ ] Build and run test
- [ ] **Green**: Test passes

### 3.4: Write Additional Tests
- [ ] Test: `test_repl_run_multiple_chars()`
  - Input: "abc\x03"
  - Verify: workspace = "abc", quit = true
- [ ] Test: `test_repl_run_with_newline()`
  - Input: "hi\n\x03"
  - Verify: workspace = "hi\n"
- [ ] Test: `test_repl_run_with_backspace()`
  - Input: "ab\x7f\x03" (a, b, backspace, Ctrl+C)
  - Verify: workspace = "a"
- [ ] Test: `test_repl_run_read_eof()`
  - Mock read returns 0 immediately
  - Verify: returns OK, quit = false (natural EOF)
- [ ] Test: `test_repl_run_read_error()`
  - Mock read returns -1
  - Verify: returns OK, breaks out of loop
- [ ] Test: `test_repl_run_parse_error()` (if possible)
  - Inject invalid UTF-8 sequence
  - Verify: returns ERR from parse
- [ ] Test: `test_repl_run_unknown_action()`
  - Input: "\x1b[99~\x03" (unknown escape sequence + Ctrl+C)
  - Verify: no re-render for UNKNOWN, continues loop
- [ ] Run: `make check`
- [ ] **Green**: All tests pass

### 3.5: Simplify client.c to Pure Coordination
- [ ] Delete static helpers from client.c:
  - Remove `process_action()` function (lines 16-43)
  - Remove `render_frame()` function (lines 46-70)
- [ ] Replace main() body with simple coordination:
  ```c
  /* LCOV_EXCL_START */
  int main(void)
  {
      void *root_ctx = talloc_new(NULL);
      if (!root_ctx) {
          fprintf(stderr, "Failed to create talloc context\n");
          return EXIT_FAILURE;
      }

      ik_repl_ctx_t *repl = NULL;
      res_t result = ik_repl_init(root_ctx, &repl);
      if (is_err(&result)) {
          error_fprintf(stderr, result.err);
          talloc_free(root_ctx);
          return EXIT_FAILURE;
      }

      result = ik_repl_run(repl);

      ik_repl_cleanup(repl);
      talloc_free(root_ctx);

      return is_ok(&result) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  /* LCOV_EXCL_STOP */
  ```
- [ ] Update includes (remove unneeded: terminal.h, input.h, workspace.h, render_direct.h, logger.h)
- [ ] Build: `make clean && make build/ikigai`
- [ ] **Green**: Compiles successfully
- [ ] Smoke test: `./build/ikigai` (type char, Ctrl+C to exit)

### 3.6: Verify Quality Gates
- [ ] Run: `make fmt` (format code)
- [ ] Run: `make check` (all tests pass)
- [ ] Run: `make lint` (complexity checks pass)
- [ ] Run: `make coverage` (100% coverage)
  - Note: LCOV exclusion count will increase by +2 (START/STOP in main)
  - Expected: 162 → 164 LCOV exclusions
- [ ] Check: no uncovered lines or branches in repl.c
- [ ] Verify: client.c shows 0% coverage (entire main excluded)

### 3.7: Verify Task 3 Complete
- [ ] All subtasks 3.1-3.6 completed
- [ ] `ik_read_wrapper()` added to wrapper.h/c
- [ ] `ik_repl_run()` implemented with event loop
- [ ] Unit tests cover: happy path, error paths, edge cases
- [ ] client.c simplified to ~25 lines (just main)
- [ ] All quality gates pass
- [ ] Coverage: 100% (lines, functions, branches)
- [ ] LCOV exclusions: 164 total (+2 from Task 2.6)

---

## Task 4: Main Entry Point

**Note**: Task 4 is completed as part of Task 3.5. The client.c simplification happens during Task 3 implementation.

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
