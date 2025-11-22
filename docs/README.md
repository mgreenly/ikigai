# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is a Linux-focused, multi-model coding agent with permanent memory, written in C.

A desktop terminal application that combines:
- Direct LLM API integration (OpenAI, Anthropic, Google, X.AI)
- Local tool execution (file operations, shell commands, code analysis)
- PostgreSQL-backed conversation history with full-text search
- Terminal UI with direct rendering and scrollback

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
- **[return_values.md](return_values.md)** - Complete guide to function return patterns and how to use them
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
- **[considerations.md](considerations.md)** - Candidate features and changes to consider for the future
- **[../fix.md](../fix.md)** - Known issues and technical debt

## Roadmap to v1.0

**Architecture Overview**:
- **[v1-architecture.md](v1-architecture.md)** - v1.0 core architecture: scrollback as context window, three-layer model
- **[v1-implementation-roadmap.md](v1-implementation-roadmap.md)** - Phase-by-phase implementation plan with dependencies

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

### ✅ Completed: LLM Integration (v0.2.0)

**Released**: 2025-11-22

**Objective**: Stream LLM responses directly to the terminal

**Completed**:
- ✅ OpenAI API client with libcurl streaming
- ✅ Display AI responses in scrollback as chunks arrive
- ✅ Basic conversation flow (user input → API call → streamed response)
- ✅ Status indicators (loading spinner, error handling)
- ✅ Layer architecture (scrollback, spinner, separator, input)
- ✅ Slash commands (/clear, /mark, /rewind, /help, /model, /system)
- ✅ In-memory conversation state with checkpoint/rollback
- ✅ Mock verification test suite

**Configuration** (in `~/.config/ikigai/config.json`):
- `openai_api_key` - Your OpenAI API key
- `openai_model` - Model to use (default: "gpt-5-mini")
- `openai_temperature` - Temperature parameter (default: 1.0)
- `openai_max_completion_tokens` - Max response tokens (default: 4096)
- `openai_system_message` - Optional system message (default: null)

**Mock Verification**:
To verify that test fixtures match real OpenAI API responses:
```bash
OPENAI_API_KEY=sk-... make verify-mocks
```

This runs integration tests against the real API to ensure fixtures stay current. Only needed when API format changes.

**Dependencies**: yyjson (complete), libcurl (added)

**Documentation**:
- **[v1-llm-integration.md](v1-llm-integration.md)** - HTTP client design, streaming responses, event loop integration
- **[v1-conversation-management.md](v1-conversation-management.md)** - Message lifecycle, slash commands, /mark and /rewind

### Future: Database Integration (PostgreSQL)

**Objective**: Persistent conversation history

**Tasks**:
- PostgreSQL schema for conversations and messages
- Persistent conversation history
- Session management and conversation retrieval

**Documentation**:
- **[v1-database-design.md](v1-database-design.md)** - PostgreSQL schema, persistence strategy, session management

### Future: Local Tool Execution

**Objective**: Enable file operations, shell commands, and code analysis

**Tasks**:
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Code analysis (tree-sitter integration)
- Results flow back to conversation

### Future: Multi-LLM Provider Support

**Objective**: Support multiple LLM providers with unified interface

**Tasks**:
- Abstract provider interface
- OpenAI, Anthropic, Google, X.AI implementations
- Provider switching via config or runtime command
- Unified conversation format

### Future: Layer Architecture Refinement

**Objective**: Remove adapter layer and integrate components directly with layer cake

**Tasks**:
- Remove `layer_wrappers.c` adapter abstraction
- Update scrollback, separator, and input components to implement layer interface directly
- Consolidate layer creation logic into REPL initialization
- Reduce indirection and simplify layer management

**Rationale**: The adapter pattern in `layer_wrappers.c` was useful for prototyping but adds unnecessary indirection. Direct implementation of the layer interface by UI components will simplify the codebase.

### Future: Enhanced Terminal UI

**Objective**: Polish the user experience

**Tasks**:
- Syntax highlighting in code blocks (tree-sitter)
- External editor integration ($EDITOR)
- Command history and session management
- Rich formatting and themes

### Future: Code Organization and Module Cleanup

**Objective**: Refactor codebase structure for better organization and maintainability

**Tasks**:
- Reorganize source into subfolder-per-module structure
- Consolidate to one public header (*.h) per module/subfolder
- Standardize naming conventions across all modules
- Remove code redundancies and duplications
- Reorganize and consolidate test structure
- Improve module boundaries and interfaces
- Clean up internal module organization

**Rationale**: This is the standard refactoring cycle before each major release. As features are added throughout development, technical debt accumulates and module boundaries become less clear. A dedicated refactoring phase before release ensures the codebase is clean, well-organized, and maintainable for the next development cycle.

## v2.0 Vision

v2.0 focuses on intelligent context management through comprehensive RAG infrastructure. Automated indexing and summarization will maintain conversation history, with retrieval systems constructing optimal context for each LLM request. The model gains tools to search and retrieve historical messages, while the architecture introduces concurrency foundations for background processing of RAG operations.

---

**Note**: This documentation is maintained for AI agents and developers. Keep it concise, accurate, and current.
