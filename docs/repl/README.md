# REPL Terminal Interface

## Overview

A terminal-based REPL with split-buffer interface: immutable scrollback history above, editable multi-line input buffer below. Direct ANSI rendering eliminates terminal emulation overhead—raw terminal bytes flow through an input parser that produces semantic actions (insert, delete, move cursor), input buffer transforms UTF-8 text with grapheme-aware cursor positioning, and render writes a single framebuffer per frame using ANSI escape sequences (CSI cursor positioning, alternate screen buffer). Scrollback uses pre-computed display widths for O(1) arithmetic reflow on terminal resize (726× faster than 5ms target). Event loop follows a strict cycle: read input → parse to action → update input buffer → render frame. Architecture prioritizes testability (100% coverage via TDD), performance (cached layouts, minimal allocations), and simplicity (single-threaded, direct ANSI escape sequences, talloc memory hierarchy).

## Table of Contents

### Overview (this document)
- [Architecture Overview](#architecture-overview)
- [Implementation Summary](#implementation-summary)
- [Key Features](#key-features)

### Component Details (separate files)
- [Phase 0: Foundation](repl-phase-0.md) - Error handling + generic array utility
- [Phase 1: Direct Rendering](repl-phase-1.md) - Direct terminal rendering (UTF-8 aware cursor positioning)
- [Phase 2: REPL Event Loop](repl-phase-2.md) - Complete interactive REPL with input buffer
- [Phase 3: Scrollback Buffer](repl-phase-3.md) - Scrollback storage with layout caching
- [Phase 4: Viewport and Scrolling](repl-phase-4.md) - Scrollback integration with viewport scrolling
- [Phase 5: Cleanup](repl-phase-5.md) - Final polish and documentation
- [Phase 6: Terminal Enhancements](repl-phase-6.md) - Future enhancements (bracketed paste + SGR colors)

---

## Architecture Overview

The REPL was built incrementally in phases, each building on the previous:

**Phase 0 (Foundation)**: Error handling cleanup and generic `ik_array_t` utility
**Phase 1 (Direct Rendering)**: Direct terminal rendering with UTF-8 aware cursor positioning
**Phase 2 (REPL Event Loop)**: Complete REPL event loop with full interactivity
**Phase 2.5 (Code Cleanup)**: Removed server and protocol code (cleanup 2025-11-13)
**Phase 2.75 (Pretty-Print)**: Pretty-print infrastructure (format module 2025-11-13)
**Phase 3 (Scrollback Buffer)**: Scrollback buffer module with O(1) arithmetic reflow (2025-11-14)
**Phase 4 (Viewport and Scrolling)**: Viewport and scrolling integration (2025-11-14)
**Phase 5 (Cleanup)**: Final polish and documentation (2025-11-14)
**Phase 6 (Future)**: Optional terminal enhancements (bracketed paste + colors)

All phases followed strict TDD (Test-Driven Development) with 100% coverage requirement.

## Implementation Summary

**Phase 0 - Foundation**:
- Error handling cleanup following 3 modes of operation philosophy
- Generic `ik_array_t` utility with typed wrappers (`ik_byte_array_t`, `ik_line_array_t`)

**Phase 1 - Direct Rendering**:
- Direct ANSI terminal rendering with UTF-8 aware cursor calculation
- Single framebuffer write to terminal (52× syscall reduction, 26× bytes reduction)

**Phase 2 - Complete REPL Event Loop** (2025-11-11):
- Full interactive REPL with input buffer
- Event loop, action processing, frame rendering
- Multi-line input, cursor movement, text editing
- Readline shortcuts (Ctrl+A/E/K/U/W)
- Column preservation in vertical navigation
- 100% test coverage (1315 lines, 105 functions, 525 branches)

**Phase 2.5 - Code Cleanup**:
- Removed server binary and protocol module (~1,483 lines)
- Simplified architecture for REPL focus

**Phase 2.75 - Pretty-Print Infrastructure** (2025-11-13):
- Format buffer module with 100% test coverage
- Generic PP helpers for structure inspection
- `/pp` command infrastructure

**Phase 3 - Scrollback Buffer Module** (2025-11-14):
- Scrollback storage with pre-computed display_width
- Layout caching for O(1) reflow on resize (0.003-0.009 ms for 1000 lines, 726× better than 5ms target)
- Input buffer layout caching
- 100% test coverage (1,569 lines, 126 functions, 554 branches)

**Phase 4 - Viewport and Scrolling** (2025-11-14):
- Scrollback integration with REPL
- Viewport calculation and scrolling commands
- Page Up/Down support with auto-scroll on submit
- 100% test coverage (1,648 lines, 128 functions, 510 branches)

**Phase 5 - Cleanup** (2025-11-14):
- Manual testing (scrolling, `/pp` command, multiple terminals)
- Quality gates verification (check, lint, coverage, check-dynamic)
- Documentation updates

**Phase 6 - Future Enhancements**:
- Bracketed paste mode for safe multi-line pasting (not implemented)
- SGR color sequences for visual distinction (not implemented)

---

## Key Features

**Terminal Interface:**
- Split-buffer design: scrollback history above, editable input below
- Direct ANSI rendering without terminal emulation library
- UTF-8 aware with proper grapheme cluster handling
- Single framebuffer write per render cycle

**Input Editing:**
- Multi-line text editing with newlines
- Cursor navigation (arrows, Ctrl+A/E)
- Text manipulation (backspace, delete, Ctrl+K/U/W)
- Column preservation in vertical navigation
- Automatic text wrapping at terminal boundaries

**Scrollback:**
- Unlimited history storage
- Pre-computed display widths for fast reflow
- O(1) arithmetic reflow on terminal resize
- Page Up/Down scrolling through history
- Auto-scroll to bottom on new content

**Performance:**
- O(1) reflow: 0.003-0.009 ms for 1000 lines (726× better than target)
- Single syscall per frame render
- Hot/cold data separation for cache locality
- Lazy layout recalculation

**Quality:**
- 100% test coverage (TDD from start)
- Zero memory leaks (talloc hierarchy)
- Clean terminal restoration on exit
- All sanitizers passing (ASan, UBSan, TSan)
