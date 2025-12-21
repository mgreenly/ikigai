# Task: Define Provider Core Types

**Layer:** 0
**Model:** sonnet/none
**Depends on:** None

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/error.h` - Existing `err_code_t` enum and `res_t` type
- `src/openai/client.h` - Existing provider structure
- `src/msg.h` - Message types
- `src/tool.h` - Tool types

**Plan:**
- `scratch/plan/provider-interface.md` - Vtable specification
- `scratch/plan/request-response-format.md` - Data structures
- `scratch/plan/streaming.md` - Stream event types
- `scratch/plan/error-handling.md` - Error categories

## Objective

Create `src/providers/provider.h` with vtable definition and core types that all providers will implement. This is a **header-only task** defining the provider abstraction interface through enums, structs, and function pointer types. No implementation files are created.

## Interface

### Enums to Define

| Enum | Values | Purpose |
|------|--------|---------|
| `ik_thinking_level_t` | NONE (0), LOW (1), MED (2), HIGH (3) | Provider-agnostic thinking budget levels |
| `ik_finish_reason_t` | STOP, LENGTH, TOOL_USE, CONTENT_FILTER, ERROR, UNKNOWN | Normalized completion reasons across providers |
| `ik_content_type_t` | TEXT, TOOL_CALL, TOOL_RESULT, THINKING | Content block types |
| `ik_role_t` | USER, ASSISTANT, TOOL | Message roles |
| `ik_tool_choice_t` | AUTO, NONE, REQUIRED, SPECIFIC | Tool invocation control modes |
| `ik_error_category_t` | AUTH, RATE_LIMIT, INVALID_ARG, NOT_FOUND, SERVER, TIMEOUT, CONTENT_FILTER, NETWORK, UNKNOWN | Provider error categories for retry logic |
| `ik_stream_event_type_t` | START, TEXT_DELTA, THINKING_DELTA, TOOL_CALL_START, TOOL_CALL_DELTA, TOOL_CALL_DONE, DONE, ERROR | Stream event types |

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
| `ik_provider_vtable_t` | send(), stream(), cleanup() function pointers | Provider vtable interface |
| `ik_provider_t` | name, vtable pointer, impl_ctx (opaque) | Provider wrapper |

### Forward Declarations File

Create `src/providers/provider_types.h` with forward declarations for all provider types (ik_provider_t, ik_request_t, ik_response_t, etc.).

### Stream Callback Type

| Type | Signature | Purpose |
|------|-----------|---------|
| `ik_stream_callback_t` | `void (*)(ik_stream_event_t *event, void *user_ctx)` | Callback for streaming events |

### Vtable Interface

| Function Pointer | Signature | Purpose |
|------------------|-----------|---------|
| `send` | `res_t (*)(void *impl_ctx, ik_request_t *req, ik_response_t **out_resp)` | Send non-streaming request |
| `stream` | `res_t (*)(void *impl_ctx, ik_request_t *req, ik_stream_callback_t cb, void *cb_ctx)` | Send streaming request |
| `cleanup` | `void (*)(void *impl_ctx)` | Cleanup provider resources (may be NULL) |

## Behaviors

### Error Type Coexistence

Two separate error systems:
- `err_code_t` in `src/error.h` - System-level errors (IO, parse, DB)
- `ik_error_category_t` in provider types - Provider API errors (auth, rate limit)

Provider vtable functions return `res_t` (uses `err_code_t`). On provider errors, return `ERR(ctx, ERR_PROVIDER, "message")` where ERR_PROVIDER is a new error code added to `err_code_t`.

### Content Block Variants

`ik_content_block_t` uses tagged union:
- TEXT: contains text string
- TOOL_CALL: contains id, name, arguments (parsed JSON object)
- TOOL_RESULT: contains tool_call_id, content, is_error flag
- THINKING: contains thinking text summary

### Stream Event Variants

`ik_stream_event_t` uses tagged union with different data per type:
- START: model name
- TEXT_DELTA/THINKING_DELTA: text fragment, block index
- TOOL_CALL_START: id, name, index
- TOOL_CALL_DELTA: arguments fragment, index
- TOOL_CALL_DONE: index
- DONE: finish_reason, usage, provider_data
- ERROR: category, message

### Memory Management

All structs use talloc patterns. No direct malloc/free. Provider implementations store opaque context in `impl_ctx`.

## Directory Structure

```
src/providers/
├── provider.h          # Core types (this task)
├── provider_types.h    # Forward declarations only
```

## Error Code Addition

Add `ERR_PROVIDER` to `err_code_t` enum in `src/error.h` with value 9. Add corresponding case to `error_code_str()` returning "Provider error".

## Test Scenarios

Create `tests/unit/providers/provider_types_test.c`:

- Enum values verification: All enums have expected integer values
- Struct size validation: Verify no unexpected padding issues
- Talloc allocation: Can allocate and free request/response structures with talloc
- Type safety: Compile-time type checking works for all structs

## Postconditions

- [ ] `src/providers/provider.h` compiles without errors
- [ ] `src/providers/provider_types.h` compiles without errors
- [ ] All enums have explicit integer values (no gaps)
- [ ] All structs use talloc-compatible patterns
- [ ] No provider-specific dependencies (no OpenAI/Anthropic includes)
- [ ] `ERR_PROVIDER` added to `src/error.h`
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Header compiles in isolation (no missing includes)
- [ ] No provider-specific strings in header files
