# Task: Define Provider Core Types

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Critical Architecture Constraint

The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking:

- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

Reference: `src/openai/client_multi.c` (existing async implementation)

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/error.h` - Existing `err_code_t` enum and `res_t` type
- `src/openai/client.h` - Existing provider structure
- `src/openai/client_multi.h` - Existing async HTTP pattern (fdset/perform/timeout/info_read)
- `src/msg.h` - Message types
- `src/tool.h` - Tool types

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop, curl_multi required
- `scratch/plan/architecture.md` - Event loop integration details, vtable interface overview
- `scratch/plan/provider-interface.md` - Complete vtable specification with callback signatures
- `scratch/plan/streaming.md` - Stream event types and async data flow
- `scratch/plan/error-handling.md` - Error categories

## Objective

Create `src/providers/provider.h` with vtable definition and core types that all providers will implement. This is a **header-only task** defining the provider abstraction interface through enums, structs, callback types, and function pointer types. No implementation files are created.

**Key design point:** The vtable uses an async/non-blocking pattern with:
- Event loop integration methods: `fdset()`, `perform()`, `timeout()`, `info_read()`
- Non-blocking request initiation: `start_request()`, `start_stream()` (return immediately)
- Callbacks for response delivery: `ik_provider_completion_cb_t`, `ik_stream_cb_t`

## Include Order

**Critical:** All provider files reference types like `ik_thinking_level_t`, `ik_provider_t`, and `ik_provider_vtable_t`. The include order is strict to avoid compilation failures:

1. **`src/providers/provider.h`** defines all core provider types:
   - `ik_thinking_level_t`, `ik_finish_reason_t`, `ik_content_type_t`, etc. (enums)
   - `ik_provider_t`, `ik_provider_vtable_t`, `ik_request_t`, `ik_response_t`, etc. (structs)
   - `ik_stream_cb_t`, `ik_provider_completion_cb_t` (callback types)

2. **Provider-specific headers** (e.g., `src/providers/anthropic/anthropic.h`, `src/providers/openai/openai.h`) MUST include `provider.h` first before declaring provider-specific functions.

3. **Application code** should include `provider.h` before any provider-specific headers.

**Example:**
```c
// In src/providers/anthropic/anthropic.h
#include "src/providers/provider.h"  // REQUIRED first

