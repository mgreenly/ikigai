# Architecture

## Overview

Desktop AI coding agent with local tool execution and persistent conversation history. Direct LLM API integration with streaming support.

## Binary

- `bin/ikigai` - Desktop client (terminal interface)

## Dependencies

### Current Libraries (Phase 4 - REPL Complete)
- **jansson** - JSON for config parsing (will migrate to yyjson during LLM integration)
- **libb64** - Base64 encoding for identifiers
- **talloc** - Hierarchical pool-based memory allocator ([why?](decisions/talloc-memory-management.md))
- **libuuid** - RFC 4122 UUID generation (util-linux)
- **libutf8proc** - UTF-8 text processing and Unicode normalization
- **pthread** - POSIX threads (required by check framework)
- **check** - Unit testing framework

### Next Phase (LLM Integration)
- **yyjson** - JSON serialization with talloc integration (replacing jansson)
- **libcurl** - HTTP client for streaming LLM APIs

### Future Libraries
- **libpq** - PostgreSQL C client library (database integration)
- **libpcre2** - Perl-compatible regex library for text processing
- **libtree-sitter** - Incremental parsing library for code analysis

Target platform: Debian 13 (Trixie)

## v1.0 Architecture

### Component Overview

```
┌─────────────────────────────────────────────────┐
│              bin/ikigai                         │
│                                                 │
│  ┌────────────────┐        ┌─────────────────┐  │
│  │  Terminal UI   │        │   LLM Clients   │  │
│  │  (direct term) │        │   (streaming)   │  │
│  │                │        │                 │  │
│  │ - Split buffer │        │ - OpenAI        │  │
│  │ - Scrollback   │        │ - Anthropic     │  │
│  │ - Input zone   │        │ - Google        │  │
│  └────────────────┘        └─────────────────┘  │
│                                                 │
│  ┌────────────────┐        ┌─────────────────┐  │
│  │  Local Tools   │        │    Database     │  │
│  │                │        │  (PostgreSQL)   │  │
│  │ - File ops     │        │                 │  │
│  │ - Shell exec   │        │ - Conversations │  │
│  │ - Code analysis│        │ - Messages      │  │
│  └────────────────┘        │ - Search        │  │
│                            │ - RAG memory    │  │
│  ┌────────────────┐        └─────────────────┘  │
│  │  Config        │                             │
│  │  ~/.ikigai/    │                             │
│  └────────────────┘                             │
└─────────────────────────────────────────────────┘
```

### Design Principles

**Local-first**: Everything runs on the user's machine. No external server dependencies except LLM APIs.

**Direct API integration**: Client talks directly to OpenAI/Anthropic/Google APIs using libcurl with streaming.

**Persistent memory**: All conversations stored locally in PostgreSQL for search, context, and RAG. ([why PostgreSQL?](decisions/postgresql-valkey.md))

**Full trust model**: Tools execute with user's permissions. No sandboxing. User's machine, user's responsibility. ([why client-side?](decisions/client-side-tool-execution.md))

**Single-threaded simplicity**: Main event loop handles terminal input, LLM streaming, and tool execution sequentially.

## Implementation Roadmap

([why phased?](decisions/phased-implementation.md))

### Completed: REPL Terminal Foundation ✅

**Status**: Phase 4 complete (2025-11-14) - Full REPL with scrollback, viewport scrolling, and 100% test coverage.

Implemented:
- Terminal initialization (raw mode, direct rendering)
- Input handling (multi-line editing, readline shortcuts, UTF-8 support)
- Scrollback buffer with O(1) arithmetic reflow (726× faster than target)
- Viewport scrolling (Page Up/Down, auto-scroll on submit)
- Rendering pipeline (single framebuffer write per frame)
- Pretty-print infrastructure (format module, pp functions)
- Config module integration

**Deliverable**: Production-ready REPL with direct terminal rendering. Foundation ready for streaming AI responses.

