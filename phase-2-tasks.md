# Phase 2 Detailed Task List

**Goal**: Complete REPL Event Loop with direct rendering (workspace only, no scrollback)

**Reference**: docs/repl/repl-phase-2.md

**Current State** (as of commit 41f2677):

**Phase 1** ✅ COMPLETE
- render_direct module: Direct terminal rendering without vterm (src/render_direct.c)

**Phase 2 Completed**:
- ✅ **Task 1**: ik_repl_render_frame() - REPL rendering helper (99c4490)
- ✅ **Task 2**: ik_repl_process_action() - Input action processing (coverage: 1074 lines, 91 functions, 373 branches)
- ✅ **Task 2.5**: Multi-line cursor navigation (cursor_up/down) (672df9b)
  - 2.5.14: LCOV exclusion reduction (-8 markers)
  - 2.5.15: File size fixes (workspace.c → workspace.c + workspace_multiline.c)
  - 2.5.16: UTF-8 contract enforcement with abort()
- ✅ **Task 2.6.1-2.6.2**: Input actions for Ctrl+A/E/K/U/W (99a5bf7)
- ✅ **Task 2.6.3-2.6.4**: cursor_to_line_start (Ctrl+A)
- ✅ **Task 2.6.4.1**: Cursor module refactor (workspace-internal, void+assertions) (0456140, LCOV -10)
- ✅ **Task 2.6.5-2.6.6**: cursor_to_line_end (Ctrl+E) (bcdaf48)
- ✅ **Task 2.6.7-2.6.8**: kill_to_line_end (Ctrl+K) (5908d58)
- ✅ **Task 2.6.8.1**: Coverage gaps fixed (1adc72e, d7fc09e, LCOV +2 → 160 total)

**Phase 2 Remaining**:
- ⏳ **Task 2.6.9-2.6.10**: kill_line (Ctrl+U) - **NEXT**
- ⏳ **Task 2.6.11-2.6.12**: delete_word_backward (Ctrl+W)
- ⏳ **Task 2.6.13**: Integration - add all shortcuts to ik_repl_process_action()
- ⏳ **Task 3**: Implement event loop (`ik_repl_run()` - currently stubbed)
- ⏳ **Task 4**: Simplify client.c main() to use REPL module
- ⏳ **Task 5**: Manual testing and polish

**Current Coverage**: 100% (1201/1201 lines, 100/100 functions, 427/427 branches)

**Working Demo**: `make build/ikigai && ./build/ikigai` - src/client.c has complete REPL with event loop (lines 72-182)

**Workspace Operations**: insert_codepoint, insert_newline, backspace, delete, cursor_left, cursor_right, cursor_up, cursor_down, cursor_to_line_start, cursor_to_line_end, kill_to_line_end

**Input Actions**: CHAR, NEWLINE, BACKSPACE, DELETE, ARROW_LEFT, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, CTRL_C, CTRL_A, CTRL_E, CTRL_K, CTRL_U, CTRL_W

---

## Remaining Tasks

### 2.6.9: Kill Line - Write Tests
- [ ] Write test: `test_workspace_kill_line_basic()`
  - Setup: "hello\nworld\ntest", cursor in middle of "world"
  - Action: `ik_workspace_kill_line()`
  - Assert: text is "hello\ntest", cursor at start of "test" line
- [ ] Write test: `test_workspace_kill_line_first_line()`
- [ ] Write test: `test_workspace_kill_line_last_line()`
- [ ] Write test: `test_workspace_kill_line_empty_line()`
- [ ] **Red**: Tests fail

### 2.6.10: Kill Line - Implementation
- [ ] Add to `src/workspace.h`: `res_t ik_workspace_kill_line(ik_workspace_t *ws);`
- [ ] Implement in `src/workspace.c`:
  - Find line start (previous \n or start)
  - Find line end (next \n or end)
  - Delete entire line including \n
  - Position cursor at new line start
