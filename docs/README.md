# ikigai Documentation Index

This directory contains documentation primarily for AI agents working on the ikigai project, and secondarily for human developers.

## Project Overview

**ikigai** is a multi-user AI coding agent with organizational memory. Written in C.

Server: WebSocket-based LLM proxy with conversation storage and RAG retrieval. Client: Terminal interface with libvterm that executes tools locally.

Built for experimentation and iteration. All conversations feed a shared knowledge base.

**Current Status**: Phase 1 - Planning

## Codebase Structure

- **Monorepo**: `src/` contains both client and server code
- **Build targets**: `bin/ikigai` (client) and `bin/ikigai-server` (server)
- **Current state**: Both `server.c` and `client.c` are hello-world stubs
- **Build system**: Make with GNU indent formatting and complexity checks
- **Testing**: Uses Check framework
- **Standard**: C17 with `-Wall -Wextra`

## Documentation Files

- **[architecture.md](architecture.md)** - System architecture and implementation phases
- **[decisions.md](decisions.md)** - Architecture decision records (ADRs)
- **[protocol.md](protocol.md)** - Client-server WebSocket protocol
- **[storage.md](storage.md)** - Database schema
- **[naming.md](naming.md)** - Naming conventions and approved abbreviations
- **[memory.md](memory.md)** - Memory management with talloc, ownership rules, patterns
- **[error_handling.md](error_handling.md)** - Error handling with talloc-integrated Result types
- **[phase-1.md](phase-1.md)** - Phase 1 implementation details (module structure, dependencies, build order)
- **[phase-1-details.md](phase-1-details.md)** - Phase 1 detailed implementation specification
- **[shutdown-verification.md](shutdown-verification.md)** - ⚠️ **REQUIRED TEST** - Verify libulfius shutdown behavior before Phase 1 implementation

## Development Phases

These are **incremental build phases**, not standalone releases. Each phase builds on the previous one's foundation.

### Phase 1: Architectural Foundation

**Goal**: Implement the core architectural patterns that all subsequent phases depend on.

**Not a shippable product** - Phase 1 establishes production-quality foundations, not end-user features. Think of it as building the frame and foundation of a house before adding rooms.

**What Phase 1 builds:**
- **Concurrency model**: Worker threads with abort semantics (needed for shutdown, DB operations, tool execution in later phases)
- **Memory management**: talloc patterns for hierarchical allocation (prevents entire classes of bugs)
- **Error handling**: Result types with CHECK macro propagation (systematic error flow)
- **HTTP/WebSocket server**: libulfius lifecycle and connection management
- **Streaming architecture**: SSE parsing, chunk forwarding, client disconnect handling
- **OpenAI integration**: Proof that streaming LLM proxy pattern works

**What Phase 1 validates:**
- libulfius shutdown behavior with active callbacks (shutdown-verification.md required)
- talloc + worker threads + Result types compose correctly
- Graceful shutdown with in-flight requests works

**What Phase 1 omits** (added in later phases):
- No persistence or message history (Phase 3)
- No tool execution (Phases 5-6)
- No multiple providers (Phase 2)
- No production client UI (Phase 4)

**Why this complexity in Phase 1?** These architectural decisions can't be retrofitted later without massive refactoring. Get the foundation right first, then build features on top.

**Phase 2**: Abstract LLM provider interface (OpenAI, Anthropic, Google)

**Phase 3**: Add PostgreSQL storage and message history

**Phase 4**: Client terminal with libvterm - concurrent background messages, external editor support

**Phase 5**: Server-side web search tool

**Phase 6**: Client-side tools (dir_list, file_read) with delegation protocol

---

**Note**: Documentation maintained for agent and developer understanding. Keep concise and current.
