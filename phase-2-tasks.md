# Phase 2 Detailed Task List

**Goal**: Complete REPL Event Loop with direct rendering (workspace only, no scrollback)

**Reference**: docs/repl/repl-phase-2.md

**Current State**:
- Phase 1 complete: render_direct module implemented and tested (src/render_direct.c)
- Task 1 complete: ik_repl_render_frame() implemented and tested (src/repl.c)
- Task 2 complete: ik_repl_process_action() implemented and tested (src/repl.c)
- REPL init/cleanup implemented (src/repl.c)
- Event loop stubbed: `ik_repl_run()` returns OK(NULL)
- Working demo in src/client.c with process_action() and render_frame() helper functions
- Workspace has: insert_codepoint, insert_newline, backspace, delete, cursor_left, cursor_right
- Input parser supports: CHAR, NEWLINE, BACKSPACE, DELETE, ARROW_LEFT, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, CTRL_C
- **Missing**: cursor_up/down, readline shortcuts (Ctrl+A/E/K/U/W), event loop implementation

## Key Insights

**What's Already Working**:
- src/client.c (lines 72-182) has a complete, working REPL implementation
- The event loop (lines 140-176) reads bytes, parses, processes actions, and renders
- Helper functions process_action() (lines 16-43) and render_frame() (lines 46-70) work correctly
- The demo can be run: `make build/ikigai && ./build/ikigai`

**What We Need to Do**:
1. **Move logic from client.c to repl.c**: Extract helper functions into REPL module
   - `process_action()` → `ik_repl_process_action()`
   - `render_frame()` → `ik_repl_render_frame()`
   - Event loop → `ik_repl_run()`

2. **Add missing workspace functions**: Implement new features not in client.c demo
   - Task 2.5: `cursor_up()`, `cursor_down()` for multi-line editing
   - Task 2.6: Readline shortcuts (Ctrl+A/E/K/U/W)

3. **Write comprehensive tests**: Add unit tests for REPL module functions
   - Tests for render_frame helper
   - Tests for process_action helper
   - Tests for new workspace functions (up/down, readline shortcuts)

4. **Simplify client.c**: Once REPL module is complete, simplify main() to just call ik_repl_init/run/cleanup

**Testing Strategy Note**:
- The event loop in `ik_repl_run()` is thin glue code (no complex logic)
- Full coverage may require mockable wrappers for read() or acceptance that integration testing suffices
- Component functions (render_frame, process_action) should have 100% unit test coverage
- The working client.c provides end-to-end integration verification

---

## Task 1: Render Frame Helper

**Note**: The logic already exists in src/client.c:render_frame() (lines 46-70). We need to move this into the REPL module.

### 1.1: Write Test - Successful Render (Empty Workspace)
- [x] Create `tests/unit/repl/` directory
- [x] Create `tests/unit/repl/repl_test.c` with test infrastructure
- [x] Write test: `test_repl_render_frame_empty_workspace()`
  - Setup: create REPL context with empty workspace
  - Action: call `ik_repl_render_frame(repl)`
  - Assert: returns OK result
  - Verify: render_direct_workspace called with correct parameters (may need mockable wrapper)
