---
name: ddd
description: Domain-Driven Design (DDD) skill for the ikigai project
---

# Domain-Driven Design (DDD)

## Ubiquitous Language

### Core Domain: Terminal AI Coding Agent

- **REPL** - Main interactive loop (`ik_repl_ctx_t`). Reads input, processes, displays results.
- **Scrollback** - Immutable conversation history. CRITICAL: Scrollback IS the context window (WYSIWYG).
- **Input Buffer** - Editable multi-line user input. Supports readline shortcuts and UTF-8.
- **Viewport** - Visible scrollback portion. Scrollable via Page Up/Down.
- **Layer** - Independent rendering component (scrollback, separator, input, spinner).
- **Session** - Conversation instance. Maps to database record. Contains active message array.
- **Message** - Conversation unit with role (user/assistant/system/mark/rewind), content, timestamp, tokens, model.
- **Context Window** - Messages sent to LLM. ALWAYS equals scrollback messages.
- **Mark** - Checkpoint via /mark. Enables /rewind to restore context.
- **Streaming** - Progressive LLM response delivery via HTTP streaming.
- **Provider** - LLM service (OpenAI, Anthropic, Google, X.AI). Unified superset API.
- **Archive** - Database storage. Permanent, never auto-loaded. User searches and selectively loads.

### Memory Management

- **talloc Context** - Hierarchical memory arena. Free parent = free subtree.
- **Ownership** - Each allocation has exactly one owner responsible for cleanup.
- **Result Type** - OK/ERR with optional error context. Use `CHECK()` and `TRY()` macros.
- **PANIC** - Unrecoverable only (OOM, corruption). Terminates immediately.

### Architectural Concepts

**Three-Layer Separation**:
1. **Display** (scrollback) - Decorated, formatted, ephemeral. ANSI/colors at render time.
2. **Active Context** (session messages) - `ik_message_t[]` sent to LLM API.
3. **Archive** (database) - All messages, searchable, never auto-injected.

**Core Principles**:
- Scrollback visibility = LLM context (explicit user control)
- Single-threaded event loop (input → parse → mutate → render)
- Direct terminal control (raw ANSI, no curses, single framebuffer write)
- Graceful degradation (never crash for recoverable errors)

## Core Entities

- **`ik_repl_ctx_t`** - Root aggregate. Owns all subsystems.
- **`ik_message_t`** - Immutable value object. Primary conversation entity.
- **`ik_scrollback_t`** - Display line manager. Append-only with O(1) reflow.
- **`ik_input_buffer_t`** - User input manager. Mutable, multi-line.
- **`ik_term_ctx_t`** - Terminal state (raw mode, dimensions, capabilities).

## Core Services

- **Rendering** (`render.c`) - Builds framebuffer from layers, atomic write.
- **LLM Client** (`openai.c`) - HTTP streaming. Abstracts provider differences.
- **Database** (`db.c`) - PostgreSQL persistence. Synchronous writes.
- **Config** (`config.c`) - Loads `~/.config/ikigai/config.json`.
- **Command** (`commands.c`) - Slash command dispatch.

## Bounded Contexts

- **Terminal UI** - REPL, scrollback, input, rendering, viewport, layers.
- **Conversation** - Sessions, messages, marks, context window.
- **LLM Integration** - Providers, streaming, API requests, tokens.
- **Persistence** - Database, sessions, messages, full-text search.
- **Memory Management** - talloc, ownership, Result types (cross-cutting).

## Key Invariants

- Scrollback MUST match session_messages (same content, same order)
- Session messages = what gets sent to LLM
- Database writes succeed before updating in-memory state
- Exactly one owner per allocation (talloc parent-child)
- 100% test coverage maintained
- Never run parallel make commands

## Anti-Patterns

- Auto-loading from database (breaks explicit control)
- Hidden context injection (violates WYSIWYG)
- malloc/free instead of talloc (breaks ownership)
- PANIC for recoverable errors
- Creating files when editing existing ones works
- Over-engineering (KISS/YAGNI)
