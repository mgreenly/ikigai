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
- **[autonomous-agents.md](autonomous-agents.md)** - Autonomous agent system: identity, communication, hierarchy, deployment
- **[services.md](services.md)** - Services infrastructure: nginx, PostgreSQL, runit for dev and production
- **[decisions/](decisions/)** - Architecture Decision Records (ADRs) for key design choices
- **[repl/](repl/)** - REPL terminal interface documentation (rel-01 - complete)

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

### Agent Tools & Automation
- **[agent-scripts.md](agent-scripts.md)** - Script-based tools architecture (vs MCP), JSON output contract, discovery workflow
- **[task-system.md](task-system.md)** - Hierarchical task execution system architecture and API reference
- **[task-system-guide.md](task-system-guide.md)** - Task system usage guide with workflows, templates, and examples

### Internal Analysis
- **[memory_usage_analysis.md](memory_usage_analysis.md)** - Memory usage analysis and optimization notes
- **[considerations.md](considerations.md)** - Candidate features and changes to consider for the future
- **[../fix.md](../fix.md)** - Known issues and technical debt

## Roadmap to v1.0

**Architecture Overview**:
- **[v1-architecture.md](v1-architecture.md)** - v1.0 core architecture: scrollback as context window, three-layer model
- **[v1-implementation-roadmap.md](v1-implementation-roadmap.md)** - Phase-by-phase implementation plan with dependencies

### rel-01: REPL Terminal Foundation

**Objective**: Production-ready terminal interface with direct rendering and UTF-8 support

**Tasks**:
- Direct terminal rendering (single write per frame)
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with readline-style shortcuts
- Scrollback buffer with O(1) arithmetic reflow
- Viewport scrolling (Page Up/Down, auto-scroll)
- 100% test coverage

### rel-02: LLM Integration

**Objective**: Stream LLM responses directly to the terminal

**Tasks**:
- OpenAI API client with libcurl streaming
- Display AI responses in scrollback as chunks arrive
- Basic conversation flow (user input → API call → streamed response)
- Status indicators (loading spinner, error handling)
- Layer architecture (scrollback, spinner, separator, input)
- Slash commands (/clear, /mark, /rewind, /help, /model, /system)
- In-memory conversation state with checkpoint/rollback
- Mock verification test suite

### rel-03: Database Integration (PostgreSQL)

**Objective**: Persistent conversation history with optional database mode

**Tasks**:
- PostgreSQL schema with event stream model
- Automatic migration system (runs on startup)
- Session lifecycle with active detection (Model B)
- Message persistence at 5 integration points
- Replay algorithm for state reconstruction
- Integration testing with transaction isolation
- Session restoration on launch
- Memory-only fallback mode

### rel-04: Local Tool Execution

**Objective**: Enable file operations, shell commands, and code analysis

**Tasks**:
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Code analysis (tree-sitter integration)
- Results flow back to conversation

### Future: Background Agents

**Objective**: Support multiple concurrent agents with human and LLM spawning

**Tasks**:
- Multiple top-level agents (human-spawned via command or hotkey)
- LLM-spawned sub-agents for parallel task execution
- Refactor existing single-agent code for multi-agent architecture
- Hot-key navigation between active agents (e.g., Alt+1, Alt+2)
- Agent lifecycle management (spawn, suspend, resume, terminate)
- Slash commands for agent control (/agents, /spawn, /switch, /kill)
- Visual indicator of active agent and background agent status
- Inter-agent communication (optional: message passing between agents)

**Rationale**: Complex tasks benefit from parallel execution. Human users may want multiple conversations active simultaneously. LLMs can delegate subtasks to specialized sub-agents, improving throughput and enabling divide-and-conquer workflows.

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

### Future: Multi-LLM Provider Support

**Objective**: Support multiple LLM providers with unified interface

**Tasks**:
- Abstract provider interface
- OpenAI, Anthropic, Google, X.AI implementations
- Provider switching via config or runtime command
- Unified conversation format

### Future: Refactor

**Objective**: Improve code quality through better patterns, dependency injection, and reduced indirection

**Tasks**:
- Improve dependency injection consistency across modules
- Apply design patterns more uniformly (strategy, factory, context struct)
- Remove `layer_wrappers.c` adapter abstraction
- Update scrollback, separator, and input components to implement layer interface directly
- Consolidate layer creation logic into REPL initialization
- Reduce indirection and simplify layer management
- Review and improve module boundaries
- Eliminate unnecessary abstractions introduced during prototyping

**Rationale**: As features are added, abstractions accumulate that made sense during prototyping but add unnecessary complexity. Periodic refactoring improves consistency, reduces cognitive load, and ensures patterns are applied uniformly.

### Future: Enhanced Terminal UI

**Objective**: Polish the user experience

**Tasks**:
- Syntax highlighting in code blocks (tree-sitter)
- External editor integration ($EDITOR)
- Command history and session management
- Rich formatting and themes

## v2.0 Vision

v2.0 focuses on intelligent context management through comprehensive RAG infrastructure. Automated indexing and summarization will maintain conversation history, with retrieval systems constructing optimal context for each LLM request. The model gains tools to search and retrieve historical messages, while the architecture introduces concurrency foundations for background processing of RAG operations.

---

**Note**: This documentation is maintained for AI agents and developers. Keep it concise, accurate, and current.