// In application code
#include "src/providers/provider.h"
#include "src/providers/anthropic/anthropic.h"
```

## Interface

### Enums to Define

| Enum | Values | Purpose |
|------|--------|---------|
| `ik_thinking_level_t` | IK_THINKING_NONE (0), IK_THINKING_LOW (1), IK_THINKING_MED (2), IK_THINKING_HIGH (3) | Provider-agnostic thinking budget levels |
| `ik_finish_reason_t` | IK_FINISH_STOP, IK_FINISH_LENGTH, IK_FINISH_TOOL_USE, IK_FINISH_CONTENT_FILTER, IK_FINISH_ERROR, IK_FINISH_UNKNOWN | Normalized completion reasons across providers |
| `ik_content_type_t` | IK_CONTENT_TEXT, IK_CONTENT_TOOL_CALL, IK_CONTENT_TOOL_RESULT, IK_CONTENT_THINKING | Content block types |
| `ik_role_t` | IK_ROLE_USER, IK_ROLE_ASSISTANT, IK_ROLE_TOOL | Message roles |
| `ik_tool_choice_t` | IK_TOOL_AUTO, IK_TOOL_NONE, IK_TOOL_REQUIRED, IK_TOOL_SPECIFIC | Tool invocation control modes |
| `ik_error_category_t` | IK_ERR_CAT_AUTH, IK_ERR_CAT_RATE_LIMIT, IK_ERR_CAT_INVALID_ARG, IK_ERR_CAT_NOT_FOUND, IK_ERR_CAT_SERVER, IK_ERR_CAT_TIMEOUT, IK_ERR_CAT_CONTENT_FILTER, IK_ERR_CAT_NETWORK, IK_ERR_CAT_UNKNOWN | Provider error categories for retry logic |
| `ik_stream_event_type_t` | IK_STREAM_START, IK_STREAM_TEXT_DELTA, IK_STREAM_THINKING_DELTA, IK_STREAM_TOOL_CALL_START, IK_STREAM_TOOL_CALL_DELTA, IK_STREAM_TOOL_CALL_DONE, IK_STREAM_DONE, IK_STREAM_ERROR | Stream event types |

### Thinking Configuration Validation

Providers MUST validate the requested thinking level against model capabilities before initiating requests. Thinking support varies by provider and model:

**Validation Rules:**

1. **Non-thinking model requested with thinking enabled:**
   - If thinking level > IK_THINKING_NONE for a model that does not support thinking
   - Return `ERR(ctx, ERR_INVALID_ARG, "Model <model_name> does not support thinking")`
   - Example: User requests `IK_THINKING_HIGH` on `gpt-4`

2. **Thinking-required model requested with thinking disabled:**
   - If thinking level == IK_THINKING_NONE for a model that requires thinking to be enabled
   - Return `ERR(ctx, ERR_INVALID_ARG, "Model <model_name> requires thinking to be enabled")`
   - Example: User requests `IK_THINKING_NONE` on `o1-preview`

3. **Provider implementation must validate in `start_request()` and `start_stream()`:**
   - Check thinking level compatibility before constructing API request
   - Return error immediately if validation fails (before making HTTP call)

**Provider-Specific Examples:**

- **OpenAI:**
  - Only `o1-*` and `o3-*` models support reasoning (thinking)
  - `gpt-*` models: thinking level MUST be IK_THINKING_NONE, otherwise return ERR_INVALID_ARG
  - `o1-*`, `o3-*` models: thinking level MUST NOT be IK_THINKING_NONE, otherwise return ERR_INVALID_ARG
  - Reasoning effort mapping: OpenAI uses string values ("low"/"medium"/"high") for `reasoning_effort` field (NOT numeric token budgets)
    - Map IK_THINKING_LOW → "low"
    - Map IK_THINKING_MED → "medium"
    - Map IK_THINKING_HIGH → "high"

- **Anthropic:**
  - All Claude models support thinking (extended_thinking parameter)
  - Thinking can be enabled or disabled (IK_THINKING_NONE is valid)
  - Uses numeric `budget_tokens` field (NOT string values)
    - IK_THINKING_LOW → 1024 tokens
    - IK_THINKING_MED → 22016 tokens
    - IK_THINKING_HIGH → 43008 tokens
  - Token budget varies by model tier (Claude Opus vs Sonnet vs Haiku)
  - Validation: Check if requested budget exceeds model's maximum thinking tokens

- **Google:**
  - Gemini 2.5: Supports thinking via `thought` content blocks
  - Gemini 3.0: Different thinking mode implementation
  - Thinking can be enabled or disabled (IK_THINKING_NONE is valid)
  - Uses numeric `thinking_budget` field (NOT string values)
    - IK_THINKING_LOW → 128 tokens
    - IK_THINKING_MED → 11008 tokens
    - IK_THINKING_HIGH → 21888 tokens
  - Validation: Verify model version supports requested thinking mode

**Error Message Format:**

All validation errors should use consistent messaging:
- Non-thinking model: `"Model {model_name} does not support thinking"`
- Thinking-required model: `"Model {model_name} requires thinking to be enabled"`
- Budget exceeded: `"Thinking budget {level} exceeds maximum for model {model_name}"`

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_usage_t` | input_tokens, output_tokens, thinking_tokens, cached_tokens, total_tokens | Token usage counters |
| `ik_thinking_config_t` | level (ik_thinking_level_t), include_summary (bool) | Thinking configuration |
| `ik_content_block_t` | type, union of {text, tool_call, tool_result, thinking} | Message content block with variant data |
| `ik_message_t` | role, content blocks array, provider metadata | Single message in conversation |
| `ik_tool_def_t` | name, description, parameters (JSON schema), strict (bool) | Tool definition |
| `ik_request_t` | system_prompt, messages, model, thinking config, tools, max_output_tokens | Request to provider |
| `ik_response_t` | content blocks, finish_reason, usage, model, provider_data | Response from provider |
| `ik_stream_event_t` | type, union of event-specific data | Streaming event with variant payload |
| `ik_provider_completion_t` | success (bool), http_status, response (ik_response_t*), error_category, error_message, retry_after_ms | Completion callback payload |
| `ik_provider_vtable_t` | fdset(), perform(), timeout(), info_read(), start_request(), start_stream(), cleanup() | Provider vtable interface (async) |
| `ik_provider_t` | name, vtable pointer, impl_ctx (opaque) | Provider wrapper |

### Forward Declarations File

Create `src/providers/provider_types.h` with forward declarations for all provider types (ik_provider_t, ik_request_t, ik_response_t, etc.).

### Callback Type Definitions

| Type | Signature | Purpose |
|------|-----------|---------|
| `ik_stream_cb_t` | `res_t (*)(const ik_stream_event_t *event, void *ctx)` | Callback for streaming events during perform() |
| `ik_provider_completion_cb_t` | `res_t (*)(const ik_provider_completion_t *completion, void *ctx)` | Callback invoked when request completes from info_read() |

### Vtable Interface (Async/Non-blocking)