- [ ] **Green**: Tests pass

### 2.6.11: Delete Word Backward - Write Tests
- [ ] Write test: `test_workspace_delete_word_backward_basic()`
  - Setup: "hello world test", cursor after "test"
  - Action: `ik_workspace_delete_word_backward()`
  - Assert: text is "hello world ", cursor after "world "
- [ ] Write test: `test_workspace_delete_word_backward_at_word_boundary()`
- [ ] Write test: `test_workspace_delete_word_backward_multiple_spaces()`
- [ ] Write test: `test_workspace_delete_word_backward_punctuation()`
- [ ] Write test: `test_workspace_delete_word_backward_utf8()`
- [ ] Write test: `test_workspace_delete_word_backward_at_start()`
- [ ] **Red**: Tests fail

### 2.6.12: Delete Word Backward - Implementation
- [ ] Add to `src/workspace.h`: `res_t ik_workspace_delete_word_backward(ik_workspace_t *ws);`
- [ ] Implement in `src/workspace.c`:
  - Scan backward from cursor
  - Skip trailing whitespace
  - Find word boundary (whitespace, punctuation, newline)
  - Delete from boundary to cursor
  - Must be UTF-8 aware
- [ ] **Green**: Tests pass

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

---

## Notes

**TDD Discipline**: Every subtask follows Red/Green/Verify cycle:
1. **Red**: Write test first, verify it fails
2. **Green**: Implement minimal code to pass, verify it passes
3. **Verify**: Run full quality checks

**Coverage Requirement**: 100% coverage is absolute. Fix all gaps before moving to next task.

**Zero Technical Debt**: Fix issues immediately when discovered. Don't defer.

**Manual Testing**: Use `./build/ikigai` to manually verify functionality at key milestones.

**Performance**: Not a concern for Phase 2 (workspace only, small text). Defer optimizations to Phase 3/4.

---

## Quick Reference

### Source Code Structure

**Existing Modules** (already implemented and tested):
- `src/terminal.{c,h}` - Terminal raw mode, alternate screen (Phase 0)
- `src/render_direct.{c,h}` - Direct terminal rendering without vterm (Phase 1)
- `src/workspace.{c,h}` - Text buffer with cursor (Phase 0, needs extensions)
- `src/workspace_cursor.{c,h}` - Grapheme-aware cursor positioning (Phase 0, workspace-internal)
- `src/input.{c,h}` - Input parser for escape sequences (Phase 0, needs extensions)
- `src/byte_array.{c,h}` - Dynamic byte array (Phase 0)
- `src/wrapper.{c,h}` - MOCKABLE wrappers for testability (Phase 0)

**REPL Module** (partially implemented):
- `src/repl.{c,h}` - REPL context and event loop
  - ✅ `ik_repl_init()` - Initialize REPL (done)
  - ✅ `ik_repl_cleanup()` - Cleanup REPL (done)
  - ⏳ `ik_repl_run()` - Event loop (stub, needs implementation)
  - ❌ `ik_repl_render_frame()` - Render helper (missing, exists in client.c)
  - ❌ `ik_repl_process_action()` - Action handler (missing, exists in client.c)

**Client/Main** (working demo):
- `src/client.c` - Working REPL demo (will be simplified to use repl module)

**Test Structure**:
- `tests/unit/terminal/` - Terminal tests ✅
- `tests/unit/render_direct/` - Render tests ✅
- `tests/unit/workspace/` - Workspace tests ✅ (will add more)
- `tests/unit/workspace_cursor/` - Cursor tests ✅ (workspace-internal module)
- `tests/unit/input/` - Input parser tests ✅ (will add more)
- `tests/unit/repl/` - REPL tests ✅

### Key Functions to Reference