See [repl/README.md](repl/README.md) for detailed design and [plan.md](../plan.md) for implementation phases.

### Next: OpenAI Integration

Add streaming LLM responses to the REPL. ([why OpenAI format?](decisions/openai-api-format.md))

Features:
- OpenAI API client (libcurl + streaming)
- Send prompts, display streaming responses
- Show spinner/status while waiting
- Append chunks to scrollback as they arrive

### Future: Database Persistence

Store conversation history locally with PostgreSQL. ([why PostgreSQL?](decisions/postgresql-valkey.md))

Features:
- PostgreSQL schema for conversations and messages
- Save/load conversation history
- Full-text search across past conversations
- RAG memory access patterns

### Future: Multi-LLM Support

Abstract provider interface. ([superset approach](decisions/superset-api-approach.md))

Features:
- Provider abstraction layer (function pointers)
- OpenAI implementation
- Anthropic implementation
- Google implementation
- Switch providers via config or command

### Future: Local Tools

Enable file operations and shell execution.

Features:
- Tool interface design
- File read/write/search
- Shell command execution
- Code analysis (tree-sitter)
- Results flow back to conversation

### Future: Enhanced Terminal

Polish the UI experience.

Features:
- Syntax highlighting in code blocks
- External editor integration ($EDITOR)
- Multi-line input with editing
- Command history
- Rich formatting

## Configuration

Client loads configuration from `~/.ikigai/config.json`:

**Initial (REPL Terminal)**:
```json
{
  "terminal": {
    "scrollback_lines": 10000
  }
}
```

**With OpenAI Integration**:
```json
{
  "terminal": {
    "scrollback_lines": 10000
  },
  "llm": {
    "default_provider": "openai",
    "openai_api_key": "sk-..."
  }
}
```

**With Multi-LLM Support**:
```json
{
  "terminal": {
    "scrollback_lines": 10000
  },
  "llm": {
    "default_provider": "openai",
    "openai_api_key": "sk-...",
    "anthropic_api_key": "sk-ant-...",
    "google_api_key": "..."
  },
  "database": {
    "connection_string": "postgresql://localhost/ikigai"
  }
}
```

## Memory Management

All allocations use **talloc** for hierarchical context management ([why talloc?](decisions/talloc-memory-management.md)):
- Main context owns all subsystems
- Cleanup is automatic with `talloc_free()`
- No manual free tracking
- Memory leaks eliminated by design

See [memory.md](memory.md) for detailed patterns.

## Error Handling

**Three-tier error handling** with talloc integration:
- **Result types** for expected runtime errors (IO, parsing, validation)
- **Assertions** for development-time contract violations (compile out in release)
- **PANIC()** for unrecoverable errors (OOM, data corruption, impossible states)
- `CHECK()` and `TRY()` macros propagate errors up call stack
- Errors carry context strings for debugging
- Systematic error flow throughout codebase

**OOM handling:** Out of memory conditions call `PANIC("Out of memory")` which immediately terminates the process. Memory allocation failures are not recoverable.

See [error_handling.md](error_handling.md) for patterns and [panic.h](../src/panic.h) for PANIC implementation.

## Testing Strategy

**Test-Driven Development**:
- Write failing test first
- Implement minimal code to pass
- Run `make check && make lint && make coverage`
- 100% coverage requirement (lines, functions, branches)

**OOM Testing**:
- Weak symbol test seams for library functions
- Inject allocation failures
- Verify error path handling

**Integration Testing**:
- Real LLM API calls during development
- Mocked responses for CI
- Database tests with test fixtures

## Concurrency Model

v1.0 is single-threaded for simplicity:
- Main event loop handles terminal input sequentially
- LLM streaming processed as chunks arrive
- Tool execution blocks until complete

This keeps the implementation straightforward and avoids concurrency complexity. Future versions may explore async patterns if performance requires it.
