---
name: source-code
description: Source Code skill for the ikigai project
---

# Source Code

## Description

Comprehensive reference for all C source files in the Ikigai REPL project organized by functional area.

## Core Infrastructure

- `src/client.c` - Main entry point for the REPL application, loads configuration and initializes the event loop.
- `src/shared.c` - Shared context initialization for terminal, rendering, database, and history.
- `src/panic.c` - Panic handler for unrecoverable errors with safe async-signal-safe cleanup.
- `src/error.c` - Error handling wrapper using talloc-based allocator for consistent memory management.
- `src/wrapper.c` - External library wrapper implementations providing link seams for testing with mockable functions.
- `src/config.c` - Configuration file loading and parsing with tilde expansion and default config creation.
- `src/logger.c` - Thread-safe logging system with ISO 8601 timestamps and local timezone support.
- `src/uuid.c` - UUID generation using base64url encoding for compact agent identifiers.

## Memory Management

- `src/array.c` - Generic expandable array implementation with configurable element size and growth strategy.
- `src/byte_array.c` - Typed wrapper for byte (uint8_t) arrays built on top of the generic array.
- `src/line_array.c` - Typed wrapper for line (char*) arrays built on top of the generic array.
- `src/json_allocator.c` - Talloc-based allocator for yyjson providing consistent memory management.

## Terminal Management

- `src/terminal.c` - Raw mode and alternate screen buffer management with CSI u support detection.
- `src/signal_handler.c` - Signal handling infrastructure for SIGWINCH (terminal resize) events.
- `src/ansi.c` - ANSI escape sequence parsing and color code generation utilities.

## Rendering System

- `src/render.c` - Direct ANSI terminal rendering with text and cursor positioning.
- `src/render_cursor.c` - Cursor screen position calculation accounting for UTF-8 widths and line wrapping.
- `src/layer.c` - Output buffer management for composable rendering layers.
- `src/layer_scrollback.c` - Scrollback layer wrapper that renders conversation history.
- `src/layer_input.c` - Input buffer layer wrapper that renders the current user input.
- `src/layer_separator.c` - Separator layer wrapper that renders horizontal separators with debug info and navigation context.
- `src/layer_spinner.c` - Spinner layer wrapper for animated loading indicators with frame cycling.
- `src/layer_completion.c` - Completion layer wrapper that renders tab completion suggestions.
- `src/event_render.c` - Universal event renderer that converts database events to styled scrollback content.

## Scrollback Buffer

- `src/scrollback.c` - Scrollback buffer implementation with line wrapping and layout caching.
- `src/scrollback_render.c` - Scrollback rendering helper functions for calculating display positions and byte offsets.
- `src/scrollback_utils.c` - Utility functions for scrollback text analysis including display width calculation with ANSI escape handling.
- `src/scroll_detector.c` - Distinguishes mouse wheel scrolling from keyboard arrow key presses using timing-based burst detection.

## Input System

- `src/input.c` - Input parser that converts raw bytes to semantic actions with UTF-8 and escape sequence handling.
- `src/input_escape.c` - Escape sequence parsing for terminal control codes with CSI sequence handling.
- `src/input_xkb.c` - XKB keyboard layout support for translating shifted keys to their base characters using reverse mapping.

## Input Buffer

- `src/input_buffer/core.c` - Input buffer text storage implementation with UTF-8 support and layout caching.
- `src/input_buffer/cursor.c` - Cursor position tracking with byte and grapheme offset management using utf8proc.
- `src/input_buffer/cursor_pp.c` - Cursor pretty-print implementation for debugging with structured output.
- `src/input_buffer/layout.c` - Input buffer layout caching for efficient display width calculation with ANSI handling.
- `src/input_buffer/multiline.c` - Multi-line navigation implementation for up/down arrow keys with line start/end detection.
- `src/input_buffer/pp.c` - Input buffer pretty-print implementation for debugging with nested structure visualization.

## REPL Core

- `src/repl.c` - REPL main event loop with select()-based multiplexing for input, HTTP, and tool execution.
- `src/repl_init.c` - REPL initialization and cleanup with session restoration support and agent tree reconstruction.
- `src/repl_viewport.c` - Viewport calculation logic for determining what's visible on screen with scroll offset tracking.
- `src/repl_callbacks.c` - HTTP callback handlers for streaming OpenAI responses with line buffering.
- `src/repl_event_handlers.c` - Event handlers for stdin, HTTP completion, tool completion, and timeouts with timeout calculation.
- `src/repl_tool.c` - Tool execution helper that runs tools in background threads with mutex-protected result passing.

## REPL Actions

- `src/repl_actions.c` - Core action processing including arrow key handling through scroll detector with multiline scrollback append.
- `src/repl_actions_llm.c` - LLM and slash command handling with conversation management and command dispatching.
- `src/repl_actions_viewport.c` - Viewport and scrolling actions (page up/down, scroll up/down) with max offset calculation.
- `src/repl_actions_history.c` - History navigation actions (Ctrl+P/N for previous/next) with pending entry preservation.
- `src/repl_actions_completion.c` - Tab completion functionality for slash commands with auto-update after character insertion.

## Agent Management

