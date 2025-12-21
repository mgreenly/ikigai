# Provider Interface Specification

## Overview

This document specifies the vtable interface that all provider adapters must implement, along with lifecycle management and common utilities.

## Vtable Interface

### Purpose
The provider vtable defines a uniform interface for interacting with different LLM providers (Anthropic, OpenAI, Google).

### Function Pointers

**send** - Send non-streaming request to provider
- Parameters: provider context, internal request, output response pointer
- Returns: result type (OK/ERR)
- Responsibility: synchronous request/response communication

**stream** - Send streaming request to provider
- Parameters: provider context, internal request, stream callback, callback context
- Returns: result type (OK/ERR)
- Responsibility: asynchronous streaming with callbacks

**cleanup** - Cleanup provider-specific resources
- Parameters: provider context
- Returns: void
- Optional if talloc hierarchy handles all cleanup

### Stream Callback Signature

Function pointer type that receives stream events and user context. See [streaming.md](streaming.md) for event structure details.

## Provider Context

### Purpose
Each provider maintains its own context structure containing:
- Credentials (API key)
- Shared HTTP client reference
- Provider-specific configuration (thinking budgets, rate limits, etc.)
- Optional rate limit tracking state

### Ownership
Context is talloc-allocated and owned by the provider wrapper. Cleaned up when provider is freed.

## Factory Functions

### Provider Creation
Each provider module exports a factory function with signature:
- Function name: `ik_{provider}_create()`
- Parameters: talloc context, API key, output provider pointer
- Returns: result type (OK/ERR)
- Responsibility: initialize provider context and vtable

Providers supported:
- Anthropic: `ik_anthropic_create()`
- OpenAI: `ik_openai_create()`
- Google: `ik_google_create()`

## Implementation Requirements

### send() Requirements
- Transform internal request to provider wire format (JSON)
- Make HTTP POST request to provider API
- Handle HTTP errors (401, 429, 500, etc.)
- Parse response JSON to internal response format
- Map provider finish reasons to normalized enum
- Extract token usage
- Return errors with appropriate category

### stream() Requirements
- Transform internal request to provider wire format
- Make HTTP POST request with SSE streaming
- Parse SSE events from provider
- Convert provider events to normalized stream events
- Call user callback with normalized events
- Handle mid-stream errors
- Accumulate partial response state

### cleanup() Requirements
- Optional implementation
- Only needed for resources beyond talloc hierarchy
- Most providers can set cleanup = NULL

## Error Handling Contract

### Error Categories
Providers must map HTTP and API errors to these categories:
- ERR_AUTH - Invalid credentials (401)
- ERR_RATE_LIMIT - Rate limit exceeded (429)
- ERR_INVALID_ARG - Bad request (400)
- ERR_NOT_FOUND - Model not found (404)
- ERR_SERVER - Server error (500, 502, 503)
- ERR_TIMEOUT - Request timeout
- ERR_CONTENT_FILTER - Content policy violation
- ERR_NETWORK - Network/connection error
- ERR_UNKNOWN - Other errors

See [error-handling.md](error-handling.md) for full mapping tables.

### Error Information
Error structures include:
- Category enum
- HTTP status code (0 if not HTTP error)
- Human-readable message
- Provider's error code/type
- Retry-after milliseconds (-1 if not applicable)

## Common Utilities

### Credentials Loading
**ik_credentials_load()** - Load API key for provider
- Checks environment variable first ({PROVIDER}_API_KEY)
- Falls back to credentials.json file
- Returns allocated key string on success

### Environment Variable Names
**ik_provider_env_var()** - Get environment variable name for provider
- Maps provider name to environment variable
- Examples: "anthropic" → "ANTHROPIC_API_KEY", "openai" → "OPENAI_API_KEY"
- Returns static string (no allocation)

Supported mappings:
- anthropic → ANTHROPIC_API_KEY
- openai → OPENAI_API_KEY
- google → GOOGLE_API_KEY
- xai → XAI_API_KEY
- meta → LLAMA_API_KEY

### HTTP Client Creation
**ik_http_client_create()** - Create HTTP client for provider
- Parameters: talloc context, base URL, output client pointer
- Returns: result type
- Client handles connection pooling and request management

### SSE Parser Creation
**ik_sse_parser_create()** - Create SSE parser
- Parameters: talloc context, event callback, user context, output parser pointer
- Returns: result type
- Parser handles SSE protocol and event extraction

## Provider Registration

### Dispatch Mechanism
Static dispatch in `ik_provider_create()`:
- Loads credentials via `ik_credentials_load()`
- Dispatches to appropriate factory based on provider name
- Returns error for unknown providers
- No dynamic registration - simple switch/if-else

## Thread Safety

Not required for rel-07. ikigai is single-threaded. Providers can assume single-threaded access.

## Performance Considerations

### Connection Pooling
HTTP clients may implement connection pooling for reusing connections across multiple requests.

### Request Caching
Not implemented in rel-07. Future optimization.

### Partial Response Accumulation
Streaming implementations should accumulate partial responses efficiently:
- Text deltas accumulated in string builder
- Tool calls tracked in array
- Token usage accumulated

## Extension Points

### Adding New Providers
Steps to add provider support (xAI, Meta, etc.):
1. Create `src/providers/{name}/` directory
2. Implement factory function: `ik_{name}_create()`
3. Implement vtable functions (send, stream, cleanup)
4. Add dispatch case to `ik_provider_create()`
5. Add credential loading for `{NAME}_API_KEY`
6. Add tests in `tests/unit/providers/`

### Custom Headers
Providers can add custom headers as needed:
- Anthropic requires: `x-api-key`, `anthropic-version`
- OpenAI supports optional: `OpenAI-Organization`, `OpenAI-Project`

### Provider-Specific Features
Features only supported by some providers can be added to `provider_data` field in request/response structures. Providers extract during serialization while keeping core format clean.
