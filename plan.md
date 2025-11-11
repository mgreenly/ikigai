# REPL Implementation Plan

## Overview

This plan transitions from the vterm-based rendering approach to direct terminal rendering as outlined in `docs/eliminate_vterm.md`. We'll build incrementally with manual verification at each phase.

**Current Status**: Phase 2 in progress - render_direct complete, workspace editing complete, event loop next.

**Strategy**: Replace vterm immediately (no parallel track), verify with client.c demos at each phase.

**Goal**: Complete REPL with direct rendering, scrollback buffer, and viewport scrolling.

---

## Phase 0: Foundation ✅

All foundation modules built: terminal, input, workspace, cursor. REPL init/cleanup implemented.

## Phase 1: Direct Rendering ✅

Replaced vterm with `render_direct` module for direct terminal rendering. UTF-8 aware cursor positioning, single framebuffer writes, 100% test coverage. Old vterm-based render module deleted.

---

## Phase 2: Complete Basic REPL Event Loop ⏳

**Goal**: Full interactive REPL with just workspace (no scrollback).

**Completed**:
- ✅ `ik_repl_render_frame()` - Render current workspace state
- ✅ `ik_repl_process_action()` - Process all input actions (char, newline, backspace, delete, arrows, readline shortcuts)
- ✅ Workspace multi-line cursor navigation (cursor_up/down)
- ✅ Readline-style editing: Ctrl+A (line start), Ctrl+E (line end), Ctrl+K (kill to end), Ctrl+U (kill line), Ctrl+W (delete word backward)
- ✅ Cursor module refactored (workspace-internal, void+assertions, LCOV -10)
- Coverage: 1257 lines, 103 functions, 467 branches, 162 LCOV exclusions

**Remaining**:
- [ ] `ik_repl_run()` - Main event loop (read → parse → process → render)
- [ ] Add `ik_read_wrapper()` to wrapper.h for testable event loop
- [ ] Simplify client.c to pure coordination (remove static helpers, add LCOV_EXCL markers)
- [ ] Manual testing checklist (UTF-8, wrapping, multi-line, readline shortcuts, Ctrl+C exit)

See [phase-2-tasks.md](phase-2-tasks.md) for detailed breakdown.

---

## Phase 3: Scrollback Buffer Module

**Goal**: Add scrollback buffer storage with layout caching for historical output.

**Key Features**:
- New `scrollback` module with separated hot/cold data for cache locality
- Pre-computed `display_width` (scan UTF-8 once on line creation)
- O(1) arithmetic reflow on terminal resize (`physical_lines = display_width / terminal_width`)
- Workspace module extended with layout caching
- Performance target: 1000 lines < 5ms reflow time

See [docs/eliminate_vterm.md](docs/eliminate_vterm.md) lines 105-413 for detailed design.

---

## Phase 4: Viewport and Scrolling Integration

**Goal**: Integrate scrollback with REPL, add viewport calculation and scrolling commands.

**Key Features**:
- REPL gains `scrollback` and `scroll_offset` members
- `ik_repl_submit_line()` - Move workspace content to scrollback history
- `ik_repl_scroll()` - Adjust viewport position
- `ik_render_direct_frame()` - Render scrollback + separator + workspace in single write (only visible lines)
- Add Page Up/Down input actions for scrollback navigation
- Viewport window calculation: total_physical_lines = scrollback + 1 (separator) + workspace
- Performance: only copy visible text (not entire scrollback)

See [docs/eliminate_vterm.md](docs/eliminate_vterm.md) lines 418-527 for rendering algorithm.

---

## Phase 5: Cleanup and Documentation

**Goal**: Remove vterm dependency from build system, update all documentation, finalize implementation.

**Tasks**:
- Remove `-lvterm` from Makefile `CLIENT_LIBS`
- Update all distro packaging (Debian, Fedora, Arch) to remove libvterm-dev dependency
- Update docs: architecture.md, repl-phase-1.md, eliminate_vterm.md, README.md
- Final quality gates: `make check && make lint && make coverage && make check-dynamic && make distro-check`
- Final manual testing on multiple terminal emulators (xterm, gnome-terminal, alacritty, kitty)

---

## Module Dependency Graph (Final State)

```
main.c
  └─ repl.{c,h}
       ├─ terminal.{c,h}
       ├─ render_direct.{c,h}    [NEW - replaces render.c]
       ├─ scrollback.{c,h}        [NEW]
       ├─ workspace.{c,h}         [MODIFIED - add layout caching]
       │    ├─ byte_array.{c,h}
       │    └─ cursor.{c,h}
       └─ input.{c,h}             [MODIFIED - add Page Up/Down]
```

**Dependencies**:
- **Removed**: libvterm
- **Unchanged**: talloc, jansson, uuid, libb64, pthread, libutf8proc

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