- [x] **Red**: Verify test fails (function doesn't exist in repl module)
- [x] Run: `make build/tests/unit/repl/repl_test && ./build/tests/unit/repl/repl_test`

### 1.2: Implement - Minimal Function Signature
- [x] Add to `src/repl.h`: `res_t ik_repl_render_frame(ik_repl_ctx_t *repl);`
- [x] Add to `src/repl.c`: stub implementation returning OK
- [x] Update Makefile if needed for repl_test
- [x] **Green**: Verify test passes
- [x] Run: `make build/tests/unit/repl/repl_test && ./build/tests/unit/repl/repl_test`

### 1.3: Implement - Actual Render Logic
- [x] Implement `ik_repl_render_frame()` in `src/repl.c`:
  - Copy logic from src/client.c:render_frame() (lines 46-70)
  - Get workspace text via `ik_workspace_get_text()`
  - Get cursor position via `ik_workspace_get_cursor_position()`
  - Call `ik_render_direct_workspace(repl->render, text, len, cursor_byte_offset)`
  - Return result
  - **Note**: Removed unreachable error checks (workspace getters always return OK)
- [x] **Green**: Verify all tests pass
- [x] Run: `make build/tests/unit/repl/repl_test && ./build/tests/unit/repl/repl_test`

### 1.4: Write Test - Render with Multi-line Text
- [x] Write test: `test_repl_render_frame_multiline()`
  - Setup: insert multi-line text into workspace (text with \n)
  - Action: call `ik_repl_render_frame(repl)`
  - Assert: returns OK, correct text and cursor passed to render
- [x] **Green**: Verify test passes (implementation already handles this)
- [x] Run: `make build/tests/unit/repl/repl_test && ./build/tests/unit/repl/repl_test`

### 1.5: Write Test - Render with Cursor at Various Positions
- [x] Write test: `test_repl_render_frame_cursor_positions()`
  - Test cursor at: start, middle, end, after multi-byte char
  - Assert correct cursor_byte_offset passed to render
- [x] **Green**: Verify test passes
- [x] Run: `make build/tests/unit/repl/repl_test && ./build/tests/unit/repl/repl_test`

### 1.6: Write Test - Error Handling (Write Failure)
- [x] Write test: `test_repl_render_frame_utf8()` (UTF-8 handling instead)
  - Setup: workspace with UTF-8 emoji
  - Action: call `ik_repl_render_frame(repl)`
  - Assert: returns OK, rendering succeeds
  - **Note**: Error paths not testable (getters always return OK)
- [x] Write test: `test_repl_render_frame_null_repl_asserts()` (NULL assertion test)
- [x] **Green**: Verify test passes
- [x] Run: `make build/tests/unit/repl/repl_test && ./build/tests/unit/repl/repl_test`

### 1.7: Verify Task 1 Complete
- [x] Run: `make check` (all tests pass)
- [x] Run: `make lint` (complexity checks pass)
- [x] Run: `make coverage` (100% coverage: 1056/1056 lines, 90/90 functions, 359/359 branches)
- [x] Check coverage: `grep "^DA:" coverage/coverage.info | grep ",0$"` (no uncovered lines)
- [x] Check coverage: `grep "^BRDA:" coverage/coverage.info | grep ",0$"` (no uncovered branches)
- [x] **Commit**: 99c4490 "Implement REPL render_frame helper function (Phase 2 Task 1)"

---

## Task 2: Process Input Action Helper

**Note**: The logic already exists in src/client.c:process_action() (lines 16-43). We need to move this into the REPL module and adjust the API (use repl->quit instead of should_exit_out parameter).

### 2.1: Write Test - CHAR Action
- [x] Write test: `test_repl_process_action_char()` in repl_action_test.c
  - Setup: REPL with empty workspace
  - Action: process `IK_INPUT_CHAR` with codepoint 'a'
  - Assert: workspace contains 'a', cursor moved
- [x] **Red**: Verify test fails (function doesn't exist)
- [x] Run: `make build/tests/unit/repl/repl_action_test && ./build/tests/unit/repl/repl_action_test`

### 2.2: Implement - Minimal Function Signature
- [x] Add to `src/repl.h`: `res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action);`
- [x] Add to `src/repl.c`: copy logic from src/client.c:process_action() (lines 16-43)
  - Change signature: remove should_exit_out parameter
  - Use `repl->quit = true` for CTRL_C instead of setting out parameter
  - Call workspace functions via `repl->workspace`
- [x] **Green**: Verify test passes
- [x] Run: `make build/tests/unit/repl/repl_action_test && ./build/tests/unit/repl/repl_action_test`

### 2.3: Write Tests - Basic Actions
- [x] Write test: `test_repl_process_action_newline()`
- [x] Write test: `test_repl_process_action_backspace()`
- [x] Write test: `test_repl_process_action_delete()`
- [x] Write test: `test_repl_process_action_arrow_left()`
- [x] Write test: `test_repl_process_action_arrow_right()`
- [x] **Green**: Verify tests pass (implementation already handles these)
- [x] Run tests

### 2.4: Write Test - Ctrl+C Quit Flag
- [x] Write test: `test_repl_process_action_ctrl_c()`
  - Setup: REPL with quit=false
  - Action: process `IK_INPUT_CTRL_C`
  - Assert: repl->quit == true
- [x] **Green**: Verify test passes (implementation already handles this)

### 2.5: Write Tests - Edge Cases
- [x] Write test: `test_repl_process_action_backspace_at_start()`
- [x] Write test: `test_repl_process_action_delete_at_end()`
- [x] Write test: `test_repl_process_action_left_at_start()`
- [x] Write test: `test_repl_process_action_right_at_end()`
- [x] **Green**: Verify tests pass (workspace already handles edge cases)

### 2.6: Write Test - Unknown Action
- [x] Write test: `test_repl_process_action_unknown()`
  - Setup: REPL with workspace
  - Action: process `IK_INPUT_UNKNOWN`
  - Assert: returns OK, workspace unchanged
- [x] **Green**: Verify test passes

### 2.7: Fix File Size Lint Error
- [x] Split repl_test.c (753 lines) into two files:
  - repl_render_test.c (247 lines) - render_frame tests
  - repl_action_test.c (489 lines) - process_action tests
- [x] Update LCOV_EXCL_COVERAGE from 152 to 154 (added 2 assertions)
- [x] Both files now under 500-line limit

### 2.8: Verify Task 2 Complete
- [x] Run: `make check` (all tests pass)
- [x] Run: `make lint` (complexity checks pass)
- [x] Run: `make coverage` (100% coverage: 1074/1074 lines, 91/91 functions, 373/373 branches)
- [x] Check: no uncovered lines or branches

---

## Task 2.5: Multi-line Cursor Movement ✅ COMPLETE

### 2.5.1: Write Test - Cursor Up (Basic)
- [x] Write test: `test_workspace_cursor_up_basic()` in `tests/unit/workspace/cursor_movement_test.c`
  - Setup: workspace with "line1\nline2\nline3", cursor at start of line2
  - Action: call `ik_workspace_cursor_up(ws)`
  - Assert: cursor now on line1, same column position
- [x] **Red**: Verify test fails (function doesn't exist)
- [x] Run: `make build/tests/unit/workspace/workspace_test && ./build/tests/unit/workspace/workspace_test`

### 2.5.2: Implement - Minimal Function Signatures
- [x] Add to `src/workspace.h`:
  ```c
  res_t ik_workspace_cursor_up(ik_workspace_t *ws);
  res_t ik_workspace_cursor_down(ik_workspace_t *ws);
  ```
- [x] Add to `src/workspace.c`: stub implementations returning OK
- [x] **Green**: Verify test passes (but cursor doesn't actually move yet)

### 2.5.3: Design - Algorithm for Up/Down Movement
- [x] Document algorithm (in comments):
  - Calculate current (row, col) from byte offset
  - For up: find start of previous line, position at same column (or line end)
  - For down: find start of next line, position at same column (or line end)
  - Track preferred column for vertical movement
- [x] Consider: need to add `preferred_column` to workspace or cursor structure?
- [x] Decision: check if cursor structure already tracks this (decision: not needed, calculate on-demand)

### 2.5.4: Write Helper Tests - Find Line Boundaries
- [x] If needed, write tests for helper: `find_line_start(text, byte_offset)` (via integration tests)
- [x] If needed, write tests for helper: `find_prev_line_start(text, byte_offset)` (not needed, inline logic)
- [x] If needed, write tests for helper: `find_next_line_start(text, byte_offset)` (not needed, inline logic)
- [x] **Red**: Tests fail
- [x] Implement helpers as static functions in workspace.c
- [x] **Green**: Helper tests pass

### 2.5.5: Write Helper Tests - Calculate Row/Col from Offset
- [x] If needed, write tests for helper: `calculate_row_col(text, byte_offset, row_out, col_out)` (via integration tests)
- [x] Test: handles \n correctly
- [x] Test: handles wrapped lines (long lines) (deferred - rendering concern)
- [x] Test: handles UTF-8 multi-byte characters
- [x] **Red**: Tests fail
- [x] Implement helper (count_graphemes, grapheme_to_byte_offset)
- [x] **Green**: Tests pass

### 2.5.6: Implement - Cursor Up Logic
- [x] Implement `ik_workspace_cursor_up()`:
  - Get current byte offset
  - Find current line start
  - If at first line, return OK (no-op)
  - Find previous line start
  - Calculate column position on current line
  - Position cursor at same column on previous line (or line end)
  - Update cursor byte offset
- [x] **Green**: Verify test from 2.5.1 passes

### 2.5.7: Write Tests - Cursor Up Edge Cases
- [x] Write test: `test_workspace_cursor_up_from_first_line()` (no-op)
- [x] Write test: `test_workspace_cursor_up_column_preservation()`
- [x] Write test: `test_workspace_cursor_up_shorter_line()` (cursor to line end)
- [x] Write test: `test_workspace_cursor_up_empty_line()`
- [x] Write test: `test_workspace_cursor_up_utf8()` (emoji, CJK)
- [x] **Green**: All tests pass

### 2.5.8: Write Test - Cursor Down (Basic)
- [x] Write test: `test_workspace_cursor_down_basic()`
  - Setup: workspace with "line1\nline2\nline3", cursor at start of line2
  - Action: call `ik_workspace_cursor_down(ws)`
  - Assert: cursor now on line3, same column position
- [x] **Red**: Test fails (not implemented yet)

### 2.5.9: Implement - Cursor Down Logic
- [x] Implement `ik_workspace_cursor_down()`:
  - Similar to cursor_up but move to next line
  - Handle last line (no-op)
- [x] **Green**: Test passes

### 2.5.10: Write Tests - Cursor Down Edge Cases
- [x] Write test: `test_workspace_cursor_down_from_last_line()` (no-op)
- [x] Write test: `test_workspace_cursor_down_column_preservation()`
- [x] Write test: `test_workspace_cursor_down_shorter_line()`
- [x] Write test: `test_workspace_cursor_down_empty_line()`
- [x] Write test: `test_workspace_cursor_down_utf8()`
- [x] **Green**: All tests pass

### 2.5.11: Write Tests - Wrapped Lines (Future Consideration)
- [x] Write test: `test_workspace_cursor_up_wrapped_line()` (deferred)
- [x] Write test: `test_workspace_cursor_down_wrapped_line()` (deferred)
- [x] Note: Wrapping is a rendering concern, not workspace concern
- [x] Consider: Do we need terminal width in workspace for this? (no)
- [x] Decision: Document and defer if complex (Phase 2 is workspace only) - DEFERRED

### 2.5.12: Integrate - Add to Process Action
- [x] Add to `ik_repl_process_action()`:
  - `IK_INPUT_ARROW_UP` → `ik_workspace_cursor_up()`
  - `IK_INPUT_ARROW_DOWN` → `ik_workspace_cursor_down()`
- [x] Write integration test in repl_test.c (repl_action_test.c)
- [x] **Green**: Test passes

### 2.5.13: Verify Task 2.5 Complete
- [x] Run: `make check` (all tests pass)
- [x] Run: `make lint` (complexity checks pass)
- [x] Run: `make coverage` (100% coverage: 1165/1165 lines, 97/97 functions, 407/407 branches)
- [x] Check: no uncovered lines or branches
- [x] **Commit**: 672df9b "Implement REPL multi-line cursor movement (Phase 2 Task 2.5)"
- [x] **Note**: File size warnings for workspace.c (575 lines), cursor_movement_test.c (815 lines), repl_action_test.c (580 lines)
- [x] **Follow-up**: Split cursor_movement_test.c → cursor_left_right_test.c + cursor_up_down_test.c (commit 99f404c)

### 2.5.14: Review and Reduce LCOV Exclusions ✅ COMPLETE
- [x] Review the 10 non-assert LCOV exclusions added in cursor movement implementation
- [x] Defensive NULL checks (4 exclusions):
  - Line 360: `find_line_start()` - text NULL check → **ELIMINATED** (replaced with assertion)
  - Lines 383-384: `find_line_end()` - text NULL check → **ELIMINATED** (replaced with assertion)
  - Line 405: `count_graphemes()` - text NULL branch → **ELIMINATED** (replaced with assertion)
  - Line 453: `grapheme_to_byte_offset()` - text NULL branch → **ELIMINATED** (replaced with assertion)
- [x] UTF-8 character handling (4 exclusions):
  - Line 428: `count_graphemes()` - 4-byte UTF-8 else fallback → **KEPT** (valid code path)
  - Line 431: `count_graphemes()` - invalid UTF-8 fallback → **KEPT** (defensive safety)
  - Line 470: `grapheme_to_byte_offset()` - 4-byte UTF-8 else fallback → **KEPT** (valid code path)
  - Line 473: `grapheme_to_byte_offset()` - invalid UTF-8 fallback → **KEPT** (defensive safety)
- [x] Provably unreachable code (2 exclusions):
  - Line 418: `count_graphemes()` - loop invariant makes condition always true → **ELIMINATED** (restructured)
  - Lines 383-384: Already counted above
- [x] For each exclusion, determine if it can be eliminated by:
  - Restructuring code to avoid defensive checks → **DONE** (line 418 restructured)
  - Adding explicit precondition checks in callers → **N/A** (all callers internal)
  - Using assert() instead of runtime checks for truly impossible conditions → **DONE** (4 NULL checks)
- [x] Goal: Reduce exclusions where possible while maintaining code safety → **ACHIEVED**
- [x] Document decision for each exclusion that remains → **DONE** (see commit 9aea7ca)
- [x] **Results**: Eliminated 5 logical exclusions (8 markers), maintained 100% coverage
- [x] **Branch coverage increased**: 407 → 415 branches (8 more branches tested)
- [x] **Commit**: 9aea7ca "Reduce LCOV exclusions in cursor movement code (Task 2.5.14)"

### 2.5.15: File Size Corrections ✅ COMPLETE
- [x] **Problem**: File size warnings (MAX_FILE_LINES = 500)
  - workspace.c: 572 lines (72 over)
  - cursor_up_down_test.c: 587 lines (87 over)
  - repl_action_test.c: 580 lines (80 over)
- [x] **Analysis**: Identified best split points for each file
  - cursor_up_down_test.c → cursor_up_test.c + cursor_down_test.c (natural boundary at line 280)
  - repl_action_test.c → text editing tests + navigation tests (by functionality)
  - workspace.c → workspace.c + workspace_multiline.c (basic editing vs multi-line)
- [x] **Split 1**: cursor_up_down_test.c (587 lines) → 2 files
  - cursor_up_test.c: 319 lines (vertical up movement + 1 assertion)
  - cursor_down_test.c: 306 lines (vertical down movement + 1 assertion)
- [x] **Split 2**: repl_action_test.c (580 lines) → 2 files
  - repl_text_editing_test.c: 258 lines (char, newline, backspace, delete + edge cases)
  - repl_navigation_test.c: 357 lines (arrows, ctrl_c, unknown + edge cases + assertions)
- [x] **Split 3**: workspace.c (572 lines) → 2 files
  - workspace.c: 349 lines (basic editing operations)
  - workspace_multiline.c: 231 lines (multi-line navigation helpers: find_line_start, find_line_end, count_graphemes, grapheme_to_byte_offset, cursor_up, cursor_down)
- [x] Update Makefile CLIENT_SOURCES and MODULE_SOURCES for workspace_multiline.c
- [x] Verify tests pass and coverage maintained
  - make check: All tests pass ✓
  - make lint: All files under 500-line limit ✓
  - make coverage: 100% coverage (1165/1165 lines, 97/97 functions, 415/415 branches) ✓
- [x] **Commit**: 4511c66 "Split oversized files to meet 500-line limit (Task 2.5.15)"

---

## Task 2.6: Readline-Style Editing Shortcuts

### 2.6.1: Add Input Actions - Write Tests First
- [x] Write test: `test_input_parse_ctrl_a()` in `tests/unit/input/char_test.c`
- [x] Write test: `test_input_parse_ctrl_e()`
- [x] Write test: `test_input_parse_ctrl_k()`
- [x] Write test: `test_input_parse_ctrl_u()`
- [x] Write test: `test_input_parse_ctrl_w()`
- [x] **Red**: Tests fail (actions don't exist)

### 2.6.2: Add Input Actions - Implementation
- [ ] Add to `src/input.h` enum:
  - `IK_INPUT_CTRL_A`
  - `IK_INPUT_CTRL_E`
  - `IK_INPUT_CTRL_K`
  - `IK_INPUT_CTRL_U`
  - `IK_INPUT_CTRL_W`
- [ ] Update `src/input.c`: parse control characters (0x01, 0x05, 0x0B, 0x15, 0x17)
- [ ] **Green**: Input tests pass

### 2.6.3: Cursor to Line Start - Write Tests
- [ ] Write test: `test_workspace_cursor_to_line_start_basic()` in workspace_test.c
  - Setup: "hello\nworld", cursor in middle of "world"
  - Action: `ik_workspace_cursor_to_line_start()`
  - Assert: cursor at start of "world"
- [ ] Write test: `test_workspace_cursor_to_line_start_first_line()`
- [ ] Write test: `test_workspace_cursor_to_line_start_already_at_start()`
- [ ] Write test: `test_workspace_cursor_to_line_start_after_newline()`
- [ ] **Red**: Tests fail

### 2.6.4: Cursor to Line Start - Implementation
- [ ] Add to `src/workspace.h`: `res_t ik_workspace_cursor_to_line_start(ik_workspace_t *ws);`
- [ ] Implement in `src/workspace.c`:
  - Find previous \n (or start of text)
  - Position cursor after that \n (or at start)
- [ ] **Green**: Tests pass

### 2.6.5: Cursor to Line End - Write Tests
- [ ] Write test: `test_workspace_cursor_to_line_end_basic()`
- [ ] Write test: `test_workspace_cursor_to_line_end_last_line()`
- [ ] Write test: `test_workspace_cursor_to_line_end_already_at_end()`
- [ ] Write test: `test_workspace_cursor_to_line_end_before_newline()`
- [ ] **Red**: Tests fail

### 2.6.6: Cursor to Line End - Implementation
- [ ] Add to `src/workspace.h`: `res_t ik_workspace_cursor_to_line_end(ik_workspace_t *ws);`
- [ ] Implement in `src/workspace.c`:
  - Find next \n (or end of text)
  - Position cursor before that \n (or at end)
- [ ] **Green**: Tests pass

### 2.6.7: Kill to Line End - Write Tests
- [ ] Write test: `test_workspace_kill_to_line_end_basic()`
  - Setup: "hello world", cursor after "hello "
  - Action: `ik_workspace_kill_to_line_end()`
  - Assert: text is "hello ", cursor unchanged
- [ ] Write test: `test_workspace_kill_to_line_end_at_newline()`
- [ ] Write test: `test_workspace_kill_to_line_end_already_at_end()`
- [ ] Write test: `test_workspace_kill_to_line_end_multiline()`
- [ ] **Red**: Tests fail

### 2.6.8: Kill to Line End - Implementation
- [ ] Add to `src/workspace.h`: `res_t ik_workspace_kill_to_line_end(ik_workspace_t *ws);`
- [ ] Implement in `src/workspace.c`:
  - Find next \n (or end of text)
  - Delete from cursor to that position (not including \n)
- [ ] **Green**: Tests pass

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
- `src/cursor.{c,h}` - Grapheme-aware cursor positioning (Phase 0)
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
- `tests/unit/input/` - Input parser tests ✅ (will add more)
- `tests/unit/repl/` - REPL tests ❌ (needs to be created)

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