All vtable methods are non-blocking. Request initiation returns immediately; progress is made through the event loop integration methods.

**Event Loop Integration (REQUIRED):**

| Function Pointer | Signature | Purpose |
|------------------|-----------|---------|
| `fdset` | `res_t (*)(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd)` | Populate fd_sets for select() - adds provider's curl_multi FDs |
| `perform` | `res_t (*)(void *ctx, int *running_handles)` | Process pending I/O (non-blocking) - drives curl_multi_perform() |
| `timeout` | `res_t (*)(void *ctx, long *timeout_ms)` | Get recommended timeout for select() from curl |
| `info_read` | `void (*)(void *ctx, ik_logger_t *logger)` | Process completed transfers, invoke completion callbacks |

**Request Initiation (Non-blocking):**

| Function Pointer | Signature | Purpose |
|------------------|-----------|---------|
| `start_request` | `res_t (*)(void *ctx, const ik_request_t *req, ik_provider_completion_cb_t cb, void *cb_ctx)` | Start non-streaming request, returns immediately |
| `start_stream` | `res_t (*)(void *ctx, const ik_request_t *req, ik_stream_cb_t stream_cb, void *stream_ctx, ik_provider_completion_cb_t completion_cb, void *completion_ctx)` | Start streaming request, returns immediately |

**Cleanup & Cancellation:**

| Function Pointer | Signature | Purpose |
|------------------|-----------|---------|
| `cleanup` | `void (*)(void *ctx)` | Cleanup provider resources (may be NULL if talloc handles everything) |
| `cancel` | `void (*)(void *ctx)` | Cancel all in-flight requests (Ctrl+C, agent termination). Must be async-signal-safe. |

## Behaviors

### Async Operation Model

The vtable follows the async curl_multi pattern used by the existing OpenAI implementation:

1. **Request initiation:** `start_request()` / `start_stream()` configure curl easy handle and add to multi handle, then return immediately (non-blocking)
2. **Event loop integration:** REPL calls `fdset()` before select(), `perform()` after select() returns, and `info_read()` to check for completions
3. **Callback delivery:** Stream events delivered via `stream_cb` during `perform()` as data arrives. Completion delivered via `completion_cb` from `info_read()` when transfer finishes

This matches the pattern in `src/openai/client_multi.c`:
- `ik_openai_multi_fdset()` -> `fdset()`
- `ik_openai_multi_perform()` -> `perform()`
- `ik_openai_multi_timeout()` -> `timeout()`
- `ik_openai_multi_info_read()` -> `info_read()`

### Error Type Coexistence

Two separate error systems:
- `err_code_t` in `src/error.h` - System-level errors (IO, parse, DB)
- `ik_error_category_t` in provider types - Provider API errors (auth, rate limit)

Provider vtable functions return `res_t` (uses `err_code_t`). On provider errors, return `ERR(ctx, ERR_PROVIDER, "message")` where ERR_PROVIDER is a new error code added to `err_code_t`.

Provider-specific errors (auth failures, rate limits, etc.) are delivered via the `ik_provider_completion_t` struct passed to completion callbacks, not through the return value of `start_request()`/`start_stream()`.

### Content Block Variants

`ik_content_block_t` uses tagged union:
- IK_CONTENT_TEXT: contains text string
- IK_CONTENT_TOOL_CALL: contains id, name, arguments (parsed JSON object)
- IK_CONTENT_TOOL_RESULT: contains tool_call_id, content, is_error flag
- IK_CONTENT_THINKING: contains thinking text summary

### Stream Event Variants

`ik_stream_event_t` uses tagged union with different data per type:
- IK_STREAM_START: model name
- IK_STREAM_TEXT_DELTA/IK_STREAM_THINKING_DELTA: text fragment, block index
- IK_STREAM_TOOL_CALL_START: id, name, index
- IK_STREAM_TOOL_CALL_DELTA: arguments fragment, index
- IK_STREAM_TOOL_CALL_DONE: index
- IK_STREAM_DONE: finish_reason, usage, provider_data
- IK_STREAM_ERROR: category, message

### Event Index Semantics

The `index` field in `ik_stream_event_t` represents the content block index within the current response. This is a **canonical format** that provider implementations MUST normalize to, regardless of their internal indexing schemes.

**Indexing Rules:**

1. The index field represents the content block index within the current response
2. For simple text responses: index is always 0
3. For tool calls: each tool call gets a unique index (0, 1, 2, ...)
4. For thinking + text: thinking content is index 0, text content is index 1
5. Index resets to 0 for each new response/turn
6. Provider implementations MUST normalize their internal indices to this canonical format

**Provider Index Normalization:**

