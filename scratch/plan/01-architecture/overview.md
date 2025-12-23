# Provider Abstraction Architecture

## Overview

The multi-provider architecture uses a vtable pattern to abstract AI provider APIs behind a unified interface. Each provider implements the same vtable, enabling polymorphic behavior without tight coupling.

**Key architectural principle:** All provider operations are async/non-blocking to integrate with the REPL's select()-based event loop.

## Directory Structure

```
src/
  providers/
    common/
      http_multi.c         # curl_multi wrapper (async HTTP)
      http_multi.h
      sse_parser.c         # Shared SSE streaming parser
      sse_parser.h
    provider.h             # Vtable definition, shared types
    provider_common.c      # Utility functions

    anthropic/
      adapter.c            # Vtable implementation (async)
      client.c             # Request serialization
      streaming.c          # SSE event handlers
      anthropic.h          # Public interface

    openai/
      adapter.c            # Vtable implementation (async)
      client.c             # Request serialization
      streaming.c          # SSE event handlers
      openai.h

    google/
      adapter.c            # Vtable implementation (async)
      client.c             # Request serialization
      streaming.c          # SSE event handlers
      google.h
```

## Modules and Responsibilities

### provider.h - Vtable Interface

**Purpose:** Defines the common async interface all providers must implement.

**Key types:**
- `ik_provider_vtable_t` - Function pointer table with async operations (fdset, perform, start_request, start_stream)
- `ik_provider_t` - Provider handle containing name, vtable pointer, and implementation context

**Responsibilities:**
- Define unified async interface for sending requests (streaming and non-streaming)
- Establish contract for event loop integration (fdset, perform, timeout)
- Enable polymorphic dispatch to provider-specific implementations
- **All operations are non-blocking**

### common/http_multi - Async HTTP Layer

**Purpose:** Provides reusable async HTTP functionality using curl_multi for all providers.

**Key types:**
- `ik_http_multi_t` - curl_multi wrapper with active request tracking

**Key functions:**
- `ik_http_multi_create` - Initialize curl_multi handle
- `ik_http_multi_fdset` - Get file descriptors for select()
- `ik_http_multi_perform` - Process pending I/O (non-blocking)
- `ik_http_multi_timeout` - Get recommended select() timeout
- `ik_http_multi_info_read` - Check for completed transfers
- `ik_http_multi_add_request` - Add async request to multi handle

**Responsibilities:**
- Manage curl_multi handle and active transfers
- Provide fdset/perform pattern for select() integration
- Handle request headers and body construction
- Process HTTP status codes and network errors via callbacks
- **NEVER block - all I/O is non-blocking**

### common/sse_parser - SSE Stream Parser

**Purpose:** Parse Server-Sent Events streams from provider APIs.

**Key types:**
- `ik_sse_parser_t` - Parser state machine
- `ik_sse_callback_t` - Callback function type for parsed events

**Key functions:**

```c
/**
 * Parse SSE chunk, invoke callback for each complete event
 *
 * @param parser Parser state machine
 * @param data   Input data chunk (may contain partial events)
 * @param len    Length of data in bytes
 * @return       Number of events parsed, or -1 on error
 *
 * This function processes incoming SSE data incrementally:
 * - Handles partial events across multiple chunks
 * - Maintains internal buffer for incomplete events
 * - Invokes parser's callback for each complete event found
 * - Supports SSE protocol fields: event:, data:, id:, retry:
 * - Handles empty lines as event delimiters
 *
 * Call this from curl write callback as data arrives during perform().
 */
int32_t ik_sse_parse_chunk(ik_sse_parser_t *parser, const char *data, size_t len);
```

**Responsibilities:**
- Parse SSE protocol (event:, data:, id: fields)
- Handle partial chunks and buffering
- Invoke callbacks for complete events
- Maintain parser state across multiple chunks

### anthropic/ - Anthropic Provider

**Modules:**
- `adapter.c` - Implements vtable interface for Anthropic API
- `client.c` - Request construction and response parsing
- `streaming.c` - SSE event handler for Anthropic streaming format

**Key types:**
- `ik_anthropic_ctx_t` - Provider-specific context holding API key and HTTP client

