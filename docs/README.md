# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is a Linux focused, multi-model, coding agent with RAG accessible permanent memory, written in "C".

It's in an early stage of development.

## v1.0 Vision

**Desktop/terminal client** with:
- **Terminal UI**: Direct terminal rendering with scrollback, dynamic input, and clean UX
- **Database integration**: PostgreSQL for persistent conversation history and RAG memory
- **Multi-LLM support**: Anthropic, OpenAI, Google, X.AI
- **Local tool execution**: File operations, shell commands, code analysis (full trust model)
- **RAG-based memory**: Conversation search and context retrieval

## Codebase Structure

- **Source**: `src/` contains the source code
- **Build target**: `bin/ikigai`
- **Build system**: Make with comprehensive warnings, sanitizers, Valgrind, and coverage support - see [build-system.md](build-system.md)
- **Testing**: [Check framework](https://github.com/libcheck/check) with unit and integration tests; OOM injection via test seams, exists in `tests/` folder.
- **Standard**: C17 with K&R style (120-char width)

## Documentation Files

### Core Documentation
- **[repl/](repl/)** - REPL terminal interface documentation (Complete - ready for LLM integration)
- **[architecture.md](architecture.md)** - System architecture and design decisions
- **[decisions/](decisions/)** - Architecture decision records (ADRs) for key design choices
- **[naming.md](naming.md)** - Naming conventions and approved abbreviations
- **[memory.md](memory.md)** - Memory management with talloc, ownership rules, patterns
- **[error_handling.md](error_handling.md)** - Error handling philosophy: Result types, assertions, FATAL(), and decision framework
- **[error_patterns.md](error_patterns.md)** - Error handling patterns, best practices, and usage examples
- **[error_testing.md](error_testing.md)** - Error handling testing strategy and coverage requirements
- **[generic-types.md](generic-types.md)** - Generic base implementation with type-safe wrapper pattern
- **[build-system.md](build-system.md)** - Build system with quality gates, testing infrastructure, and multi-distro support
- **[security-analysis.md](security-analysis.md)** - Security analysis of input parsing, UTF-8 handling, and terminal control
- **[vulnerabilities.md](vulnerabilities.md)** - CVE vulnerability tracking for dependencies


### Future Work
- **[roadmap.md](roadmap.md)** - Post-v1.0 exploration areas

## Development Roadmap

### Completed: REPL Terminal Foundation ✅
**All phases complete (2025-11-15)** - See [repl/](repl/) for detailed documentation.

Built a production-ready terminal interface with direct rendering:
- ✅ Phase 0: Foundation (error handling, memory management, generic arrays)
- ✅ Phase 1: Direct rendering (UTF-8 aware cursor positioning)
- ✅ Phase 2: Complete REPL event loop (multi-line editing, readline shortcuts)
- ✅ Phase 2.5: Remove server/protocol code (cleanup complete)
- ✅ Phase 2.75: Pretty-print infrastructure (format module, pp_helpers, pp functions)
- ✅ Phase 3: Scrollback buffer with layout caching (O(1) reflow, 726× faster than target)
- ✅ Phase 4: Viewport and scrolling integration (100% test coverage)
- ✅ Phase 5: Manual testing, documentation, and final cleanup

**Delivered capabilities:**
- Direct terminal rendering (single write per frame, 52× syscall reduction)
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with cursor navigation
- Readline-style shortcuts (Ctrl+A/E/K/U/W)
- Text wrapping and clean terminal restoration
- Full REPL event loop with comprehensive test coverage
- Scrollback buffer with O(1) arithmetic reflow (0.003-0.009 ms for 1000 lines)
- Viewport scrolling (Page Up/Down, auto-scroll on submit)
- Pretty-print infrastructure for debugging
- 100% test coverage (1,807 lines, 131 functions, 600 branches)

### Next: Core v1.0 Features

The following features will be implemented **in the order listed**:

**1. LLM Integration**

*JSON Library Migration (yyjson)*
- Migrate from jansson to yyjson for better talloc integration
- Custom allocator support eliminates reference counting complexity
- 3× faster parsing for streaming LLM responses
- See [jansson_to_yyjson_proposal.md](jansson_to_yyjson_proposal.md) for details

*OpenAI API Integration*
- OpenAI API client with streaming
- Display AI responses in scrollback
- Basic conversation flow
- Response streaming to terminal

**2. Database Integration (PostgreSQL)**
- Persistent conversation history
- Message storage and retrieval
- Full-text search capabilities
- RAG memory access

**3. Multi-LLM Provider Support**
- Abstract provider interface
- OpenAI integration (streaming)
- Anthropic integration
- Google integration
- Unified conversation format

**4. Local Tool Execution**
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Code analysis tools
- Full trust model (user's machine)

**5. Enhanced Terminal UI**
- Syntax highlighting in code blocks (tree-sitter based)
- External editor integration ($EDITOR)
- Command history and session management
- Rich formatting

## Development Philosophy

**Build incrementally**. Each phase establishes solid foundations before adding features. Focus on getting core patterns right (memory management, error handling, terminal UI) early.

**Test-driven development**. 100% test coverage requirement with comprehensive unit tests, OOM injection testing, and multiple quality gates.

**Keep it focused**. v1.0 is a desktop client for a single user. Local execution, full trust model, direct LLM API integration.

---

**Note**: Documentation maintained for agent and developer understanding. Keep concise and current.