Different providers use different indexing schemes in their streaming APIs:
- Anthropic: `current_block_index` (0-based content block index)
- Google: `part_index` (per-part index within a candidate)
- OpenAI: `tool_call_index` (tool-specific index)

Provider implementations are responsible for mapping these provider-specific indices to the canonical index format described above.

**Example Index Values:**

| Scenario | Event Type | Index | Description |
|----------|-----------|-------|-------------|
| Text only | IK_STREAM_TEXT_DELTA | 0 | Single text content block |
| Single tool call | IK_STREAM_TOOL_CALL_START/DELTA/DONE | 0 | Only one tool call |
| Two tool calls | IK_STREAM_TOOL_CALL_START/DELTA/DONE | 0, 1 | First tool call: 0, Second tool call: 1 |
| Thinking + text | IK_STREAM_THINKING_DELTA | 0 | Thinking content block |
| Thinking + text | IK_STREAM_TEXT_DELTA | 1 | Text content block after thinking |
| Thinking + 2 tools | IK_STREAM_THINKING_DELTA | 0 | Thinking block |
| Thinking + 2 tools | IK_STREAM_TOOL_CALL_START/DELTA/DONE | 1, 2 | First tool: 1, Second tool: 2 |

### HTTP Completion Structure

`ik_provider_completion_t` contains all information about a completed request:
- `success` (bool): true if request succeeded
- `http_status` (int): HTTP status code
- `response` (ik_response_t*): Parsed response (NULL on error)
- `error_category` (ik_error_category_t): Error category if failed
- `error_message` (char*): Human-readable error message if failed
- `retry_after_ms` (int32_t): Suggested retry delay (-1 if not applicable)

### Memory Management

All structs use talloc patterns. No direct malloc/free. Provider implementations store opaque context in `impl_ctx`.

## Functions to Define

In `src/providers/provider.h`:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_infer_provider` | `const char *ik_infer_provider(const char *model_name)` | Infer provider name from model prefix. Returns "openai" for "gpt-*", "o1-*", "o3-*"; "anthropic" for "claude-*"; "google" for "gemini-*". Returns NULL for unknown models. |

**Model prefix to provider mapping:**
- `gpt-*`, `o1-*`, `o3-*` → "openai"
- `claude-*` → "anthropic"
- `gemini-*` → "google"
- Unknown → NULL

## Directory Structure

```
src/providers/
  provider.h          # Core types (this task)
  provider_types.h    # Forward declarations only
```

## Error Code Additions

Add the following error codes to `err_code_t` enum in `src/error.h`:

| Code | Value | String | Purpose |
|------|-------|--------|---------|
| `ERR_PROVIDER` | 9 | "Provider error" | Provider API errors (auth, rate limit, etc.) |
| `ERR_MISSING_CREDENTIALS` | 10 | "Missing credentials" | No API key configured for provider |
| `ERR_NOT_IMPLEMENTED` | 11 | "Not implemented" | Stub functions not yet implemented |

Add corresponding cases to `error_code_str()` for each new code. Also verify all existing `err_code_t` values have cases - `ERR_AGENT_NOT_FOUND` is currently missing and must be added with string "Agent not found".

## Test Scenarios

Create `tests/unit/providers/provider_types_test.c`:

- Enum values verification: All enums have expected integer values
- Struct size validation: Verify no unexpected padding issues
- Talloc allocation: Can allocate and free request/response structures with talloc
- Type safety: Compile-time type checking works for all structs
- Error codes: `ERR_PROVIDER`, `ERR_MISSING_CREDENTIALS`, `ERR_NOT_IMPLEMENTED` have correct values and strings
- Callback type compatibility: Function pointer assignments compile correctly

## Postconditions

- [ ] `src/providers/provider.h` compiles without errors
- [ ] `src/providers/provider_types.h` compiles without errors
- [ ] All enums have explicit integer values (no gaps)
- [ ] All structs use talloc-compatible patterns
- [ ] No provider-specific dependencies (no OpenAI/Anthropic includes)
- [ ] Vtable defines async methods: `fdset`, `perform`, `timeout`, `info_read`, `start_request`, `start_stream`, `cleanup`, `cancel`
- [ ] Vtable does NOT have blocking `send` or `stream` methods
- [ ] Callback types `ik_stream_cb_t` and `ik_provider_completion_cb_t` defined
- [ ] `ik_provider_completion_t` struct defined for completion callback payload
- [ ] `ERR_PROVIDER`, `ERR_MISSING_CREDENTIALS`, `ERR_NOT_IMPLEMENTED` added to `src/error.h`
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Header compiles in isolation (no missing includes)
- [ ] No provider-specific strings in header files
- [ ] Changes committed to git with message: `task: provider-types.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
