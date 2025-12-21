# Provider Abstraction Architecture

## Overview

The multi-provider architecture uses a vtable pattern to abstract AI provider APIs behind a unified interface. Each provider implements the same vtable, enabling polymorphic behavior without tight coupling.

## Directory Structure

```
src/
  providers/
    common/
      http_client.c        # Shared libcurl wrapper
      http_client.h
      sse_parser.c         # Shared SSE streaming parser
      sse_parser.h
    provider.h             # Vtable definition, shared types
    provider_common.c      # Utility functions

    anthropic/
      adapter.c            # Vtable implementation
      client.c             # API client
      streaming.c          # SSE event handlers
      anthropic.h          # Public interface

    openai/
      adapter.c            # Refactored from src/openai/*
      client.c
      streaming.c
      openai.h

    google/
      adapter.c
      client.c
      streaming.c
      google.h
```

## Modules and Responsibilities

### provider.h - Vtable Interface

**Purpose:** Defines the common interface all providers must implement.

**Key types:**
- `ik_provider_vtable_t` - Function pointer table with send, stream, cleanup operations
- `ik_provider_t` - Provider handle containing name, vtable pointer, and implementation context

**Responsibilities:**
- Define unified interface for sending requests (streaming and non-streaming)
- Establish contract for provider initialization and cleanup
- Enable polymorphic dispatch to provider-specific implementations

### common/http_client - Shared HTTP Layer

**Purpose:** Provides reusable HTTP functionality for all providers.

**Key types:**
- `ik_http_client_t` - HTTP client handle with base URL and connection pooling

**Key functions:**
- `ik_http_client_create` - Initialize HTTP client with base URL
- `ik_http_post` - Synchronous POST request returning full response
- `ik_http_post_stream` - Streaming POST request with SSE callback support

**Responsibilities:**
- Manage libcurl sessions and connection pooling
- Handle request headers and body construction
- Process HTTP status codes and network errors
- Support both synchronous and streaming response modes

### common/sse_parser - SSE Stream Parser

**Purpose:** Parse Server-Sent Events streams from provider APIs.

**Key types:**
- `ik_sse_parser_t` - Parser state machine
- `ik_sse_callback_t` - Callback function type for parsed events

**Key functions:**
- `ik_sse_parse_chunk` - Process incoming data chunk, emit events

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
5. HTTP client initialized with provider's base URL
6. Vtable populated with provider-specific function pointers
7. Provider handle cached in agent context

### Non-Streaming Request Flow

1. Caller creates `ik_request_t` with messages and configuration
2. Call `provider->vt->send(provider->impl_ctx, req, &resp)`
3. Provider translates request to native API format (JSON)
4. HTTP client sends POST request to provider endpoint
5. Provider parses response JSON into `ik_response_t`
6. Response returned with normalized content blocks and usage data

### Streaming Request Flow

1. Caller creates `ik_request_t` and provides stream callback
2. Call `provider->vt->stream(provider->impl_ctx, req, callback, ctx)`
3. Provider translates request to native API format with stream flag
4. HTTP client initiates streaming POST request
5. SSE parser receives chunks, emits parsed events
6. Provider-specific streaming handler translates events to `ik_stream_event_t`
7. Caller's callback invoked with normalized stream events
8. Stream events include text deltas, thinking deltas, and completion

### Cleanup Flow

1. Provider freed via talloc hierarchy
2. Talloc destructor invokes vtable cleanup function
3. Provider-specific cleanup releases resources
4. HTTP client closed and connections released
5. API keys and buffers freed via talloc parent-child relationships

## Integration Points

### REPL Integration

The REPL layer becomes provider-agnostic by routing through the vtable interface instead of directly calling OpenAI functions. The agent context holds the active provider handle, and message sending operations dispatch through `provider->vt->send` or `provider->vt->stream`.

### Agent Context Extension

Agent structure extended with:
- `provider_name` - String identifying provider (e.g., "anthropic")
- `model` - Model name within provider's namespace
- `thinking_level` - Normalized thinking level (LOW/MED/HIGH/MAX)
- `provider` - Cached provider handle (NULL until first use)

### Database Schema Changes

