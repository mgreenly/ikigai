# REPL Terminal Interface

## Overview

Build a minimal REPL chatbot with a split-buffer terminal interface that will eventually talk to OpenAI. This serves as a **manual testing harness** for developing AI model modules.

## Table of Contents

### Overview (this document)
- [Progressive Development](#progressive-development)
- [Notes](#notes)

### Phase Details (separate files)
- [Phase 0: Foundation](repl-phase-0.md) ✅ - Clean up error handling + build generic array utility
- [Phase 1: Direct Rendering](repl-phase-1.md) ✅ - Direct terminal rendering (UTF-8 aware cursor positioning)
- [Phase 2: REPL Event Loop](repl-phase-2.md) ⏳ - Complete interactive REPL with workspace
- [Phase 3: Scrollback Buffer](repl-phase-3.md) - Add scrollback storage with layout caching
- Phase 4: Viewport and Scrolling - Integrate scrollback with REPL, add scrolling
- Phase 5: Cleanup - Final polish and documentation
- [Testing Strategy](repl-testing.md) - TDD approach and manual test plan

---

## Progressive Development

This work is split into incremental phases, each building on the previous.

**Current Roadmap:**
- **Phase 0** ✅: Clean up existing error handling + build generic `ik_array_t` utility
- **Phase 1** ✅: Direct terminal rendering with UTF-8 aware cursor positioning
- **Phase 2** ⏳: Complete REPL event loop with full interactivity
- **Phase 3**: Add scrollback buffer module with layout caching
- **Phase 4**: Integrate viewport and scrolling
- **Phase 5**: Final polish and documentation

**Current focus**: Phase 2 - multi-line editing and readline shortcuts

Each phase follows strict TDD (Test-Driven Development) with 100% coverage requirement.

### Quick Phase Summary

**Phase 0** - Foundation work ✅ COMPLETE:
- ✅ Task 1: Error handling cleanup
- ✅ Task 2: Generic array utility with typed wrappers

**Phase 1** - Direct Rendering (workspace only) ✅ COMPLETE:
- ✅ Removed old vterm-based render module
- ✅ Implemented `render_direct` with UTF-8 aware cursor calculation
- ✅ Single framebuffer write to terminal
- ✅ Manual verification completed

**Phase 2** - Complete REPL Event Loop:
- Full interactive REPL with workspace (no scrollback)
- Event loop, action processing, frame rendering
- Multi-line input, cursor movement, text editing
- Main entry point in main.c

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

---

## Notes

- **No server connection** - this is a standalone client
- **No persistence** - conversation lost on exit
- **Minimal UI** - just text, no colors/formatting (can add later)
- Purpose is rapid iteration on AI module patterns with manual testing
