# Task: Refactor OpenAI to Native Provider

**Phase:** 6 - OpenAI Native
**Depends on:** 04-openai-adapter-shim, 10-anthropic-provider

## Objective

Replace the adapter shim with a native vtable implementation, moving code from `src/openai/` to `src/providers/openai/`.

## Deliverables

1. Refactor `src/providers/openai/`:
   - `adapter.c` - Native vtable implementation (not shim)
   - `client.c` - Request serialization
   - `streaming.c` - SSE event handling

2. Implement request transformation:
   - System prompt → `messages[0]` with role: "system"
   - Messages → Chat Completions format
   - Thinking level → `reasoning_effort`
   - Tools → `tools` array with function wrapper

3. Implement response parsing:
   - `choices[0].message.content` → text
   - `choices[0].message.tool_calls` → tool calls
   - Parse `arguments` from JSON string
   - Usage → `ik_usage_t`

4. Implement streaming:
   - Parse Chat Completions SSE events
   - Handle Responses API if `use_responses_api` config set
   - Emit normalized `ik_stream_event_t`

5. Implement thinking effort mapping:
   - Reasoning models: low/medium/high
   - Non-reasoning models: skip

## Reference

- `scratch/plan/transformation.md` - OpenAI Transformation
- `scratch/plan/thinking-abstraction.md` - OpenAI section
- `scratch/research/openai.md` - API details

## Verification

- All existing OpenAI tests pass
- Can use both Chat Completions and Responses API
- Tool calls work
- Reasoning effort maps correctly
