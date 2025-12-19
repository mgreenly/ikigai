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
- `src/layer_separator.c` - Separator layer wrapper that renders horizontal separators with optional debug info.
- `src/layer_spinner.c` - Spinner layer wrapper for animated loading indicators.
- `src/layer_completion.c` - Completion layer wrapper that renders tab completion suggestions.
- `src/event_render.c` - Universal event renderer that converts database events to styled scrollback content.

## Scrollback Buffer

- `src/scrollback.c` - Scrollback buffer implementation with line wrapping and layout caching.
- `src/scrollback_render.c` - Scrollback rendering helper functions for calculating display positions.
- `src/scrollback_utils.c` - Utility functions for scrollback text analysis including display width calculation.
- `src/scroll_detector.c` - Distinguishes mouse wheel scrolling from keyboard arrow key presses using timing.

## Input System

- `src/input.c` - Input parser that converts raw bytes to semantic actions with UTF-8 and escape sequence handling.
- `src/input_escape.c` - Escape sequence parsing for terminal control codes.
- `src/input_xkb.c` - XKB keyboard layout support for translating shifted keys to their base characters.

## Input Buffer

- `src/input_buffer/core.c` - Input buffer text storage implementation with UTF-8 support.
- `src/input_buffer/cursor.c` - Cursor position tracking with byte and grapheme offset management.
- `src/input_buffer/cursor_pp.c` - Cursor pretty-print implementation for debugging.
- `src/input_buffer/layout.c` - Input buffer layout caching for efficient display width calculation.
- `src/input_buffer/multiline.c` - Multi-line navigation implementation for up/down arrow keys.
- `src/input_buffer/pp.c` - Input buffer pretty-print implementation for debugging.

## REPL Core

- `src/repl.c` - REPL main event loop with select()-based multiplexing for input, HTTP, and tool execution.
- `src/repl_init.c` - REPL initialization and cleanup with session restoration support.
- `src/repl_viewport.c` - Viewport calculation logic for determining what's visible on screen.
- `src/repl_callbacks.c` - HTTP callback handlers for streaming OpenAI responses.
- `src/repl_event_handlers.c` - Event handlers for stdin, HTTP completion, tool completion, and timeouts.
- `src/repl_tool.c` - Tool execution helper that runs tools in background threads.

## REPL Actions

- `src/repl_actions.c` - Core action processing including arrow key handling through scroll detector.
- `src/repl_actions_llm.c` - LLM and slash command handling with conversation management.
- `src/repl_actions_viewport.c` - Viewport and scrolling actions (page up/down, scroll up/down).
- `src/repl_actions_history.c` - History navigation actions (Ctrl+P/N for previous/next).
- `src/repl_actions_completion.c` - Tab completion functionality for slash commands.

## Agent Management

- `src/agent.c` - Agent context creation and lifecycle management with layer system and conversation state.
- `src/repl/session_restore.c` - Session restoration logic for continuous sessions using database replay.

## Commands

- `src/commands.c` - REPL command registry and dispatcher for slash commands (/clear, /help, /model, etc).
- `src/commands_mark.c` - Mark and rewind command implementations for conversation checkpoints.
- `src/marks.c` - Mark creation and management with ISO 8601 timestamps.
- `src/completion.c` - Tab completion data structures and fuzzy matching logic.

## History

- `src/history.c` - Command history management with persistence to JSON file and capacity limits.

## Database Layer

- `src/db/connection.c` - PostgreSQL connection management with connection string validation and migrations.
- `src/db/migration.c` - Database schema migration system with version tracking.
- `src/db/session.c` - Session management for creating and querying conversation sessions.
- `src/db/message.c` - Message persistence with event kind validation and parameterized queries.
- `src/db/pg_result.c` - PGresult wrapper with automatic cleanup using talloc destructors.
- `src/db/replay.c` - Replay context for loading and replaying conversation history with mark stack support.

## OpenAI Client

- `src/openai/client.c` - OpenAI API client with conversation management and message serialization.
- `src/openai/client_msg.c` - OpenAI message creation utilities for user/assistant/tool messages.
- `src/openai/client_serialize.c` - Message serialization helpers for transforming canonical format to OpenAI wire format.
- `src/openai/client_multi.c` - Multi-handle client core implementation for concurrent HTTP requests.
- `src/openai/client_multi_request.c` - Request management for adding new requests to the multi-handle manager.
- `src/openai/client_multi_callbacks.c` - HTTP callback handlers for extracting metadata from SSE events.
- `src/openai/http_handler.c` - Low-level HTTP client functionality using libcurl with SSE streaming support.
- `src/openai/sse_parser.c` - Server-Sent Events parser for streaming HTTP responses.
- `src/openai/tool_choice.c` - Tool choice implementation for controlling OpenAI tool invocation modes.

## Tool System

- `src/tool.c` - Tool call data structures and JSON schema generation for OpenAI function definitions.
- `src/tool_dispatcher.c` - Tool dispatcher that routes tool calls to appropriate handlers with JSON validation.
- `src/tool_arg_parser.c` - Tool argument parsing utilities for extracting parameters from JSON.
- `src/tool_bash.c` - Bash command execution tool using popen with output capture.
- `src/tool_file_read.c` - File reading tool with error handling for missing/inaccessible files.
- `src/tool_file_write.c` - File writing tool with error handling for permission/space issues.
- `src/tool_glob.c` - File pattern matching tool using glob() with JSON result formatting.
- `src/tool_grep.c` - Pattern search tool using regex with file filtering and match counting.

## Utilities

- `src/format.c` - Format buffer implementation for building strings with printf-style formatting.
- `src/pp_helpers.c` - Pretty-print helpers for debugging data structures with indentation.
- `src/fzy_wrapper.c` - Wrapper for fzy fuzzy matching library used in tab completion.
- `src/debug_pipe.c` - Debug output pipe system for capturing tool output in separate channels.
- `src/msg.c` - Canonical message format conversion from database to OpenAI API format.

## Vendor Libraries

- `src/vendor/yyjson/yyjson.c` - High-performance JSON library for parsing and generation.
- `src/vendor/fzy/match.c` - Fuzzy string matching algorithm from the fzy project.
