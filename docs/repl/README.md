# REPL Terminal Interface

## Overview

Build a minimal REPL chatbot with a split-buffer terminal interface that will eventually talk to OpenAI. This serves as a **manual testing harness** for developing AI model modules.

## Table of Contents

### Overview (this document)
- [Progressive Development](#progressive-development)
- [Notes](#notes)

### Phase Details (separate files)
- [Phase 0: Foundation](repl-phase-0.md) ✅ - Clean up error handling + build generic array utility
- [Phase 1: Simple Dynamic Zone](repl-phase-1.md) 🔄 - Basic terminal, UTF-8, cursor handling
- [Phase 2: Add Scrollback and Scrolling](repl-phase-2.md) - Complete terminal UI with continuous buffer model
- [Phase 3: OpenAI Integration](repl-phase-3.md) - Make it a real chatbot
- [Testing Strategy](repl-testing.md) - TDD approach and manual test plan

---

## Progressive Development

This work is split into incremental phases, each building on the previous.

**Current Roadmap:**
- **Phase 0**: Clean up existing error handling + build generic `ik_array_t` utility
- **Phase 1**: Simple dynamic zone only (validate terminal, UTF-8, cursor handling)
- **Phase 2**: Add scrollback buffer and scrolling (complete terminal UI)
- **Phase 3**: OpenAI integration (make it a real chatbot)

**Current focus**: Phase 1, Task 6 (REPL event loop - Steps 4-8 remaining)

Each phase follows strict TDD (Test-Driven Development) with 100% coverage requirement.

**Note**: This roadmap will be re-evaluated after completing each phase. We'll adjust future phases based on what we learn during implementation.

### Quick Phase Summary

**Phase 0** - Foundation work before any REPL code:
- ✅ Task 1: Error handling cleanup
- ✅ Task 2: Generic array utility with typed wrappers

**Phase 1** - Simple dynamic zone (IN PROGRESS):
- ✅ Terminal setup (raw mode, alternate screen)
- ✅ Single editable zone (no scrollback yet)
- ✅ UTF-8/grapheme handling
- ✅ Cursor tracking and movement
- ✅ Text editing (insert, backspace, delete)
- ✅ Basic vterm rendering
- 🔄 REPL event loop (Steps 4-8 remaining)

**Phase 2** - Add scrollback:
- Scrollback buffer
- Separator line
- Continuous buffer model
- Viewport scrolling (mouse wheel, Page Up/Down)
- Snap-back behavior
- Line submission to history

**Phase 3** - OpenAI integration:
- API client library
- Streaming responses
- Status indicators
- Error handling

---

## Notes

- **No server connection** - this is a standalone client
- **No persistence** - conversation lost on exit
- **Minimal UI** - just text, no colors/formatting (can add later)
- Purpose is rapid iteration on AI module patterns with manual testing
