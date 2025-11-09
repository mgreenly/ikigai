# ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**ikigai** is a Linux focused desktop AI coding agent with persistent memory, written in "C".

A terminal-based assistant that executes tools locally, remembers conversations in a database, and works with multiple LLM providers. Built for experimentation and iteration.

**Current Status**: Early development - Building REPL terminal foundation

## v1.0 Vision

**Long-term target**: Robust desktop client with:
- **Database integration**: Persistent conversation history (PostgreSQL/DuckDB)
- **Multi-LLM support**: OpenAI, Anthropic, Google with unified interface
- **Local tool execution**: File operations, shell commands, code analysis
- **Robust CLI**: Terminal UI with streaming responses, scrollback, editor integration

**What v1.0 is NOT**:
- Not a multi-user server (future exploration)
- Not a web service (local desktop tool)
- Not trying to be production infrastructure (experimentation platform)

## Codebase Structure

- **Source**: `src/` contians the source code
- **Build target**: `bin/ikigai` (desktop client)
- **Build system**: Make with comprehensive warnings, sanitizers, Valgrind, and coverage support - see [build-system.md](build-system.md)
- **Testing**: Check framework with unit and integration tests; OOM injection via test seams, exists in `tests/` folder.
- **Standard**: C17 with K&R style (120-char width)

## Documentation Files

### Current Work
- **[repl/](repl/)** - REPL terminal interface (current implementation target)

### Core Documentation
- **[architecture.md](architecture.md)** - System architecture and design decisions
- **[decisions/](decisions/)** - Architecture decision records (ADRs) for key design choices
- **[naming.md](naming.md)** - Naming conventions and approved abbreviations
- **[memory.md](memory.md)** - Memory management with talloc, ownership rules, patterns
- **[error_handling.md](error_handling.md)** - Error handling with talloc-integrated Result types
- **[build-system.md](build-system.md)** - Build system with quality gates, testing infrastructure, and multi-distro support
- **[vulnerabilities.md](vulnerabilities.md)** - CVE vulnerability tracking for dependencies

### Future Work
- **[roadmap.md](roadmap.md)** - Post-v1.0 exploration areas
- **[archive/](archive/)** - Earlier server-focused design docs (historical reference)

## Development Roadmap

### Immediate: REPL Terminal Foundation
**Current work target** - See [repl/](repl/) for details.

Building a terminal interface with:
- Split-buffer layout (scrollback + dynamic input)
- Alternate screen mode with clean exit
- Mouse and keyboard scrolling
- Config integration
- Foundation for streaming AI responses

### Future: Core v1.0 Features

**Database Integration**
- Choose storage backend (PostgreSQL or DuckDB)
- Persistent conversation history
- Message storage and retrieval
- Query/search capabilities

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

**Enhanced Terminal Client**
- Streaming response display
- External editor integration ($EDITOR)
- Command history
- Session management
- Rich formatting

### Future Exploration
See [roadmap.md](roadmap.md) for post-v1.0 directions including:
- Multi-user server architecture
- WebSocket protocol for remote access
- Organizational memory and RAG
- Collaborative features

## Development Philosophy

**Build incrementally**. Each phase establishes solid foundations before adding features. Focus on getting core patterns right (memory management, error handling, concurrency) early.

**Experiment freely**. This is a platform for trying ideas, not production infrastructure. Make architectural choices that enable rapid iteration.

**Keep it focused**. v1.0 is about a great desktop experience. Server/multi-user features are future exploration, not current distractions.

---

**Note**: Documentation maintained for agent and developer understanding. Keep concise and current.
