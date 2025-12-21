# Task: Update REPL to Use Provider Abstraction

**Phase:** 3 - REPL Integration
**Depends on:** 04-openai-adapter-shim, 07-agent-provider-fields

## Objective

Update REPL to route all LLM requests through the provider vtable instead of directly calling OpenAI.

## Deliverables

1. Update `src/repl_actions_llm.c`:
   - Replace `ik_openai_*` calls with provider vtable
   - Get provider via `ik_agent_get_provider()`
   - Build `ik_request_t` from conversation
   - Call `provider->vt->stream()`

2. Update streaming callbacks:
   - Handle normalized `ik_stream_event_t`
   - Map events to scrollback updates
   - Accumulate tool calls
   - Handle thinking deltas

3. Update message saving:
   - Store provider/model/thinking in message data
   - Store thinking content if present
   - Store token usage breakdown

## Reference

- `scratch/plan/architecture.md` - Integration Points section
- `scratch/plan/streaming.md` - REPL Stream Callback section

## Key Change

```c
// Before (rel-06)
ik_openai_stream(repl->llm, messages, ...);

// After (rel-07)
ik_provider_t *provider = NULL;
TRY(ik_agent_get_provider(agent, &provider));
provider->vt->stream(provider->impl_ctx, req, callback, ctx);
```

## Verification

- All OpenAI traffic goes through vtable
- Streaming works correctly
- Messages saved with provider info