**Responsibilities:**
- Translate `ik_request_t` to Anthropic Messages API format
- Parse Anthropic responses into `ik_response_t`
- Handle Anthropic-specific streaming events (content_block_delta, etc.)
- Manage extended thinking feature integration

### openai/ - OpenAI Provider

**Modules:**
- `adapter.c` - Implements vtable interface for OpenAI API
- `client.c` - Request construction and response parsing
- `streaming.c` - SSE event handler for OpenAI streaming format

**Key types:**
- `ik_openai_ctx_t` - Provider-specific context holding API key and HTTP client

**Responsibilities:**
- Translate `ik_request_t` to OpenAI Chat Completions API format
- Parse OpenAI responses into `ik_response_t`
- Handle OpenAI-specific streaming events (chat.completion.chunk)
- Support o1 reasoning tokens through extended thinking abstraction

### google/ - Google Provider

**Modules:**
- `adapter.c` - Implements vtable interface for Google Gemini API
- `client.c` - Request construction and response parsing
- `streaming.c` - SSE event handler for Google streaming format

**Key types:**
- `ik_google_ctx_t` - Provider-specific context holding API key and HTTP client

**Responsibilities:**
- Translate `ik_request_t` to Google Gemini API format
- Parse Google responses into `ik_response_t`
- Handle Google-specific streaming events
- Map thinking levels to Gemini model variants

## Data Flow

### Provider Initialization

1. Agent sets provider name (e.g., "anthropic")
2. First message send triggers lazy initialization via `ik_provider_get_or_create`
3. Credentials loaded from environment variable or credentials.json
4. Provider-specific factory function creates implementation context
5. **curl_multi handle initialized** for async HTTP
6. Vtable populated with provider-specific function pointers
7. Provider handle cached in agent context

### Event Loop Integration (CRITICAL)

The REPL main loop integrates with providers via select():

```c
// Simplified event loop (see src/repl.c for full implementation)
while (!quit) {
    fd_set read_fds, write_fds, exc_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);
    int max_fd = 0;

    // Add terminal input FD
    FD_SET(tty_fd, &read_fds);
    max_fd = tty_fd;

    // Add provider FDs for all active agents
    for (each agent) {
        if (agent->provider) {
            agent->provider->vt->fdset(agent->provider->ctx,
                                       &read_fds, &write_fds, &exc_fds, &max_fd);
        }
    }

    // Calculate timeout (minimum of all providers)
    long timeout_ms = DEFAULT_TIMEOUT;
    for (each agent) {
        if (agent->provider) {
            long provider_timeout;
            agent->provider->vt->timeout(agent->provider->ctx, &provider_timeout);
            if (provider_timeout >= 0 && provider_timeout < timeout_ms) {
                timeout_ms = provider_timeout;
            }
        }
    }

    // Wait for activity
    select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);

    // Process provider I/O (non-blocking)
    for (each agent) {
        if (agent->provider) {
            int running;
            agent->provider->vt->perform(agent->provider->ctx, &running);
            agent->provider->vt->info_read(agent->provider->ctx, logger);
        }
    }

    // Handle terminal input
    if (FD_ISSET(tty_fd, &read_fds)) {
        handle_keyboard_input();
    }
}
```

### Non-Streaming Request Flow (Async)

1. Caller creates `ik_request_t` with messages and configuration
2. Call `provider->vt->start_request(ctx, req, completion_cb, user_ctx)`
3. Provider translates request to native API format (JSON)
4. Provider configures curl easy handle and adds to multi handle
5. **Function returns immediately (non-blocking)**
6. Event loop calls `perform()` when select() indicates activity
7. When transfer completes, `info_read()` invokes completion_cb
8. Completion callback receives `ik_provider_completion_t` with response data

### Streaming Request Flow (Async)

1. Caller creates `ik_request_t` and provides stream + completion callbacks
2. Call `provider->vt->start_stream(ctx, req, stream_cb, stream_ctx, completion_cb, completion_ctx)`
3. Provider translates request to native API format with stream flag
4. Provider configures curl easy handle with write callback
5. **Function returns immediately (non-blocking)**
6. Event loop calls `perform()` when select() indicates activity
7. As data arrives, curl write callback invokes SSE parser
8. SSE parser invokes provider's event handler
9. Provider translates events to `ik_stream_event_t` and calls stream_cb
10. stream_cb updates UI (scrollback, spinner, etc.)
11. When transfer completes, `info_read()` invokes completion_cb

