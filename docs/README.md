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
- **[repl/](repl/)** - REPL terminal interface documentation (Phase 2 in progress)
- **[architecture.md](architecture.md)** - System architecture and design decisions
- **[decisions/](decisions/)** - Architecture decision records (ADRs) for key design choices
- **[naming.md](naming.md)** - Naming conventions and approved abbreviations
- **[memory.md](memory.md)** - Memory management with talloc, ownership rules, patterns
- **[error_handling.md](error_handling.md)** - Error handling with talloc-integrated Result types
- **[generic-types.md](generic-types.md)** - Generic base implementation with type-safe wrapper pattern
- **[build-system.md](build-system.md)** - Build system with quality gates, testing infrastructure, and multi-distro support
- **[security-analysis.md](security-analysis.md)** - Security analysis of input parsing, UTF-8 handling, and terminal control
- **[vulnerabilities.md](vulnerabilities.md)** - CVE vulnerability tracking for dependencies

### Historical Reference
- **[archive/](archive/)** - Earlier design docs (server architecture, vterm elimination design)

### Future Work
- **[roadmap.md](roadmap.md)** - Post-v1.0 exploration areas

## Development Roadmap

### Current: REPL Terminal Foundation ⏳
**Phase 2 in progress** - See [repl/](repl/) for details.

Building a robust terminal interface with direct rendering:
- ✅ Phase 0: Foundation (error handling, memory management, generic arrays)
- ✅ Phase 1: Direct rendering (vterm eliminated, UTF-8 aware cursor positioning)
- ⏳ Phase 2: Complete REPL event loop (multi-line editing, readline shortcuts)
- 📋 Phase 3: Scrollback buffer with layout caching
- 📋 Phase 4: Viewport and scrolling integration
- 📋 Phase 5: Cleanup and documentation

**Current capabilities:**
- Direct terminal rendering without external terminal emulator
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with cursor navigation
- Text wrapping and clean terminal restoration

### Next: LLM Integration
- OpenAI API client with streaming
- Display AI responses in scrollback
- Basic conversation flow
- Response streaming to terminal

### Future: Core v1.0 Features

**Database Integration (PostgreSQL)**
- Persistent conversation history
- Message storage and retrieval
- Full-text search capabilities
- RAG memory access

**Multi-LLM Provider Support**
- Abstract provider interface
- OpenAI integration (streaming)
- Anthropic integration
- Google integration
- Unified conversation format

**Local Tool Execution**
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Code analysis tools
- Full trust model (user's machine)

**Enhanced Terminal UI**
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
