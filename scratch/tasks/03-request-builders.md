# Task: Create Request/Response Builders

**Phase:** 1 - Foundation
**Depends on:** 01-provider-types

## Objective

Create builder functions for constructing `ik_request_t` and working with `ik_response_t`.

## Deliverables

1. Create `src/providers/request.c` with:
   - `ik_request_create()` - Create empty request
   - `ik_request_set_system()` - Set system prompt
   - `ik_request_add_message()` - Add simple text message
   - `ik_request_add_message_blocks()` - Add message with content blocks
   - `ik_request_set_thinking()` - Configure thinking level
   - `ik_request_add_tool()` - Add tool definition

2. Create `src/providers/response.c` with:
   - `ik_response_create()` - Create empty response
   - `ik_response_add_content()` - Add content block
   - Helper functions for parsing from JSON

## Reference

- `scratch/plan/request-response-format.md` - Builder pattern section

## Verification

- Can construct requests with all content types
- Round-trip: request → serialize → parse → response