### Cleanup Flow

1. Provider freed via talloc hierarchy
2. Talloc destructor invokes vtable cleanup function
3. Active transfers removed from curl_multi
4. curl_multi handle cleaned up
5. API keys and buffers freed via talloc parent-child relationships

## Integration Points

### REPL Integration

The REPL layer becomes provider-agnostic by routing through the vtable interface instead of directly calling OpenAI functions. The agent context holds the active provider handle, and message sending operations dispatch through `provider->vt->start_stream`.

**Key REPL changes for async providers:**

1. **Setup FD sets**: Before calling select(), REPL calls `provider->vt->fdset()` for each active agent
2. **Calculate timeout**: REPL calls `provider->vt->timeout()` and uses minimum across all agents
3. **Process I/O**: After select(), REPL calls `provider->vt->perform()` and `info_read()` for each agent
4. **Handle callbacks**: Stream callbacks update scrollback; completion callbacks finalize response

The existing pattern in `src/repl.c` (calling `ik_openai_multi_*` functions) becomes the generic pattern for all providers.

### Agent Context Extension

Agent structure extended with:
- `provider` - String identifying provider (e.g., "anthropic")
- `model` - Model name within provider's namespace
- `thinking_level` - Normalized thinking level (NONE/LOW/MED/HIGH)
- `provider_ctx` - Cached provider handle (NULL until first use)

### Database Schema Changes

The agents table requires new columns:
- `provider` (TEXT) - Provider name, defaults to "openai"
- `model` (TEXT) - Model identifier
- `thinking_level` (TEXT) - Thinking level ("none", "low", "med", "high")

Migration adds these columns with appropriate defaults for existing rows.

### Command Updates

The `/model` command updated to:
- Parse model strings with optional provider prefix (e.g., "claude-sonnet-4-5")
- Infer provider from model name when not explicitly specified
- Extract thinking level suffix (e.g., "/med")
- Update agent's provider, model, and thinking_level fields
- Trigger provider re-initialization on next message send

### Configuration System

New credentials loading:
- Check environment variable `{PROVIDER}_API_KEY` (e.g., `ANTHROPIC_API_KEY`)
- Fall back to `credentials.json` file in config directory
- Format: `{"anthropic": {"api_key": "sk-ant-..."}, "openai": {"api_key": "sk-..."}}`
- Return auth error with setup instructions if credentials missing

## Migration from Existing OpenAI Code

### Current State (rel-06)

```
src/openai/                    # Hardcoded implementation (ALREADY ASYNC)
  client.c                     # Direct calls from REPL
  client_multi.c               # curl_multi async HTTP (REFERENCE)
  client_multi_callbacks.c     # Completion callbacks
  client_multi_request.c       # Request building
  client_msg.c
  client_serialize.c
  http_handler.c
  sse_parser.c                 # SSE parsing (reusable)
  tool_choice.c

src/client.c                   # Main entry point (REPL application) - STAYS
```

**Key insight:** The existing `src/openai/client_multi.c` already implements the correct async pattern. New providers must follow this pattern:
- `ik_openai_multi_fdset()` - exposes FDs for select()
- `ik_openai_multi_perform()` - non-blocking I/O
- `ik_openai_multi_timeout()` - timeout for select()
- `ik_openai_multi_info_read()` - completion handling
- `ik_openai_multi_add_request()` - non-blocking request start

**Call sites with hardcoded OpenAI dependencies:**
- `src/commands_basic.c` - Hardcoded model list in `/model` command
- `src/completion.c` - Model autocomplete using fixed OpenAI models
- `src/repl_actions_llm.c` - Direct dispatch to OpenAI client
- `src/agent.c` - No provider field
- `src/db/agent.c` - No provider column
- `src/config.c` - No credentials loading

### Target State (rel-07)

