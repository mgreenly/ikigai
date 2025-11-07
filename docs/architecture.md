# Architecture

## Overview

Desktop AI coding agent with local tool execution and persistent conversation history. Direct LLM API integration with streaming support.

## Binary

- `bin/ikigai` - Desktop client (terminal interface)

## Dependencies

### Core Libraries
- **libvterm** - Terminal emulator for client UI
- **libjansson** - JSON serialization
- **libcurl** - HTTP client for LLM APIs
- **libb64** - Base64 encoding for identifiers
- **talloc** - Hierarchical pool-based memory allocator
- **libuuid** - RFC 4122 UUID generation (util-linux)
- **check** - Unit testing framework

### Database (To Be Selected)
- **libpq** - PostgreSQL C client library (option 1)
- **libduckdb** - DuckDB embedded database (option 2)

### Future Libraries
- **libutf8proc** - UTF-8 text processing and Unicode normalization
- **libpcre2** - Perl-compatible regex library for text processing
- **libtree-sitter** - Incremental parsing library for code analysis

Target platform: Debian 13 (Trixie)

## v1.0 Architecture

### Component Overview

```
┌─────────────────────────────────────────────────┐
│              bin/ikigai                          │
│                                                  │
│  ┌────────────────┐        ┌─────────────────┐  │
│  │  Terminal UI   │        │   LLM Clients   │  │
│  │   (libvterm)   │        │   (streaming)   │  │
│  │                │        │                 │  │
│  │ - Split buffer │        │ - OpenAI       │  │
│  │ - Scrollback   │        │ - Anthropic    │  │
│  │ - Input zone   │        │ - Google       │  │
│  └────────────────┘        └─────────────────┘  │
│                                                  │
│  ┌────────────────┐        ┌─────────────────┐  │
│  │  Local Tools   │        │    Database     │  │
│  │                │        │   (PostgreSQL   │  │
│  │ - File ops     │        │    or DuckDB)   │  │
│  │ - Shell exec   │        │                 │  │
│  │ - Code analysis│        │ - Conversations │  │
│  └────────────────┘        │ - Messages      │  │
│                            │ - Search        │  │
│  ┌────────────────┐        └─────────────────┘  │
│  │  Config        │                              │
│  │  ~/.ikigai/    │                              │
│  └────────────────┘                              │
└─────────────────────────────────────────────────┘
```

### Design Principles

**Local-first**: Everything runs on the user's machine. No external server dependencies except LLM APIs.

**Direct API integration**: Client talks directly to OpenAI/Anthropic/Google APIs using libcurl with streaming.

**Persistent memory**: All conversations stored locally in database for search and context.

**Full trust model**: Tools execute with user's permissions. No sandboxing. User's machine, user's responsibility.

**Single-threaded simplicity**: Main event loop handles terminal input, LLM streaming, and tool execution sequentially. Async complexity deferred to future explorations.

## Implementation Roadmap

### Current: REPL Terminal Foundation

**Foundation**: Terminal interface mechanics without LLM integration.

Establishes:
- Terminal initialization (raw mode, alternate screen)
- Split-buffer layout (scrollback + dynamic input zone)
- Input handling (keyboard, mouse, scrolling)
- Rendering pipeline (compose + blit)
- Config module integration

**Deliverable**: Working REPL where lines move from input to scrollback. Foundation for streaming AI responses.

See [repl-terminal.md](repl-terminal.md) for detailed design.

### Next: OpenAI Integration

Add streaming LLM responses to the REPL.

Features:
- OpenAI API client (libcurl + streaming)
- Send prompts, display streaming responses
- Show spinner/status while waiting
- Append chunks to scrollback as they arrive

### Future: Database Persistence

Store conversation history locally.

Features:
- Choose database (PostgreSQL or DuckDB)
- Schema for conversations, messages
- Save/load conversation history
- Search across past conversations

### Future: Multi-LLM Support

Abstract provider interface.

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
    "type": "postgresql",
    "connection_string": "postgresql://localhost/ikigai"
  }
}
```

## Memory Management

All allocations use **talloc** for hierarchical context management:
- Main context owns all subsystems
- Cleanup is automatic with `talloc_free()`
- No manual free tracking
- Memory leaks eliminated by design

See [memory.md](memory.md) for detailed patterns.

## Error Handling

**Result types** with talloc integration:
- Functions return `Result` wrappers (Ok/Err)
- `CHECK()` macro propagates errors up call stack
- Errors carry context strings for debugging
- Systematic error flow throughout codebase

See [error_handling.md](error_handling.md) for patterns.

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

## Concurrency Model (Future)

v1.0 is single-threaded for simplicity. Future explorations may add:
- Worker threads for long-running tool execution
- Async I/O for LLM streaming
- Background database queries

Keep it simple until proven necessary.

## Future: Server Architecture

Post-v1.0 may explore multi-user server. See [roadmap.md](roadmap.md).

Key concepts from earlier design (see `docs/archive/` for details):
- WebSocket protocol for client-server communication
- Server proxies LLM requests
- Centralized conversation storage
- Tools still execute locally on client machines

Not in scope for v1.0.
