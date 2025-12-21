# Task: Implement Anthropic Provider

**Layer:** 3
**Model:** sonnet/extended
**Depends on:** provider-types.md, shared-utilities.md, request-builders.md

## Pre-Read

**Skills:**
- `/load errors`
- `/load patterns/vtable`

**Research Documents:**
- `scratch/research/anthropic.md`

**Plan Documents:**
- `scratch/plan/transformation.md`
- `scratch/plan/thinking-abstraction.md`

## Objective

Create native Anthropic provider implementing the vtable interface.

## Deliverables

1. Create `src/providers/anthropic/` directory:
   - `anthropic.h` - Public interface
   - `adapter.c` - Vtable implementation
   - `client.c` - Request serialization
   - `streaming.c` - SSE event handling

2. Implement vtable:
   - `ik_anthropic_create()` - Factory function
   - `ik_anthropic_send()` - Non-streaming requests
   - `ik_anthropic_stream()` - Streaming with SSE

3. Implement request transformation:
   - System prompt → `system` field
   - Messages → Anthropic format
   - Thinking level → `budget_tokens`
   - Tools → `tools` array

4. Implement response parsing:
   - Content blocks → internal format
   - Tool calls → `IK_CONTENT_TOOL_CALL`
   - Thinking → `IK_CONTENT_THINKING`
   - Usage → `ik_usage_t`

5. Implement streaming:
   - Parse Anthropic SSE events
   - Emit normalized `ik_stream_event_t`

6. Implement thinking budget calculation:
   - Model-specific min/max budgets
   - Level → budget mapping

## Reference

- `scratch/plan/transformation.md` - Anthropic Transformation
- `scratch/plan/thinking-abstraction.md` - Anthropic section
- `scratch/research/anthropic.md` - API details

## Verification

- Can send non-streaming request
- Can stream responses
- Tool calls work
- Thinking levels map correctly

## Postconditions

- [ ] Non-streaming requests work correctly
- [ ] Streaming responses work with SSE
- [ ] Tool calls are properly transformed and parsed
- [ ] Thinking levels map correctly to budget_tokens