```
src/providers/
  common/
    http_multi.c        # Refactored curl_multi layer (from client_multi.c)
    http_multi.h
    sse_parser.c        # Extracted SSE parser
    sse_parser.h
  provider.h            # Vtable interface (async methods)
  openai/
    adapter.c           # Vtable implementation (delegates to http_multi)
    client.c            # Request serialization
    streaming.c         # SSE event handling
    openai.h

src/client.c            # Main entry point - UPDATED to use provider abstraction
src/openai/             # DELETED completely (moved to src/providers/openai/)
```

**Emphasis:** The old `src/openai/` directory is COMPLETELY DELETED, not refactored in place. This is a clean replacement.

### Migration Strategy: Adapter-First

**Approach:** Maintain OpenAI functionality throughout migration by creating adapter shim, then refactor incrementally.

**Phase Mapping:**

The 8 implementation steps below map to the high-level phases in [README.md](../README.md):

| Phase | Steps | Description |
|-------|-------|-------------|
| Phase 1: Coexistence | Steps 1-7 | Build new provider abstraction alongside existing code |
| Phase 2: Removal | Step 8 | Delete old code after verification (see README.md prerequisites) |

**Phases:**

1. **Vtable Foundation** - Create `src/providers/provider.h` with async interface definitions
   - Include fdset(), perform(), timeout(), info_read() methods
   - Include start_request(), start_stream() for non-blocking request initiation
2. **Common HTTP Layer** - Extract `src/openai/client_multi.c` to `src/providers/common/http_multi.c`
   - Generalize for any provider (remove OpenAI-specific code)
   - Keep fdset/perform/timeout/info_read pattern
3. **Adapter Shim** - Wrap existing `src/openai/` with vtable adapter to validate interface
   - Adapter delegates to existing client_multi functions
4. **Call Site Updates (Incremental)** - Break the transition into testable sub-steps

   **4a: Add Provider Fields to Agent**
   - Add `provider`, `model`, `thinking_level` fields to `ik_agent_ctx_t`
   - Initialize to NULL/defaults in agent creation
   - Test: Agent creates successfully, fields are NULL
   - Files: `src/agent.c`, `src/agent.h`

   **4b: Provider Dispatch Layer**
   - Create `src/providers/dispatch.c` with `ik_provider_send()`
   - This function checks agent->provider, creates if needed, calls vtable
   - Initially just wraps old OpenAI code path
   - Test: Message send still works through dispatch layer
   - Files: `src/providers/dispatch.c`, `src/providers/dispatch.h`

   **4c: Update REPL to Use Dispatch**
   - Change `src/repl_actions_llm.c` to call `ik_provider_send()` instead of direct OpenAI
   - Old OpenAI path still works via dispatch wrapper
   - Test: REPL sends messages successfully
   - Files: `src/repl_actions_llm.c`

   **4d: Wire Up /model Command**
   - Update `/model` to set agent->provider, agent->model, agent->thinking_level
   - Dispatch layer now routes to correct provider based on fields
   - Test: `/model gpt-4o` then send message works
   - Files: `src/commands.c` or `src/commands_basic.c`

   **4e: Update Tab Completion**
   - Add model names to completion provider
   - Test: Tab completion shows model options
   - Files: `src/completion.c`

   Each sub-step is independently testable. If any fails, previous steps still work.
5. **Anthropic Provider** - Add second provider using same async pattern
   - Uses common http_multi layer
   - Implements provider-specific serialization and SSE handling
6. **Google Provider** - Add third provider for additional validation
7. **OpenAI Refactor** - Move OpenAI to native vtable implementation in `src/providers/openai/`
8. **Cleanup** - Delete adapter shim and old `src/openai/` directory

### Makefile Migration

The Makefile must be updated incrementally during migration to maintain a working build at each step.

**Step 3 (Adapter Shim):** Add new source paths
```makefile
# Add to SRC list
SRC += src/providers/provider_common.c
SRC += src/providers/common/http_multi.c
SRC += src/providers/common/sse_parser.c
```

**Step 5-6 (New Providers):** Add provider implementations
```makefile
# Add as each provider is implemented
SRC += src/providers/anthropic/adapter.c
SRC += src/providers/anthropic/client.c
SRC += src/providers/anthropic/streaming.c
# ... repeat for openai/, google/
```

