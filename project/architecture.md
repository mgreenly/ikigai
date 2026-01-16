# Architecture

## Overview

Desktop AI coding agent with local tool execution and persistent conversation history. Direct LLM API integration with streaming support.

## Binary

- `bin/ikigai` - Desktop client (terminal interface)

## Dependencies

### Current Libraries (rel-08 - External Tools Complete)
- **yyjson** - JSON parsing with talloc integration (vendored, migration from jansson complete)
- **talloc** - Hierarchical pool-based memory allocator ([why?](decisions/talloc-memory-management.md))
- **libutf8proc** - UTF-8 text processing and Unicode normalization
- **libcurl** - HTTP client for streaming LLM APIs
- **pthread** - POSIX threads (required by logger and check framework)
- **check** - Unit testing framework
- **libb64** - Base64 encoding (linked, not yet actively used)
- **libuuid** - RFC 4122 UUID generation (linked, not yet actively used)

### Future Libraries
- **libexpat** - SAX XML parser for reading Anthropic API tool call responses (stream-oriented, read-only)
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
│  │ External Tools │        │    Database     │  │
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

**Status**: rel-01 released (2025-11-16) - Full REPL with scrollback, viewport scrolling, and 100% test coverage.

Implemented:
- Terminal initialization (raw mode, direct rendering)
- Input handling (multi-line editing, readline shortcuts, UTF-8 support)
- Scrollback buffer with O(1) arithmetic reflow (726× faster than target)
- Viewport scrolling (Page Up/Down, auto-scroll on submit)
- Rendering pipeline (single framebuffer write per frame)
- Pretty-print infrastructure (format module, pp functions)
- Config module (yyjson-based, currently loading scrollback_lines setting)

**Deliverable**: Production-ready REPL with direct terminal rendering. Foundation ready for streaming AI responses.

See [repl/README.md](repl/README.md) for detailed design and [plan.md](../plan.md) for implementation phases.

### Completed: OpenAI Integration ✅

**Status**: rel-02 released (2025-11-22) - Streaming LLM responses with full conversation management.

([why OpenAI format?](decisions/openai-api-format.md))

Implemented:
- OpenAI API client with libcurl streaming
- Display AI responses in scrollback as chunks arrive
- Basic conversation flow (user input → API call → streamed response)
- Status indicators (loading spinner, error handling)
- Layer architecture (scrollback, spinner, separator, input)
- Slash commands (/clear, /mark, /rewind, /help, /model, /system)
- In-memory conversation state with checkpoint/rollback
- Mock verification test suite

**Deliverable**: Full conversational AI agent with streaming responses and conversation management. Foundation ready for database persistence.

### Completed: External Tools ✅

**Status**: rel-08 released (2026-01-15) - External tool system with LLM integration complete.

Implemented:
- External tool discovery from 3 directories (core, user home, project)
- JSON protocol for tool communication (stdin/stdout)
- Self-description via --schema flag
- 6 operational tools: bash, grep, glob, file_read, file_write, file_edit
- Integration with LLM tool calling
- Tool execution results flow back to conversation

**Deliverable**: Full local tool execution capability with file operations and shell execution. LLM can now perform actions on the local filesystem.

### Future: Database Persistence

Store conversation history locally with PostgreSQL. ([why PostgreSQL?](decisions/postgresql-valkey.md))

Features:
- PostgreSQL schema for conversations and messages
- Persistent conversation history
- Full-text search across past conversations (tool-based)
- RAG memory access patterns

### Future: Multi-LLM Support

Abstract provider interface. ([superset approach](decisions/superset-api-approach.md))

Features:
- Provider abstraction layer (function pointers)
- OpenAI implementation
- Anthropic implementation
- Google implementation
- Switch providers via config or command

### Future: Advanced Tool Features

Extend tool capabilities with code analysis.

Features:
- Code analysis tools (tree-sitter integration)
- Advanced text processing tools
- Project-specific custom tools
- Tool composition and chaining

### Future: Enhanced Terminal

Polish the UI experience.

Features:
- Syntax highlighting in code blocks
- External editor integration ($EDITOR)
- Multi-line input with editing
- Command history
- Rich formatting

## Configuration

Client loads configuration from `~/.config/ikigai/config.json`.

If the config file doesn't exist, a default configuration is created automatically on first run.

**Current (rel-08 - External Tools)**:
```json
{
  "openai_api_key": "YOUR_API_KEY_HERE",
  "openai_model": "gpt-5-mini",
  "openai_temperature": 1.0,
  "openai_max_completion_tokens": 4096,
  "openai_system_message": null,
  "listen_address": "127.0.0.1",
  "listen_port": 1984
}
```

**Configuration Fields**:
- `openai_api_key` - Your OpenAI API key (required for LLM functionality)
- `openai_model` - Model to use (default: "gpt-5-mini")
- `openai_temperature` - Temperature parameter (default: 1.0)
- `openai_max_completion_tokens` - Max response tokens (default: 4096)
- `openai_system_message` - Optional system message (default: null)
- `listen_address` - Legacy field from previous architecture (not currently used)
- `listen_port` - Legacy field from previous architecture (not currently used)

**Future (Multi-LLM Support)**:

Additional provider fields will be added for Anthropic, Google, and X.AI:
```json
{
  "openai_api_key": "sk-...",
  "openai_model": "gpt-5-mini",
  "anthropic_api_key": "sk-ant-...",
  "anthropic_model": "claude-sonnet-4.5",
  "google_api_key": "...",
  "google_model": "gemini-2.0-flash",
  "default_provider": "openai"
}
```

**Future (Database Integration)**:

PostgreSQL connection settings will be added:
```json
{
  "openai_api_key": "sk-...",
  "database_connection_string": "postgresql://localhost/ikigai"
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