- `src/agent.c` - Agent context creation and lifecycle management with layer system, conversation state, and mutex initialization.
- `src/repl/agent_restore.c` - Agent restoration on startup with conversation replay, mark stack reconstruction, and sorted agent tree building.

## Commands

- `src/commands.c` - REPL command registry and dispatcher for slash commands (/clear, /help, /model, /system, /debug, etc).
- `src/commands_mark.c` - Mark and rewind command implementations for conversation checkpoints with database truncation.
- `src/commands_fork.c` - Fork command handler for creating child agents with quoted prompt parsing and conversation cloning.
- `src/commands_kill.c` - Kill command handler for terminating agents and descendants with depth-first collection and database updates.
- `src/commands_mail.c` - Mail command handlers (/send, /inbox) for inter-agent messaging with unread count tracking.
- `src/commands_agent_list.c` - /agents command implementation for displaying agent hierarchy tree with indentation and status.
- `src/marks.c` - Mark creation and management with ISO 8601 timestamps and scrollback rendering.
- `src/completion.c` - Tab completion data structures and fuzzy matching logic with model/agent argument providers.

## History

- `src/history.c` - Command history management with persistence to JSON file, capacity limits, and duplicate detection.

## Database Layer

- `src/db/connection.c` - PostgreSQL connection management with connection string validation and automatic migrations.
- `src/db/migration.c` - Database schema migration system with version tracking and directory scanning.
- `src/db/session.c` - Session management for creating and querying conversation sessions.
- `src/db/message.c` - Message persistence with event kind validation, parameterized queries, and conversation/metadata filtering.
- `src/db/agent.c` - Agent persistence with insert/update/list operations and status tracking.
- `src/db/agent_replay.c` - Agent replay helpers for finding clear events and loading conversation history with mark boundaries.
- `src/db/mail.c` - Mail message persistence with insert/query/update operations for inter-agent messaging.
- `src/db/replay.c` - Replay context for loading and replaying conversation history with mark stack, message array, and rewind support.
- `src/db/pg_result.c` - PGresult wrapper with automatic cleanup using talloc destructors for RAII-style resource management.

## OpenAI Client

- `src/openai/client.c` - OpenAI API client with conversation management, message serialization, and streaming request execution.
- `src/openai/client_msg.c` - OpenAI message creation utilities for user/assistant/tool messages with data_json handling.
- `src/openai/client_serialize.c` - Message serialization helpers for transforming canonical format (tool_call/tool_result) to OpenAI wire format.
- `src/openai/client_multi.c` - Multi-handle client core implementation for concurrent HTTP requests with lifecycle management and event loop.
- `src/openai/client_multi_request.c` - Request management for adding new requests to the multi-handle manager with JSON serialization.
- `src/openai/client_multi_callbacks.c` - HTTP callback handlers for extracting metadata (model, usage) from SSE events.
- `src/openai/http_handler.c` - Low-level HTTP client functionality using libcurl with SSE streaming, finish reason extraction, and tool call detection.
- `src/openai/sse_parser.c` - Server-Sent Events parser for streaming HTTP responses with accumulation buffer and event extraction.
- `src/openai/tool_choice.c` - Tool choice implementation (auto/none/required/specific) for controlling OpenAI tool invocation modes.

## Tool System

- `src/tool.c` - Tool call data structures and JSON schema generation for OpenAI function definitions with parameter helpers.
- `src/tool_dispatcher.c` - Tool dispatcher that routes tool calls to appropriate handlers with JSON validation and error envelope building.
- `src/tool_arg_parser.c` - Tool argument parsing utilities for extracting string/boolean/integer parameters from JSON.
- `src/tool_response.c` - Tool response building helpers for success/error envelopes with custom data callbacks.
- `src/tool_bash.c` - Bash command execution tool using popen with output capture and exit code extraction.
- `src/tool_file_read.c` - File reading tool with error handling for missing/inaccessible files and size limits.
- `src/tool_file_write.c` - File writing tool with error handling for permission/space issues and byte counting.
- `src/tool_glob.c` - File pattern matching tool using glob() with JSON result formatting and count tracking.
- `src/tool_grep.c` - Pattern search tool using regex with file filtering, line matching, and result formatting.

## Utilities

- `src/format.c` - Format buffer implementation for building strings with printf-style formatting and JSON appending.
- `src/pp_helpers.c` - Pretty-print helpers for debugging data structures with indentation and type formatting.
- `src/fzy_wrapper.c` - Wrapper for fzy fuzzy matching library used in tab completion with scoring and prefix matching.
- `src/debug_pipe.c` - Debug output pipe system for capturing tool output in separate channels with non-blocking reads.
- `src/msg.c` - Canonical message format utilities for distinguishing conversation kinds from metadata events.
- `src/file_utils.c` - File I/O utilities for reading entire files with error handling and size limits.

## Mail System

- `src/mail/msg.c` - Mail message structure creation with timestamp generation and field initialization.

## Vendor Libraries

- `src/vendor/yyjson/yyjson.c` - High-performance JSON library for parsing and generation (vendored).
- `src/vendor/fzy/match.c` - Fuzzy string matching algorithm from the fzy project with bonus scoring (vendored).