The agents table requires new columns:
- `provider` (TEXT) - Provider name, defaults to "openai"
- `model` (TEXT) - Model identifier
- `thinking_level` (INTEGER) - Enum value for thinking level

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
- Format: `{"anthropic": "sk-ant-...", "openai": "sk-..."}`
- Return auth error with setup instructions if credentials missing

## Migration from Existing OpenAI Code

### Current State (rel-06)

```
src/openai/                    # Hardcoded implementation
  client.c                     # Direct calls from REPL
  client_multi.c               # Streaming support
  client_multi_callbacks.c
  client_multi_request.c
  client_msg.c
  client_serialize.c
  http_handler.c
  sse_parser.c
  tool_choice.c

src/client.c                   # HTTP client
```

**Call sites with hardcoded OpenAI dependencies:**
- `src/commands_basic.c` - Hardcoded model list in `/model` command
- `src/completion.c` - Model autocomplete using fixed OpenAI models
- `src/repl_actions_llm.c` - Direct dispatch to OpenAI client
- `src/client.c` - HTTP layer coupled to OpenAI
- `src/agent.c` - No provider field
- `src/db/agent.c` - No provider column
- `src/config.c` - No credentials loading

### Target State (rel-07)

```
src/providers/
  common/
    http_client.c       # Refactored HTTP layer
    sse_parser.c        # Extracted SSE parser
  provider.h            # Vtable interface
  openai/
    adapter.c           # Vtable implementation
    client.c            # Refactored OpenAI client
    streaming.c         # Refactored streaming support
    openai.h

src/client.c            # Removed or integrated into http_client.c
src/openai/             # DELETED
```

### Migration Strategy: Adapter-First

**Approach:** Maintain OpenAI functionality throughout migration by creating adapter shim, then refactor incrementally.

**Phases:**

1. **Vtable Foundation** - Create `src/providers/provider.h` with interface definitions
2. **Adapter Shim** - Wrap existing `src/openai/` with vtable adapter to validate interface
3. **Call Site Updates** - Route all traffic through provider abstraction
4. **Anthropic Provider** - Add second provider to prove multi-provider design
5. **Google Provider** - Add third provider for additional validation
6. **OpenAI Refactor** - Move OpenAI to native vtable implementation in `src/providers/openai/`
7. **Cleanup** - Delete adapter shim and old `src/openai/` directory

### Critical Cleanup Requirements

After migration completes:
- Old `src/openai/` directory must be completely deleted
- Adapter shim code removed
- No `#include` statements referencing old paths outside `src/providers/`
- No direct function calls to provider implementations outside vtable dispatch
- Makefile updated to remove references to deleted files
- Full test suite passing with new structure

### src/client.c Integration

The existing `src/client.c` HTTP client will be refactored into `src/providers/common/http_client.c` to support all providers. Migration occurs during Phase 3 when call sites are updated to use provider abstraction.

## Error Handling

Providers return errors through standard `res_t` mechanism with appropriate error codes:

- `ERR_AUTH` - Invalid or missing API key (401 status)
- `ERR_RATE_LIMIT` - Rate limit exceeded (429 status)
- `ERR_NETWORK` - Connection failures, timeouts
- `ERR_INVALID_ARG` - Malformed requests
- `ERR_PROVIDER` - Provider-specific API errors (4xx, 5xx)

Error messages include actionable guidance such as credential setup URLs and retry timing.

## Testing Strategy

Each provider has dedicated test suites:

**Unit tests:**
- `tests/unit/providers/test_anthropic_adapter.c` - Vtable implementation
- `tests/unit/providers/test_anthropic_client.c` - Request/response handling
- `tests/unit/providers/test_openai_adapter.c`
- `tests/unit/providers/test_google_adapter.c`
- `tests/unit/common/test_http_client.c` - Shared HTTP layer
- `tests/unit/common/test_sse_parser.c` - SSE parsing

**Integration tests:**
- Test actual provider APIs with real credentials (optional)
- Mock HTTP responses for deterministic testing
- Validate error handling for various failure modes
- Verify streaming event sequences
- Test provider switching within single agent session

Mock responses use existing `MOCKABLE` pattern to simulate provider responses without network calls.
