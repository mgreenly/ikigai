# Task: Define Provider Core Types

**Phase:** 1 - Foundation
**Depends on:** None

## Objective

Create `src/providers/provider.h` with vtable definition and core types that all providers will implement.

## Deliverables

1. Create `src/providers/` directory structure
2. Define `ik_provider_vtable_t` with `send()`, `stream()`, `cleanup()` functions
3. Define `ik_provider_t` wrapper struct (name, vtable, impl_ctx)
4. Define `ik_request_t` internal request format
5. Define `ik_response_t` internal response format
6. Define `ik_content_block_t` for text, tool_call, tool_result, thinking
7. Define `ik_stream_event_t` for normalized streaming events
8. Define `ik_thinking_level_t` enum (NONE, LOW, MED, HIGH)
9. Define `ik_finish_reason_t` enum
10. Define `ik_usage_t` for token counts
11. Define `ik_error_category_t` enum

## Reference

- `scratch/plan/provider-interface.md` - Vtable specification
- `scratch/plan/request-response-format.md` - Data structures
- `scratch/plan/streaming.md` - Stream event types
- `scratch/plan/error-handling.md` - Error categories

## Verification

- Header compiles without errors
- Types can be used in test stubs
