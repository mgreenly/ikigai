# Architecture

## Overview

Client-server architecture with WebSocket communication. Server proxies LLM requests and manages conversation storage. Client executes all tools locally.

## Binaries

- `bin/ikigai` - Client (terminal interface)
- `bin/ikigai-server` - Server (WebSocket listener, LLM proxy)

## Dependencies

### Core Libraries
- **libulfius** - HTTP/WebSocket server framework
- **libjansson** - JSON serialization
- **libcurl** - HTTP client for LLM APIs
- **libvterm** - Terminal emulator for client
- **libpq** - PostgreSQL C client library
- **libhiredis** - C client for Valkey/Redis
- **libuuid** - RFC 4122 UUID generation (util-linux)
- **check** - Unit testing framework

### Additional Libraries
- **libb64** - Base64 encoding for UUIDs (Phase 1)
- **talloc** - Hierarchical pool-based memory allocator (Phase 1)

### Future Libraries
- **libutf8proc** - UTF-8 text processing and Unicode normalization
- **libpcre2** - Perl-compatible regex library for text processing
- **libtree-sitter** - Incremental parsing library for code analysis
- **jemalloc** - Scalable concurrent malloc implementation

### Storage Systems
- **PostgreSQL** - Persistent conversation history
- **Valkey** - Cache, session storage, and message queue

Target platform: Debian 13 (Trixie)

## Implementation Phases

**Note**: These are incremental build phases, not standalone releases. Each phase establishes production-quality foundations for subsequent features.

### Phase 1: WebSocket Server with Streaming LLM Proxy

**Architectural Foundations Established:**
- **Concurrency model**: Worker threads + abort semantics (required for correct shutdown, used in all phases)
- **Memory management**: talloc patterns (hierarchical contexts, no manual free)
- **Error handling**: Result types with talloc integration
- **HTTP/WebSocket server**: libulfius configuration and lifecycle
- **Testing patterns**: TDD with Check framework, OOM injection via weak symbol test seams
- **External library wrappers**: Mockable functions for comprehensive error path testing

**Functional Scope:**
- WebSocket connection between client and server (`ws://localhost:1984/ws`)
- Handshake protocol (hello/welcome) with session_id generation
- Server loads config from `~/.ikigai/config.json`
- Server forwards queries to OpenAI API (gpt-4o-mini) with streaming
- Stream responses back to client
- No message history, no tools, no storage (deferred to later phases)

**Client:** stdio stub for basic verification; Python WebSocket client for testing

**Testing:** Check framework with real OpenAI API during development; mocked responses for CI

### Phase 2: LLM Provider Abstraction
- Abstract provider interface (function pointers)
- Support OpenAI, Anthropic, Google

### Phase 3: Message History
- Add PostgreSQL storage
- Store conversations, exchanges, messages
- Send message history to LLM

### Phase 4: Client Terminal (libvterm)
- Handle concurrent background messages during user input
- External editor support ($EDITOR)
- Proper terminal UI for streaming responses

### Phase 5: Web Search Tool
- Web search tool proxies through server
- Client executes tool, implementation calls server

### Phase 6: Client-Side Tools
- File operations and shell commands execute locally
- Uniform tool interface (client doesn't know which proxy to server)
- Full trust model (no sandboxing)

## Identity

- **Client identity**: `hostname@username` (trust-based, no authentication)
- **Session ID**: UUID per WebSocket connection
- **Correlation ID**: UUID per exchange (server-generated, for logging and observability)

## Concurrency Model

The server handles one request at a time per WebSocket connection, blocking until the request completes (e.g., OpenAI streams until `[DONE]`). Clients open multiple connections for concurrent operations—one for streaming LLM responses, another for file operations, etc. This keeps both client and server code simple and synchronous while enabling practical concurrency. Each connection maintains independent state with its own `session_id`.

## Server Configuration

Server loads configuration from `~/.ikigai/config.json`:

**Phase 1:**
```json
{
  "openai_api_key": "sk-...",
  "listen_address": "127.0.0.1",
  "listen_port": 1984
}
```

Future phases will add database connection strings and other provider API keys.
