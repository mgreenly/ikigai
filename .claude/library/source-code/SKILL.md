---
name: source-code
description: Source Code skill for the ikigai project
---

# Source Code

Reference for all C source files organized by functional area.

## Core Infrastructure

- `src/client.c` - Main entry point; initializes RNG, logger, config, starts event loop.
- `src/shared.c` - Shared context init: terminal, rendering, database, history, credentials.
- `src/panic.c` - Panic handler with async-signal-safe cleanup and terminal restoration.
- `src/error.c` - Error handling wrapper using talloc-based allocator.
- `src/config.c` - Config file loading with tilde expansion and default creation.
- `src/logger.c` - Thread-safe logging: ISO 8601 timestamps, local TZ, JSON output.
- `src/uuid.c` - UUID generation via base64url (22-char agent identifiers).
- `src/credentials.c` - API key loading from environment or credentials file.

## Memory Management

- `src/array.c` - Generic expandable array with configurable element size, geometric growth.
- `src/byte_array.c` - Typed wrapper (uint8_t) over generic array.
- `src/line_array.c` - Typed wrapper (char*) over generic array.
- `src/json_allocator.c` - Talloc-based yyjson allocator for consistent ownership.

## Terminal Management

- `src/terminal.c` - Raw mode, alternate screen, CSI u detection, size queries.
- `src/signal_handler.c` - SIGWINCH handling with global flag coordination.
- `src/ansi.c` - ANSI escape parsing, 256-color generation, CSI skip.

## Rendering System

- `src/render.c` - Direct ANSI rendering context with dimension tracking.
- `src/render_cursor.c` - Cursor position calculation: UTF-8 widths, ANSI escapes, wrapping.
- `src/layer.c` - Output buffer for composable rendering layers with auto-growth.
- `src/layer_scrollback.c` - Scrollback layer with layout caching.
- `src/layer_input.c` - Input buffer layer with line wrapping.
- `src/layer_separator.c` - Horizontal separators with debug info, navigation arrows.
- `src/layer_spinner.c` - Animated loading indicator (4-frame cycle).
- `src/layer_completion.c` - Tab completion suggestions with highlighting.
- `src/event_render.c` - Converts DB events to styled scrollback content.

## Scrollback Buffer

- `src/scrollback.c` - Contiguous text storage with line wrapping, layout caching.
- `src/scrollback_layout.c` - Layout calculation: segment widths, newline handling.
- `src/scrollback_render.c` - Display positions, byte offsets, partial row rendering.
- `src/scrollback_utils.c` - Display width calculation with ANSI escape handling.
- `src/scroll_detector.c` - Wheel vs keyboard detection via timing-based state machine.

## Input System

- `src/input.c` - Byte-to-action parser: UTF-8 decoding, xkbcommon integration.
- `src/input_escape.c` - Escape/CSI sequence parsing, discardable sequence detection.
- `src/input_xkb.c` - XKB layout: shifted key to base char via reverse keycode mapping.

## Input Buffer

- `src/input_buffer/core.c` - Text storage with UTF-8, cursor management, lazy layout cache.
- `src/input_buffer/cursor.c` - Byte/grapheme offset tracking via utf8proc grapheme breaks.
- `src/input_buffer/cursor_pp.c` - Debug pretty-print for cursor state.
- `src/input_buffer/pp.c` - Debug pretty-print for input buffer.
- `src/input_buffer/layout.c` - Layout caching for display width with ANSI handling.
- `src/input_buffer/multiline.c` - Up/down navigation: line boundaries, target column.

## REPL Core

- `src/repl.c` - Main event loop: select() multiplexing for input, HTTP, tools.
- `src/repl_init.c` - Init/cleanup: session restore, signal setup, agent tree rebuild.
- `src/repl_viewport.c` - Viewport calculation: component sizing, scroll offset.
- `src/repl_callbacks.c` - Stream callbacks: normalized events, UI updates, response accumulation.
- `src/repl_event_handlers.c` - Handlers: stdin, HTTP completion, tool completion, timeouts.
- `src/repl_tool.c` - Background thread tool execution with mutex-protected results.
- `src/repl_tool_completion.c` - Tool completion handling and tool loop continuation.
- `src/repl_navigation.c` - Agent tree navigation: parent/child/sibling switching.
- `src/repl_agent_mgmt.c` - Agent array: add/remove with dynamic capacity growth.

## REPL Actions

