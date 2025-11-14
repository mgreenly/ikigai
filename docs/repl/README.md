# REPL Terminal Interface

## Overview

A terminal-based REPL with split-buffer interface: immutable scrollback history above, editable multi-line workspace below. Direct ANSI rendering eliminates terminal emulation overhead—raw terminal bytes flow through an input parser that produces semantic actions (insert, delete, move cursor), workspace transforms UTF-8 text with grapheme-aware cursor positioning, and render writes a single framebuffer per frame using ANSI escape sequences (CSI cursor positioning, alternate screen buffer). Scrollback uses pre-computed display widths for O(1) arithmetic reflow on terminal resize (1000× faster than re-measuring). Event loop follows a strict cycle: read input → parse to action → update workspace → render frame. Architecture prioritizes testability (100% coverage via TDD), performance (cached layouts, minimal allocations), and simplicity (single-threaded, no VTerm library, talloc memory hierarchy).

## Table of Contents

### Overview (this document)
- [Progressive Development](#progressive-development)
- [Notes](#notes)

### Phase Details (separate files)
- [Phase 0: Foundation](repl-phase-0.md) ✅ - Clean up error handling + build generic array utility
- [Phase 1: Direct Rendering](repl-phase-1.md) ✅ - Direct terminal rendering (UTF-8 aware cursor positioning)
- [Phase 2: REPL Event Loop](repl-phase-2.md) ✅ - Complete interactive REPL with workspace
- [Phase 3: Scrollback Buffer](repl-phase-3.md) - Add scrollback storage with layout caching
- [Phase 4: Viewport and Scrolling](repl-phase-4.md) - Integrate scrollback with REPL, add scrolling
- [Phase 5: Cleanup](repl-phase-5.md) - Final polish and documentation
- [Phase 6: Terminal Enhancements](repl-phase-6.md) - Bracketed paste mode + SGR colors
- [Testing Strategy](repl-testing.md) - TDD approach and manual test plan

---

## Progressive Development

This work is split into incremental phases, each building on the previous.

**Current Roadmap:**
- **Phase 0** ✅: Clean up existing error handling + build generic `ik_array_t` utility
- **Phase 1** ✅: Direct terminal rendering with UTF-8 aware cursor positioning
- **Phase 2** ✅: Complete REPL event loop with full interactivity
- **Phase 2.5** ✅: Remove server and protocol code (cleanup complete 2025-11-13)
- **Phase 2.75** ⚙️: Pretty-print infrastructure (format module complete, REPL integration deferred)
- **Phase 3**: Add scrollback buffer module with layout caching + `/pp` command integration
- **Phase 4**: Integrate viewport and scrolling
- **Phase 5**: Final polish and documentation
- **Phase 6**: Terminal enhancements (bracketed paste + colors)

**Current focus**: Phase 3 - scrollback buffer module (includes deferred `/pp` REPL integration)

Each phase follows strict TDD (Test-Driven Development) with 100% coverage requirement.

### Quick Phase Summary

**Phase 0** - Foundation work ✅ COMPLETE:
- ✅ Task 1: Error handling cleanup
- ✅ Task 2: Generic array utility with typed wrappers

**Phase 1** - Direct Rendering (workspace only) ✅ COMPLETE:
- ✅ Removed old vterm-based render module
- ✅ Implemented `render` with UTF-8 aware cursor calculation
- ✅ Single framebuffer write to terminal
- ✅ Manual verification completed

**Phase 2** - Complete REPL Event Loop ✅ COMPLETE (2025-11-11):
- ✅ Full interactive REPL with workspace (no scrollback)
- ✅ Event loop, action processing, frame rendering
- ✅ Multi-line input, cursor movement, text editing
- ✅ Readline shortcuts (Ctrl+A/E/K/U/W)
- ✅ Column preservation in vertical navigation
- ✅ Manual testing (32 tests), bug fixes (3 critical/medium/low)
- ✅ Code review (Grade: A-, production-ready)
- ✅ 100% test coverage (1315 lines, 105 functions, 525 branches)

**Phase 2.5** - Remove Server and Protocol Code ✅ COMPLETE:
- ✅ Remove server binary (`src/server.c`)
- ✅ Remove protocol module (`src/protocol.{c,h}`)
- ✅ Remove protocol tests (~773+ lines total)
- ✅ Keep jansson in config.c temporarily
- ✅ Update/archive documentation
- ✅ Clean architecture for Phase 3+

**Phase 2.75** - Pretty-Print Infrastructure ⚙️ PARTIAL:
- ✅ Format buffer module with 100% test coverage
- ✅ Generic PP helpers for reusable formatting (`pp_helpers.c`)
- ✅ `ik_pp_workspace()` and `ik_pp_cursor()` for recursive structure inspection
- ⏸️ `/pp` command in REPL - **DEFERRED to Phase 3**
  - Current stdout implementation violates design principle
  - All output must go: screenbuffer → blit to alternate buffer (never stdout/stderr)
  - Requires scrollback buffer for proper visible output

**Phase 3** - Scrollback Buffer Module:
- Scrollback storage with pre-computed display_width
- Layout caching for O(1) reflow on resize
- Workspace layout caching
- Performance: 1000× faster resize via arithmetic reflow

**Phase 4** - Viewport and Scrolling:
- Integrate scrollback with REPL
- Viewport calculation and scrolling commands
- Page Up/Down support
- Complete terminal UI

**Phase 5** - Cleanup:
- Final polish and code review
- Update all documentation
- Verification across all distros

**Phase 6** - Terminal Enhancements:
- Bracketed paste mode for safe multi-line pasting
- SGR color sequences for visual distinction
- Optional, improves UX without changing core architecture

---

## Notes

- **No persistence** - conversation lost on exit
- **Minimal UI** - Phases 0-5 focus on core functionality with plain text
- **Phase 6 enhancements** - Bracketed paste and colors added after core is stable
- Purpose is rapid iteration on AI module patterns with manual testing
