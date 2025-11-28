# REPL Implementation Plan

## Overview

This plan implements a REPL with direct terminal rendering. We'll build incrementally with manual verification at each phase.

**Current Status**: Phase 4 COMPLETE (2025-11-14) - Viewport and scrolling fully implemented with 100% test coverage. Phase 5 (manual testing and final cleanup) remains.

**Goal**: Complete REPL with direct rendering, scrollback buffer, and viewport scrolling.

---

## Phase 0: Foundation ✅

**Goal**: Clean up error handling and build reusable infrastructure for REPL implementation.

**Status**: Complete

**Completed**:
- ✅ Error handling cleanup with talloc-integrated Result types
- ✅ Generic `ik_array_t` utility with type-safe wrappers (`ik_byte_array_t`, `ik_line_array_t`)
- ✅ Terminal module (`terminal.c`) - raw mode, ANSI escape sequences
- ✅ Input parser module (`input.c`) - parse bytes to semantic actions
- ✅ input buffer module (`input_buffer.c`) - multi-line text buffer with UTF-8 support
- ✅ REPL init/cleanup infrastructure

**Key Achievements**:
- Foundation for TDD workflow with 100% coverage requirement
- Memory management patterns established with talloc
- Generic array utility enables code reuse across modules

## Phase 1: Direct Rendering ✅

**Status**: COMPLETE - Direct ANSI terminal rendering with UTF-8 support. Single framebuffer write per frame (52× syscall reduction, 26× bytes reduction).

---

## Phase 2: Complete Basic REPL Event Loop ✅

**Goal**: Full interactive REPL with just input buffer (no scrollback).

**Status**: COMPLETE (2025-11-11)

**Completed**:
- ✅ Task 1: `ik_repl_render_frame()` - Render current input buffer state
- ✅ Task 2: `ik_repl_process_action()` - Process all input actions (char, newline, backspace, delete, arrows)
- ✅ Task 2.5: Multi-line cursor navigation (cursor_up/down with column preservation)
- ✅ Task 2.6: Readline-style editing shortcuts
  - Ctrl+A (line start), Ctrl+E (line end)
  - Ctrl+K (kill to end), Ctrl+U (kill line)
  - Ctrl+W (delete word backward with punctuation class support)
- ✅ Task 3: Main event loop
  - `ik_repl_run()` - Event loop (read → parse → process → render)
  - `ik_read_wrapper()` added to wrapper.h for testability
  - 9 comprehensive tests (basic functionality + error handling)
- ✅ Task 4: client.c simplified to pure coordination
  - Reduced from 182 lines → 32 lines
  - All logic moved to testable `repl` module
  - LCOV_EXCL markers added to main()
- ✅ Task 5: Manual testing (32 tests - 29 passed, 3 bugs found)
  - UTF-8 handling verified (emoji, CJK, combining chars)
  - Text wrapping and multi-line editing verified
  - Readline shortcuts verified
  - Terminal restoration verified
  - Results documented in docs/repl/phase-2-manual-testing-guide.md
- ✅ Task 6: Bug fixes (all 3 bugs fixed)
  - Bug 6.1 CRITICAL: Empty input buffer crashes (commit 9b32cff)
  - Bug 6.2 MEDIUM: Column preservation (commit 3c226d3)
  - Bug 6.3 LOW: Ctrl+W punctuation handling (commits 3c226d3, 4f38c6b)
- ✅ Code review: Security/memory/error handling analysis (Grade: A-)
  - 0 CRITICAL, 0 HIGH issues
  - 2 MEDIUM issues documented (theoretical overflows, commit 025491e)
  - Excellent security: no unsafe functions, proper UTF-8 validation
- ✅ Code refactoring:
  - Cursor module input buffer-internal (void+assertions, -10 LCOV markers)
  - input buffer split: `input_buffer.c` + `input_buffer_multiline.c` (file size lint compliance)
  - Test files modularized for maintainability

**Final Metrics**:
- **Coverage**: 1315 lines, 105 functions, 525 branches (all 100%)
- **LCOV exclusions**: 162/164 (within limit)
- **Quality gates**: ✅ fmt, ✅ check, ✅ lint, ✅ coverage, ✅ check-dynamic (ASan/UBSan/TSan)

**Key Achievement**: Production-ready interactive REPL with full test coverage and security validation

See [tasks.md](tasks.md) for detailed breakdown.

---

## Phase 2.5: Remove Server and Protocol Code ✅

**Status**: COMPLETE (2025-11-13) - Removed ~1,483 lines of server/protocol code. Jansson retained for config.c only.

---

## Phase 2.75: Pretty-Print (PP) Infrastructure ✅

**Status**: COMPLETE (2025-11-13) - Infrastructure ready (~1000 lines). REPL `/pp` command integration deferred to Phase 5.