- `src/repl_actions.c` - Core actions: arrow keys via scroll detector, multiline append.
- `src/repl_actions_llm.c` - LLM/slash commands: conversation mgmt, command dispatch, tool loop.
- `src/repl_actions_viewport.c` - Scrolling: page up/down, mouse scroll, max offset.
- `src/repl_actions_history.c` - History nav (Ctrl+P/N) with pending entry preservation.
- `src/repl_actions_completion.c` - Tab completion for slash commands, auto-update on insert.

## Agent Management

- `src/agent.c` - Agent lifecycle: layers, conversation state, mutex, message handling.
- `src/agent_messages.c` - Agent message array management: add, clear, clone content blocks.
- `src/agent_provider.c` - Provider initialization: apply config defaults, thinking level parsing.
- `src/agent_state.c` - Agent state transitions: idle, waiting for LLM, executing tool.
- `src/repl/agent_restore.c` - Startup restore: replay, mark stack, sorted tree building.
- `src/repl/agent_restore_replay.c` - Replay helpers: conversation arrays, command effects.

## Commands

- `src/commands.c` - Command registry/dispatcher: command table, help generation.
- `src/commands_basic.c` - /clear, /help, /system, /debug commands.
- `src/commands_model.c` - /model command: change model with thinking level override.
- `src/commands_mark.c` - /mark, /rewind: conversation checkpoints, DB truncation.
- `src/commands_fork.c` - /fork: child agents with quoted prompts, model override.
- `src/commands_fork_args.c` - Fork argument parsing: model, prompt extraction.
- `src/commands_fork_helpers.c` - Fork helpers: name generation, display, navigation.
- `src/commands_kill.c` - /kill: terminate agents/descendants, cascade cleanup.
- `src/commands_mail.c` - /send, /inbox, /read, /delete, /filter: inter-agent messaging.
- `src/commands_mail_helpers.c` - Mail helpers: timestamp formatting, inbox rendering.
- `src/commands_agent_list.c` - /agents: hierarchy tree with indentation, status.
- `src/marks.c` - Mark creation: ISO 8601 timestamps, scrollback, DB persistence.
- `src/completion.c` - Tab completion: fuzzy matching, model/agent/marks providers.

## History

- `src/history.c` - Command history: capacity limits, dedup, entry reordering.
- `src/history_io.c` - JSONL persistence: directory creation, load/save.

## Message System

- `src/message.c` - Message structures: text, tool call, tool result with content blocks.
- `src/msg.c` - Canonical format: distinguish conversation kinds from metadata events.

## Database Layer

- `src/db/connection.c` - PG connection: validation, auto-migrations, talloc destructor.
- `src/db/migration.c` - Schema migrations: version tracking, directory scan, transactions.
- `src/db/session.c` - Session mgmt: create/query with auto-increment IDs.
- `src/db/message.c` - Message persistence: event kind validation, parameterized queries.
- `src/db/agent.c` - Agent CRUD: insert/update/list/find, status tracking.
- `src/db/agent_row.c` - PGresult parsing: field extraction, thinking level.
- `src/db/agent_zero.c` - Root agent mgmt: creation, lookup for fresh sessions.
- `src/db/agent_replay.c` - Replay helpers: clear events, history with ID boundaries.
- `src/db/mail.c` - Mail CRUD for inter-agent messaging.
- `src/db/replay.c` - Replay context: load history, mark stack, geometric growth.
- `src/db/pg_result.c` - PGresult wrapper: auto PQclear via talloc destructor (RAII).

## Provider System (Common)

- `src/providers/factory.c` - Provider factory: create by name with API key lookup.
- `src/providers/provider.c` - Model capability table: prefix to provider/thinking mapping.
- `src/providers/request.c` - Request builder: content blocks, tools, messages, thinking.
- `src/providers/request_tools.c` - Standard tool definitions and request building from agent conversation.
- `src/providers/response.c` - Response builder for ik_response_t.
- `src/providers/stubs.c` - Stub provider factories for incremental development.
- `src/providers/common/http_multi.c` - Shared async HTTP via curl multi-handle.
- `src/providers/common/http_multi_info.c` - HTTP completion info processing from curl multi.
- `src/providers/common/error_utils.c` - Error categories: retryable classification.
- `src/providers/common/sse_parser.c` - SSE parser: streaming HTTP, buffer accumulation.

## Anthropic Provider

- `src/providers/anthropic/anthropic.c` - Provider vtable: async HTTP, streaming.
- `src/providers/anthropic/error.c` - HTTP status mapping, JSON error extraction.
- `src/providers/anthropic/request.c` - Canonical to Messages API JSON.
- `src/providers/anthropic/request_serialize.c` - Content block serialization for Messages API.
- `src/providers/anthropic/response.c` - Parse content blocks, usage, finish reasons.
- `src/providers/anthropic/response_helpers.c` - Response parsing helper functions.
- `src/providers/anthropic/streaming.c` - SSE to stream callbacks.
- `src/providers/anthropic/streaming_events.c` - SSE event processors (message_start, content_block, etc).
- `src/providers/anthropic/thinking.c` - Thinking budget: model limits, level mapping.

