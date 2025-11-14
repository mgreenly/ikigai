# REPL Implementation Plan

## Overview

This plan transitions from the vterm-based rendering approach to direct terminal rendering as outlined in `docs/eliminate_vterm.md`. We'll build incrementally with manual verification at each phase.

**Current Status**: Phase 2 COMPLETE (2025-11-11) - All tasks done including manual testing, bug fixes, and code review. Phase 2.5 (server/protocol removal) inserted before Phase 3.

**Strategy**: Replace vterm immediately (no parallel track), verify with client.c demos at each phase.

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
- ✅ Workspace module (`workspace.c`) - multi-line text buffer with UTF-8 support
- ✅ REPL init/cleanup infrastructure

**Key Achievements**:
- Foundation for TDD workflow with 100% coverage requirement
- Memory management patterns established with talloc
- Generic array utility enables code reuse across modules

## Phase 1: Direct Rendering ✅

**Goal**: Replace libvterm dependency with direct ANSI terminal rendering.

**Status**: Complete

**Completed**:
- ✅ New `render` module with UTF-8 aware cursor positioning
- ✅ Single framebuffer write per frame (no VTerm intermediate buffer)
- ✅ ANSI escape sequences for cursor movement and screen control
- ✅ Support for multi-byte UTF-8 characters (emoji, CJK, combining chars)
- ✅ Text wrapping at terminal boundaries
- ✅ Old vterm-based `render.c`/`render.h` deleted (commit cbcc6f5)
- ✅ 100% test coverage with comprehensive UTF-8 tests
- ✅ Manual verification: text entry, wrapping, cursor movement, clean exit