---

## Phase 3: Scrollback Buffer Module ✅

**Status**: COMPLETE (2025-11-14) - O(1) arithmetic reflow, 726× better than 5ms target (0.003-0.009 ms). 100% coverage.

---

## Phase 4: Viewport and Scrolling Integration ✅

**Status**: COMPLETE (2025-11-14) - Automated testing done (11 tests, 100% coverage). Manual testing deferred to Phase 5.

---

## Phase 5: Cleanup and Documentation ⏳

**Goal**: Finalize REPL implementation and prepare for LLM integration.

**Status**: IN PROGRESS

**Remaining Work**:

1. **Manual Testing** ✅
   - [x] Test Phase 4 scrolling (Page Up/Down, auto-scroll on submit)
   - [x] Test `/pp` command integration (requires scrollback output)
   - [x] Test on multiple terminal emulators (xterm, gnome-terminal, alacritty, kitty)

2. **Build System Cleanup**
   - [ ] Update distro packaging files (Debian, Fedora, Arch)
   - [ ] Run `make distro-check` to verify builds

3. **Documentation**
   - [ ] Update `docs/architecture.md` (final dependencies list)
   - [ ] Update `docs/repl/README.md` (final status)
   - [ ] Archive plan.md and tasks.md (move to docs/repl/)

4. **Quality Gates**
   - [ ] Final run: `make check && make lint && make coverage && make check-dynamic`

---

## Module Dependency Graph

**Current State (Phase 2)**:
```
client.c (32 lines, main only)
  └─ repl.{c,h}
       ├─ terminal.{c,h}
       ├─ render.{c,h}         [Replaces old render.c - Phase 1]
       ├─ input buffer.{c,h}              [Split into 3 files]
       │    ├─ input_buffer_cursor.{c,h} [Internal cursor management]
       │    ├─ input_buffer_multiline.c  [Multi-line operations]
       │    └─ byte_array.{c,h}       [Dynamic byte buffer]
       └─ input.{c,h}
```

**After Phase 2.75 (PP Functionality)**:
```
client.c (32 lines, main only)
  └─ repl.{c,h}
       ├─ terminal.{c,h}
       ├─ render.{c,h}         [Single framebuffer writes]
       ├─ format.{c,h}                 [Phase 2.75 - printf-style buffer formatting]
       │    └─ byte_array.{c,h}       [Reused for text accumulation]
       ├─ input buffer.{c,h}              [With ik_pp_input_buffer() - Phase 2.75]
       │    ├─ input_buffer_cursor.{c,h} [Internal cursor management]
       │    ├─ input_buffer_multiline.c  [Multi-line operations]
       │    └─ byte_array.{c,h}       [Dynamic byte buffer]
       └─ input.{c,h}
```

**Final State (After Phase 4)**:
```
client.c (32 lines, main only)
  └─ repl.{c,h}
       ├─ terminal.{c,h}
       ├─ render.{c,h}         [Single framebuffer writes]
       ├─ format.{c,h}                 [Phase 2.75 - printf-style buffer formatting]
       │    └─ byte_array.{c,h}       [Reused for text accumulation]
       ├─ scrollback.{c,h}             [Phase 3 - hot/cold data separation]
       ├─ input buffer.{c,h}              [With layout caching - Phase 3]
       │    ├─ input_buffer_cursor.{c,h} [Internal cursor management]
       │    ├─ input_buffer_multiline.c  [Multi-line operations]
       │    └─ byte_array.{c,h}       [Dynamic byte buffer]
       └─ input.{c,h}                  [With Page Up/Down - Phase 4]
```

**External Dependencies**:
- talloc, jansson, uuid, libb64, pthread, libutf8proc

---

## Performance Targets

### Scrollback Reflow (Terminal Resize)
- **Target**: 1000 lines < 5ms
- **Achieved**: 0.003-0.009 ms (726× better than target)
- **Operation**: O(n) where n = number of lines, but each line is O(1) arithmetic

### Frame Rendering
- **Write operations**: 1 (entire frame in single write)
- **Bytes processed**: Only visible content (~1,920 chars for 24×80 terminal)
- **Syscall reduction**: 52× fewer syscalls, 26× fewer bytes than cell-by-cell updates

### Memory Overhead
- **Per scrollback line**: 32 bytes metadata + text content
- **1000 lines**: ~32 KB metadata
- **Cache locality**: Hot data (layouts) separated from cold data (text)

---

## Development Notes

- **TDD**: Red/Green/Verify cycle for all new code
- **Quality**: 100% test coverage before each phase completion
- **Manual Testing**: Use client.c demos to verify each phase before moving forward
- **Zero Technical Debt**: Fix issues immediately as discovered
- **Incremental**: Each phase builds on the previous, with full verification