## OpenAI Provider

- `src/providers/openai/openai.c` - Provider: dual-API (Chat Completions, Responses API).
- `src/providers/openai/openai_handlers.c` - HTTP completion handlers for non-streaming requests.
- `src/providers/openai/error.c` - HTTP status mapping, content filter detection.
- `src/providers/openai/reasoning.c` - O-series detection, reasoning effort mapping.
- `src/providers/openai/request_chat.c` - Chat Completions request serialization.
- `src/providers/openai/request_responses.c` - Responses API request serialization.
- `src/providers/openai/response_chat.c` - Chat Completions parsing, tool call extraction.
- `src/providers/openai/response_responses.c` - Responses API parsing, reasoning tokens.
- `src/providers/openai/serialize.c` - JSON serialization: role mapping, content handling.
- `src/providers/openai/streaming_chat.c` - Chat delta processing, tool call accumulation.
- `src/providers/openai/streaming_chat_delta.c` - Chat Completions delta event processing.
- `src/providers/openai/streaming_responses.c` - Responses API SSE handling.
- `src/providers/openai/streaming_responses_events.c` - Responses API event processing.

## Google Provider

- `src/providers/google/google.c` - Gemini provider vtable: async HTTP, streaming.
- `src/providers/google/error.c` - HTTP status mapping, JSON error extraction.
- `src/providers/google/request.c` - Canonical to Gemini contents/parts structure.
- `src/providers/google/request_helpers.c` - Content block serialization for Gemini.
- `src/providers/google/response.c` - Response parsing, content parts extraction.
- `src/providers/google/response_error.c` - Error response parsing and categorization.
- `src/providers/google/response_utils.c` - Tool ID generation, finish reason mapping.
- `src/providers/google/streaming.c` - Thinking detection, tool call handling.
- `src/providers/google/thinking.c` - Gemini 2.5 budget limits, 3.x level mapping.

## Tool System

- `src/tool.c` - Tool call structures, JSON schema generation, parameter helpers.
- `src/tool_dispatcher.c` - Route calls to handlers: JSON validation, error envelopes.
- `src/tool_arg_parser.c` - Extract string/boolean/integer params from JSON.
- `src/tool_response.c` - Success/error envelopes with custom data callbacks.
- `src/tool_bash.c` - Bash execution via popen: dynamic output capture, exit codes.
- `src/tool_file_read.c` - File read with errno-based error messages.
- `src/tool_file_write.c` - File write with directory creation, byte counting.
- `src/tool_glob.c` - Pattern matching via glob() with JSON result formatting.
- `src/tool_grep.c` - POSIX regex search: file filtering, line matching, accumulation.

## Utilities

- `src/format.c` - Format buffer: printf-style building, JSON appending.
- `src/pp_helpers.c` - Debug pretty-print: indentation, type headers, field formatting.
- `src/fzy_wrapper.c` - Fzy wrapper: prefix-first filtering, scoring, sorting.
- `src/debug_pipe.c` - Debug output pipe: non-blocking reads, prefix tagging.
- `src/file_utils.c` - File I/O: read entire file, error handling, size validation.

## Mail System

- `src/mail/msg.c` - Mail message creation: timestamp, fields, read status.

## Wrapper Functions (Link Seams)

- `src/wrapper_curl.c` - libcurl wrappers for VCR integration and test mocking.
- `src/wrapper_internal.c` - Internal function wrappers for test mocking.
- `src/wrapper_json.c` - yyjson wrappers for test failure injection.
- `src/wrapper_posix.c` - POSIX system call wrappers (open, close, stat, mkdir, termios).
- `src/wrapper_postgres.c` - PostgreSQL wrappers (PQexec, PQexecParams, PQgetvalue).
- `src/wrapper_pthread.c` - pthread wrappers (mutex, thread create/join).
- `src/wrapper_stdlib.c` - C stdlib wrappers (snprintf, gmtime, strftime).
- `src/wrapper_talloc.c` - talloc wrappers (zero, strdup, array, realloc, asprintf).

## Vendored Libraries

- `src/vendor/fzy/match.c` - Fuzzy matching algorithm from jhawthorn/fzy (MIT licensed).
- `src/vendor/yyjson/yyjson.c` - High-performance JSON library by ibireme (MIT licensed).