**Step 7 (OpenAI Refactor):** Both old and new OpenAI paths exist temporarily
```makefile
# Old paths still present (will be removed in Step 8)
SRC += src/openai/client.c
SRC += src/openai/client_multi.c
# ...

# New paths added
SRC += src/providers/openai/adapter.c
SRC += src/providers/openai/client.c
SRC += src/providers/openai/streaming.c
```

**Step 8 (Cleanup):** Remove old paths BEFORE deleting files
```makefile
# Remove these lines from Makefile:
# SRC += src/openai/client.c
# SRC += src/openai/client_multi.c
# SRC += src/openai/client_multi_callbacks.c
# SRC += src/openai/client_multi_request.c
# SRC += src/openai/client_msg.c
# SRC += src/openai/client_serialize.c
# SRC += src/openai/http_handler.c
# SRC += src/openai/sse_parser.c
# SRC += src/openai/tool_choice.c
```

**Test target updates:** Similarly update test paths:
- Add `tests/unit/providers/` tests as providers are implemented
- Remove `tests/unit/openai/` references in Step 8

**Critical:** Always run `make clean && make all && make check` after Makefile changes to verify build integrity.

### Critical Cleanup Requirements

After migration completes:

### Files and Directories to Delete

**Phase 2 deletion targets (execute only after all prerequisites verified):**

```
# Old OpenAI implementation (replaced by src/providers/openai/)
rm -rf src/openai/

# Old test files for OpenAI client
rm -rf tests/unit/openai/
rm -rf tests/integration/openai/

# Old fixtures (replaced by VCR cassettes in tests/fixtures/vcr/)
rm -rf tests/fixtures/openai/
rm -rf tests/fixtures/responses/

# Any mock files specific to old implementation
rm -f tests/mocks/openai_*.c
```

**Verification before deletion:**
```bash
# Ensure no remaining references to old paths
grep -r "src/openai/" src/ --include="*.c" --include="*.h"
grep -r "openai/client" src/ --include="*.c" --include="*.h"
# Should return empty or only src/providers/openai/ paths
```

**Note:** The Makefile must be updated BEFORE deletion to remove references to these files, otherwise the build will fail.

**Additional cleanup requirements:**
- Old `src/openai/` directory must be completely deleted
- Old tests and fixtures are also completely deleted - not migrated or converted
- Adapter shim code removed
- No `#include` statements referencing old paths outside `src/providers/`
- No direct function calls to provider implementations outside vtable dispatch
- Makefile updated to remove references to deleted files
- Full test suite passing with new structure (VCR-based tests only)

### src/client.c Integration

The `src/client.c` file contains the main() entry point for the REPL application. It will be UPDATED (not removed) to use the new provider abstraction when calling agent operations. No structural changes are needed - only the agent context will gain provider fields that client.c passes through.

## Error Handling

Providers return errors through standard `res_t` mechanism with appropriate error codes:

- `IK_ERR_CAT_AUTH` - Invalid or missing API key (401 status)
- `IK_ERR_CAT_RATE_LIMIT` - Rate limit exceeded (429 status)
- `IK_ERR_CAT_NETWORK` - Connection failures, timeouts
- `IK_ERR_CAT_INVALID_ARG` - Bad request / validation error (400)
- `IK_ERR_CAT_SERVER` - Server error (500, 502, 503)

Error messages include actionable guidance such as credential setup URLs and retry timing.

## Testing Strategy

Each provider has dedicated test suites:

**Unit tests:**
- `tests/unit/providers/test_anthropic_adapter.c` - Vtable implementation
- `tests/unit/providers/test_anthropic_client.c` - Request/response handling
- `tests/unit/providers/test_openai_adapter.c`
- `tests/unit/providers/test_google_adapter.c`
- `tests/unit/common/test_http_multi.c` - Shared HTTP layer
- `tests/unit/common/test_sse_parser.c` - SSE parsing

**Integration tests:**
- Test actual provider APIs with real credentials (optional)
- Mock HTTP responses for deterministic testing
- Validate error handling for various failure modes
- Verify streaming event sequences
- Test provider switching within single agent session

Mock responses use existing `MOCKABLE` pattern to simulate provider responses without network calls.
