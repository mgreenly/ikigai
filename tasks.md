# Phase 0 Tasks: Foundation ✅ COMPLETE

All Phase 0 tasks completed with 100% test coverage.

---

# Phase 1 Tasks: Simple Dynamic Zone REPL

See [docs/repl/README.md](docs/repl/README.md) for complete design.

**Goal**: Build minimal interactive terminal with just a workspace (no scrollback buffer). Validate terminal fundamentals: raw mode, UTF-8, cursor handling, vterm rendering.

**What we defer to Phase 2**: Scrollback buffer, viewport scrolling, separator line, mouse input

---

## Task 1: Terminal Module ✅ COMPLETE

Terminal module with raw mode, alternate screen, and size detection. 100% coverage via MOCKABLE wrappers.

**Files**: `src/terminal.h`, `src/terminal.c`, `tests/unit/terminal/terminal_test.c`

## Task 2: Input Parser Module ✅ COMPLETE

Parser for raw bytes → semantic actions (char, arrows, Ctrl+C). Full UTF-8 support with security hardening against overlong encodings, surrogates, and out-of-range codepoints. 100% coverage.

**Files**: `src/input.h`, `src/input.c`, `tests/unit/input/*.c`, `tests/integration/utf8_security_integration_test.c`

## Task 3: Workspace (Text Buffer) ✅ COMPLETE

Text buffer with UTF-8 insert/delete operations and byte/grapheme cursor tracking. 100% coverage.

**Files**: `src/workspace.h`, `src/workspace.c`, `tests/unit/workspace/*.c`

## Task 4: Cursor Management ✅ COMPLETE

Cursor tracking with byte + grapheme offsets using libutf8proc for grapheme cluster boundaries. Integrated with workspace. 100% coverage.

**Files**: `src/cursor.h`, `src/cursor.c`, `tests/unit/cursor/*.c`

---

## Task 5: Rendering Module ✅ COMPLETE

Vterm-based rendering with compose + blit pattern. UTF-8 output to terminal. 100% coverage.

**Files**: `src/render.h`, `src/render.c`, `tests/unit/render/render_test.c`

## Task 6: Main REPL Context and Event Loop

**Files**: `src/repl.h`, `src/repl.c`, `tests/integration/repl_test.c`, `src/main.c`

### Steps 1-3: REPL Init/Cleanup ✅ COMPLETE

REPL context integrates terminal, render, workspace, and input parser. Comprehensive mocking for testing without TTY. 100% coverage.

---

### Step 4: Render Frame

- [ ] Add helper function `ik_repl_render_frame()`:
  - Clear render context
  - Get text from workspace
  - Write text to render context
  - Get cursor position from workspace
  - Calculate screen position for cursor (account for wrapping)
  - Set cursor in render context
  - Blit to screen
- [ ] This will primarily be tested via manual testing
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 5: Process Input Action

- [ ] Add helper function `ik_repl_process_action()`:
  - Handle each action type:
    - `IK_INPUT_CHAR` → insert codepoint into workspace
    - `IK_INPUT_NEWLINE` → insert newline into workspace
    - `IK_INPUT_BACKSPACE` → backspace in workspace
    - `IK_INPUT_DELETE` → delete in workspace
    - `IK_INPUT_ARROW_LEFT` → move cursor left
    - `IK_INPUT_ARROW_RIGHT` → move cursor right
    - `IK_INPUT_ARROW_UP` → (defer to Phase 2, no-op for now)
    - `IK_INPUT_ARROW_DOWN` → (defer to Phase 2, no-op for now)
    - `IK_INPUT_CTRL_C` → set quit flag
    - `IK_INPUT_UNKNOWN` → ignore
- [ ] Write tests for each action type
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 6: Main Event Loop

- [ ] Implement `ik_repl_run()`:
  - Initial render
  - Loop until quit:
    - Read bytes from terminal (blocking read)
    - For each byte:
      - Parse byte with input parser
      - Process resulting action
    - Render frame after processing input
- [ ] This is primarily integration/manual testing
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 7: Main Entry Point

- [ ] Rename `src/client.c` to `src/main.c` (preserve demo work as basis)
- [ ] Refactor `main()` to use REPL context:
  - Initialize REPL
  - Run REPL
  - Cleanup REPL
  - Return 0 on success, 1 on error
- [ ] Update Makefile: `src/main.c` builds to `ikigai` executable
- [ ] Run quality gates: `make check`, `make lint`, `make coverage`

### Step 8: Final Demo and Polish

- [ ] Build and comprehensive manual test:
  - `make && ./ikigai`
  - Test all features from manual testing checklist (see below)
  - Verify proper cleanup on exit
- [ ] Clean up any debug output from earlier demos
- [ ] Ensure code follows project style
- [ ] Run `make fmt`
- [ ] Commit work: "Complete Phase 1 REPL with full integration"

---

## Final Quality Gates and Manual Testing

### Quality Gates

- [ ] All tasks complete with 100% test coverage
- [ ] `make check` passes (all tests)
- [ ] `make lint` passes (complexity under threshold)
- [ ] `make coverage` shows 100% coverage (Lines, Functions, Branches)
- [ ] Run `make fmt` before committing

### Manual Testing

Perform manual testing to validate the complete system:

- [ ] **Launch and basic operation**:
  - Launch `./ikigai`
  - Verify alternate screen activated
  - Type some text, verify it appears
  - Exit with Ctrl+C, verify terminal restored cleanly

- [ ] **UTF-8 handling**:
  - Type emoji: 🎉 👨‍👩‍👧‍👦
  - Verify they display correctly
  - Type combining characters: e + ´ = é
  - Use left/right arrows through multi-byte characters
  - Verify cursor moves by whole grapheme clusters

- [ ] **Text editing**:
  - Type "hello world"
  - Move cursor to middle with arrow keys
  - Insert characters in middle
  - Backspace and delete characters
  - Verify editing works correctly

- [ ] **Multi-line input**:
  - Type text
  - Press Enter to insert newline
  - Continue typing on next line
  - Verify text wraps correctly
  - Use arrow keys to move around (left/right only, up/down no-op for now)

- [ ] **Edge cases**:
  - Fill entire screen with text
  - Verify wrapping continues to work
  - Try very long lines
  - Try rapid typing
  - Try holding down arrow keys

---

**Development Approach**: Strict TDD red/green cycle
1. Red: Write failing test first (verify it fails)
2. Green: Write minimal code to pass the test
3. Verify: Run `make check`, `make lint`, `make coverage`

**Zero Technical Debt**: Fix any deficiencies immediately as discovered.