**From client.c** (to be moved to repl.c):
```c
// Lines 46-70: render_frame() → ik_repl_render_frame()
static res_t render_frame(ik_render_direct_ctx_t *render,
                          ik_workspace_t *workspace)

// Lines 16-43: process_action() → ik_repl_process_action()
static res_t process_action(ik_workspace_t *workspace,
                            const ik_input_action_t *action,
                            bool *should_exit_out)

// Lines 140-176: event loop → ik_repl_run()
```

**Workspace API** (already implemented):
```c
res_t ik_workspace_insert_codepoint(ik_workspace_t *ws, uint32_t cp);
res_t ik_workspace_insert_newline(ik_workspace_t *ws);
res_t ik_workspace_backspace(ik_workspace_t *ws);
res_t ik_workspace_delete(ik_workspace_t *ws);
res_t ik_workspace_cursor_left(ik_workspace_t *ws);
res_t ik_workspace_cursor_right(ik_workspace_t *ws);
res_t ik_workspace_get_text(ik_workspace_t *ws, char **text_out, size_t *len_out);
res_t ik_workspace_get_cursor_position(ik_workspace_t *ws, size_t *byte_out, size_t *grapheme_out);
```

**Workspace API** (to be added):
```c
// Task 2.5: Multi-line cursor movement
res_t ik_workspace_cursor_up(ik_workspace_t *ws);
res_t ik_workspace_cursor_down(ik_workspace_t *ws);

// Task 2.6: Readline shortcuts
res_t ik_workspace_cursor_to_line_start(ik_workspace_t *ws);
res_t ik_workspace_cursor_to_line_end(ik_workspace_t *ws);
res_t ik_workspace_kill_to_line_end(ik_workspace_t *ws);
res_t ik_workspace_kill_line(ik_workspace_t *ws);
res_t ik_workspace_delete_word_backward(ik_workspace_t *ws);
```

**Input Actions** (already defined in src/input.h):
```c
typedef enum {
    IK_INPUT_CHAR,        // ✅ Implemented
    IK_INPUT_NEWLINE,     // ✅ Implemented
    IK_INPUT_BACKSPACE,   // ✅ Implemented
    IK_INPUT_DELETE,      // ✅ Implemented
    IK_INPUT_ARROW_LEFT,  // ✅ Implemented
    IK_INPUT_ARROW_RIGHT, // ✅ Implemented
    IK_INPUT_ARROW_UP,    // ✅ Parsed, not yet used
    IK_INPUT_ARROW_DOWN,  // ✅ Parsed, not yet used
    IK_INPUT_CTRL_C,      // ✅ Implemented
    IK_INPUT_UNKNOWN      // ✅ Implemented
} ik_input_action_type_t;
```

**Input Actions** (to be added in Task 2.6):
```c
    IK_INPUT_CTRL_A,      // ❌ To be added
    IK_INPUT_CTRL_E,      // ❌ To be added
    IK_INPUT_CTRL_K,      // ❌ To be added
    IK_INPUT_CTRL_U,      // ❌ To be added
    IK_INPUT_CTRL_W,      // ❌ To be added
```

### Build and Test Commands

```bash
# Build everything
make build/ikigai

# Run the working demo (before REPL module completion)
./build/ikigai

# Run all tests
make check

# Run lint checks
make lint

# Generate coverage report
make coverage

# View coverage gaps
grep "^DA:" coverage/coverage.info | grep ",0$"   # Uncovered lines
grep "^BRDA:" coverage/coverage.info | grep ",0$" # Uncovered branches

# Run specific test
make build/tests/unit/workspace/workspace_test && ./build/tests/unit/workspace/workspace_test
```

### Documentation References

- **Phase 2 Overview**: docs/repl/repl-phase-2.md
- **Phase 1 (Direct Rendering)**: docs/repl/repl-phase-1.md
- **Eliminate vterm Design**: docs/repl/eliminate-vterm-design.md
- **REPL Overview**: docs/repl/README.md
- **Agent Instructions**: AGENT.md (TDD discipline, coverage requirements)
