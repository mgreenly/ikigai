# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is a Linux-focused, multi-model coding agent with RAG-accessible permanent memory, written in C.

A desktop terminal application that combines:
- Direct LLM API integration (OpenAI, Anthropic, Google, X.AI)
- Local tool execution (file operations, shell commands, code analysis)
- PostgreSQL-backed conversation history with full-text search
- Terminal UI with direct rendering and scrollback

**Current status**: v0.1.0 - Production-ready REPL terminal foundation complete. Ready for LLM integration.

## v1.0 Vision

**Desktop terminal client** with:
- **Terminal UI**: Direct terminal rendering with scrollback, multi-line input, and clean UX
- **LLM Integration**: Streaming responses from multiple providers (OpenAI, Anthropic, Google, X.AI)
- **Local Tool Execution**: File operations, shell commands, code analysis (full trust model)
- **Database Integration**: PostgreSQL for persistent conversation history and RAG memory
- **RAG-based Memory**: Conversation search and context retrieval across all past interactions

## Quick Start

- **Source code**: `src/`
- **Build target**: `bin/ikigai`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/performance/`
- **Build**: `make` (debug), `make release`, `make check` (run tests)
- **Standard**: C17 with K&R style (120-char width)

See [build-system.md](build-system.md) for comprehensive build documentation.

## Documentation

### Architecture & Design
- **[architecture.md](architecture.md)** - System architecture, dependencies, and design principles
- **[decisions/](decisions/)** - Architecture Decision Records (ADRs) for key design choices
- **[repl/](repl/)** - REPL terminal interface documentation (v0.1.0 - complete)

### Development Standards
- **[naming.md](naming.md)** - Naming conventions and approved abbreviations
- **[memory.md](memory.md)** - Memory management with talloc, ownership rules, and patterns
- **[error_handling.md](error_handling.md)** - Error handling philosophy and decision framework
- **[error_patterns.md](error_patterns.md)** - Error handling patterns and best practices
- **[error_testing.md](error_testing.md)** - Error handling testing strategy
- **[memory+error_handling.md](memory+error_handling.md)** - Integration of memory and error handling
- **[generic-types.md](generic-types.md)** - Generic base implementation with type-safe wrappers

### Build & Testing
- **[build-system.md](build-system.md)** - Build system, quality gates, testing infrastructure, multi-distro support
- **[lcov_exclusion_strategy.md](lcov_exclusion_strategy.md)** - Coverage exclusion guidelines

### Internal Analysis
- **[memory_usage_analysis.md](memory_usage_analysis.md)** - Memory usage analysis and optimization notes

## Roadmap to v1.0

### ✅ Completed: REPL Terminal Foundation (v0.1.0)

**Released**: 2025-11-16

Production-ready terminal interface with:
- Direct terminal rendering (single write per frame)
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with readline-style shortcuts
- Scrollback buffer with O(1) arithmetic reflow
- Viewport scrolling (Page Up/Down, auto-scroll)
- 100% test coverage (1,807 lines, 131 functions, 600 branches)

See [repl/README.md](repl/README.md) for detailed documentation.

### Next: LLM Integration

**Status**: Next phase

**Objective**: Stream LLM responses directly to the terminal

**Tasks**:
- OpenAI API client with libcurl streaming
- Display AI responses in scrollback as chunks arrive
- Basic conversation flow (user input → API call → streamed response)
- Status indicators (loading spinner, error handling)

**Dependencies**: yyjson (complete), libcurl (to be added)

### Future: Database Integration (PostgreSQL)

**Objective**: Persistent conversation history with RAG memory

**Tasks**:
- PostgreSQL schema for conversations and messages
- Save/load conversation history
- Full-text search across past conversations
- RAG memory access patterns

### Future: Multi-LLM Provider Support

**Objective**: Support multiple LLM providers with unified interface

**Tasks**:
- Abstract provider interface
- OpenAI, Anthropic, Google, X.AI implementations
- Provider switching via config or runtime command
- Unified conversation format

### Future: Local Tool Execution

**Objective**: Enable file operations, shell commands, and code analysis

**Tasks**:
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Code analysis (tree-sitter integration)
- Results flow back to conversation

### Future: Enhanced Terminal UI

**Objective**: Polish the user experience

**Tasks**:
- Syntax highlighting in code blocks (tree-sitter)
- External editor integration ($EDITOR)
- Command history and session management
- Rich formatting and themes

---

**Note**: This documentation is maintained for AI agents and developers. Keep it concise, accurate, and current.