**Key Achievements**:
- **52× reduction** in syscalls (single write vs. vterm's cell-by-cell updates)
- **26× reduction** in bytes processed
- Eliminated libvterm dependency from source code (build system cleanup pending Phase 5)

---

## Phase 2: Complete Basic REPL Event Loop ✅

**Goal**: Full interactive REPL with just workspace (no scrollback).

**Status**: COMPLETE (2025-11-11)

**Completed**:
- ✅ Task 1: `ik_repl_render_frame()` - Render current workspace state
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
  - Bug 6.1 CRITICAL: Empty workspace crashes (commit 9b32cff)
  - Bug 6.2 MEDIUM: Column preservation (commit 3c226d3)
  - Bug 6.3 LOW: Ctrl+W punctuation handling (commits 3c226d3, 4f38c6b)
- ✅ Code review: Security/memory/error handling analysis (Grade: A-)
  - 0 CRITICAL, 0 HIGH issues
  - 2 MEDIUM issues documented (theoretical overflows, commit 025491e)
  - Excellent security: no unsafe functions, proper UTF-8 validation
- ✅ Code refactoring:
  - Cursor module workspace-internal (void+assertions, -10 LCOV markers)
  - Workspace split: `workspace.c` + `workspace_multiline.c` (file size lint compliance)
  - Test files modularized for maintainability

**Final Metrics**:
- **Coverage**: 1315 lines, 105 functions, 525 branches (all 100%)
- **LCOV exclusions**: 162/164 (within limit)
- **Quality gates**: ✅ fmt, ✅ check, ✅ lint, ✅ coverage, ✅ check-dynamic (ASan/UBSan/TSan)

**Key Achievement**: Production-ready interactive REPL with full test coverage and security validation

See [tasks.md](tasks.md) for detailed breakdown.

---

## Phase 2.5: Remove Server and Protocol Code ✅

**Goal**: Remove all server and protocol-related code before Phase 3. This eliminates maintenance burden and jansson complexity.

**Status**: COMPLETE (2025-11-13)

**Rationale**:
v1.0 is a **desktop client** that connects directly to LLM APIs (OpenAI, Anthropic, etc.). The server/protocol code was from an earlier architecture exploration and is no longer needed.

**Scope**:
- Remove server binary target (`src/server.c` - 41 lines)
- Remove protocol module (`src/protocol.{c,h}` - 332 lines)
- Remove protocol tests (unit + integration - ~400+ lines)
- Keep jansson in config.c temporarily (revisit in Phase 3)
- Update Makefile to remove server targets
- Archive or remove server/protocol documentation
- Update architecture docs to clarify desktop-only design

**Tasks**:
1. [x] Remove protocol module (src + tests)
2. [x] Remove server binary (src + Makefile)
3. [x] Document jansson scope (config only, temporary)
4. [x] Update/archive documentation
5. [x] Verification and commit

**Benefits**:
- ~773+ lines of code removed
- Eliminates jansson reference counting complexity (except config)
- Cleaner architecture for Phase 3+
- No maintenance burden from unused code

**Estimated effort**: 1-2 hours

See [tasks.md](tasks.md) Phase 2.5 for detailed task breakdown.

---

## Phase 2.75: Pretty-Print (PP) Functionality

**Goal**: Implement debug pretty-printing for internal C data structures and JSON/YAML content.

**Status**: Infrastructure COMPLETE, REPL Integration DEFERRED to Phase 3 (2025-11-13)

**Rationale**:
Enables debugging and inspection capabilities before full scrollback implementation. PP functions can output to stdout initially, then migrate to scrollback buffer once Phase 3 is complete.

**Scope**:
- Format buffer module (`format.{c,h}`) - printf-style formatting into byte buffers
- `ik_pp_<type>()` functions for C data structures (shows pointers, sizes, internal state)
- Temporary REPL `/pp` command integration (stdout output)
- Optional: JSON pretty-print utilities (defer if not immediately needed)

**Tasks**:
1. [x] Implement format buffer module with 100% test coverage
2. [x] Implement `ik_pp_workspace()` as first example
3. [x] Add generic PP helpers (`pp_helpers.c`) for reusable formatting
4. [x] Implement recursive `ik_pp_cursor()` for proper nesting
5. [ ] Add `/pp` command to REPL - **DEFERRED to Phase 3**
   - Current stdout implementation violates design (all output must go screenbuffer → blit)
   - Requires scrollback buffer for proper visible output
6. [x] Manual verification and testing (for Tasks 1-4)

**Benefits**:
- PP infrastructure ready for debugging once scrollback exists
- Smaller, testable increments
- Informs scrollback requirements
- Generic helpers established for all future PP functions

**Actual Effort**: ~1000 lines (format module, pp_helpers, pp functions with 100% tests)

**REPL Integration Blocker**: Without scrollback buffer, cannot display PP output adhering to core principle: all visible output through screenbuffer → blit to alternate buffer (never stdout/stderr)

See [tasks.md](tasks.md) Phase 2.75 for detailed task breakdown.

---

## Phase 3: Scrollback Buffer Module

**Goal**: Add scrollback buffer storage with layout caching for historical output.

**Status**: Not started (blocked on Phase 2.75 completion)

**Planned Features**:
- New `scrollback` module with separated hot/cold data for cache locality
- Pre-computed `display_width` (scan UTF-8 once on line creation)
- O(1) arithmetic reflow on terminal resize (`physical_lines = display_width / terminal_width`)
- Workspace module extended with layout caching
- Performance target: 1000 lines < 5ms reflow time

**Implementation Tasks**:
- [ ] Design scrollback data structure (hot/cold separation)
- [ ] Implement line storage with pre-computed display widths
- [ ] Implement O(1) arithmetic reflow algorithm
- [ ] Add layout caching to workspace module
- [ ] Write comprehensive tests (TDD)
- [ ] Achieve 100% coverage
- [ ] Performance benchmarks (verify < 5ms for 1000 lines)

See [docs/eliminate_vterm.md](docs/eliminate_vterm.md) lines 105-413 for detailed design.

---

## Phase 4: Viewport and Scrolling Integration

**Goal**: Integrate scrollback with REPL, add viewport calculation and scrolling commands.

**Status**: Not started (blocked on Phase 3 completion)

**Planned Features**:
- REPL gains `scrollback` and `scroll_offset` members
- `ik_repl_submit_line()` - Move workspace content to scrollback history
- `ik_repl_scroll()` - Adjust viewport position
- `ik_render_frame()` - Render scrollback + separator + workspace in single write (only visible lines)
- Add Page Up/Down input actions for scrollback navigation
- Viewport window calculation: total_physical_lines = scrollback + 1 (separator) + workspace
- Performance: only copy visible text (not entire scrollback)

**Implementation Tasks**:
- [ ] Update REPL context with scrollback and scroll_offset
- [ ] Implement `ik_repl_submit_line()` function
- [ ] Implement `ik_repl_scroll()` function
- [ ] Update `ik_render_frame()` for viewport rendering
- [ ] Add Page Up/Down to input parser
- [ ] Integrate viewport calculation into event loop
- [ ] Write comprehensive tests (TDD)
- [ ] Achieve 100% coverage
- [ ] Manual testing of scrolling behavior

See [docs/eliminate_vterm.md](docs/eliminate_vterm.md) lines 418-527 for rendering algorithm.

---

## Phase 5: Cleanup and Documentation

**Goal**: Remove vterm dependency from build system, update all documentation, finalize implementation.

**Status**: Not started (blocked on Phase 4 completion)

**Current State**:
- ✅ Source code has NO vterm references (all removed in Phase 1)
- ✅ `-lvterm` removed from Makefile (CLIENT_LIBS and all test targets)
- ❌ Distro packaging still lists libvterm-dev dependency

**Cleanup Tasks**:
- [x] Remove `-lvterm` from Makefile `CLIENT_LIBS` and all test targets
- [x] Delete obsolete `docs/archive/` folder (vterm docs, old phase-1 server plans)
- [ ] Update distro packaging files (remove libvterm-dev):
  - [ ] Debian packaging
  - [ ] Fedora packaging
  - [ ] Arch packaging
- [ ] Run `make distro-check` to verify builds across all distros

**Documentation Tasks**:
- [ ] Update `docs/architecture.md` (remove libvterm from dependencies)
- [ ] Update `docs/README.md` (mark REPL complete through Phase 4)
- [ ] Update `docs/repl/README.md` (final status update)
- [ ] Review and finalize all phase documentation

**Final Verification**:
- [ ] Run all quality gates: `make check && make lint && make coverage && make check-dynamic`
- [ ] Run distro verification: `make distro-check`
- [ ] Manual testing on multiple terminal emulators:
  - [ ] xterm
  - [ ] gnome-terminal
  - [ ] alacritty
  - [ ] kitty
- [ ] Verify clean terminal restoration across all emulators

---

## Module Dependency Graph

**Current State (Phase 2)**:
```
client.c (32 lines, main only)
  └─ repl.{c,h}
       ├─ terminal.{c,h}
       ├─ render.{c,h}         [Replaces old render.c - Phase 1]
       ├─ workspace.{c,h}              [Split into 3 files]
       │    ├─ workspace_cursor.{c,h} [Internal cursor management]
       │    ├─ workspace_multiline.c  [Multi-line operations]
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
       ├─ workspace.{c,h}              [With ik_pp_workspace() - Phase 2.75]
       │    ├─ workspace_cursor.{c,h} [Internal cursor management]
       │    ├─ workspace_multiline.c  [Multi-line operations]
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
       ├─ workspace.{c,h}              [With layout caching - Phase 3]
       │    ├─ workspace_cursor.{c,h} [Internal cursor management]
       │    ├─ workspace_multiline.c  [Multi-line operations]
       │    └─ byte_array.{c,h}       [Dynamic byte buffer]
       └─ input.{c,h}                  [With Page Up/Down - Phase 4]
```

**External Dependencies**:
- **Removed**: libvterm (Phase 1 source, Phase 5 build system)
- **Current**: talloc, jansson, uuid, libb64, pthread, libutf8proc

---

## Performance Targets

Based on `docs/eliminate_vterm.md` analysis:

### Scrollback Reflow (Terminal Resize)
- **1000 lines**: < 5ms (target: ~2μs with pre-computed display_width)
- **Operation**: O(n) where n = number of lines, but each line is O(1) arithmetic

### Frame Rendering
- **Write operations**: 1 (entire frame in single write)
- **Bytes processed**: Only visible content (~1,920 chars for 24×80 terminal)
- **vs. vterm**: 52× reduction in syscalls, 26× reduction in bytes processed

### Memory Overhead
- **Per scrollback line**: 32 bytes metadata + text content
- **1000 lines**: ~32 KB metadata (vs ~96 KB for full VTerm grid)
- **Cache locality**: Hot data (layouts) separated from cold data (text)

---

## Development Notes

- **TDD**: Red/Green/Verify cycle for all new code
- **Quality**: 100% test coverage before each phase completion
- **Manual Testing**: Use client.c demos to verify each phase before moving forward
- **Zero Technical Debt**: Fix issues immediately as discovered
- **Incremental**: Each phase builds on the previous, with full verification
- **Strategy**: Replace immediately (Option B) - no parallel vterm/direct tracks
