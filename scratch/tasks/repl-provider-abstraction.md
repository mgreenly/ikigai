# Task: Update REPL to Use Provider Abstraction

**Layer:** 3
**Model:** sonnet/extended
**Depends on:** openai-adapter-shim.md, agent-provider-fields.md, configuration.md

## Pre-Read

**Skills:**
- `/load source-code` - Map of REPL implementation
- `/load ddd` - Domain modeling patterns
- `/load errors` - Result type patterns

**Source:**
- `src/repl_actions_llm.c` - LLM request handling
- `src/repl_callbacks.c` - Streaming callbacks
- `src/repl_event_handlers.c` - Event processing
- `src/config.h` - Provider defaults

**Plan:**
- `scratch/plan/architecture.md` - Integration Points section
- `scratch/plan/streaming.md` - REPL callback section
- `scratch/plan/configuration.md` - Default provider section

## Objective

Update REPL to route all LLM requests through the provider vtable instead of directly calling OpenAI. This includes updating streaming callbacks to handle normalized events and ensuring new agents are initialized with correct provider defaults from configuration.

## Interface

Functions to update:

| Function | Changes |
|----------|---------|
| `res_t ik_repl_send_llm_request(ik_repl_t *repl)` | Replace OpenAI calls with provider vtable, build `ik_request_t`, call `provider->vt->stream()` |
| `void ik_repl_stream_callback(ik_stream_event_t *event, void *ctx)` | Handle normalized events instead of OpenAI-specific events |
| `res_t ik_repl_save_message(...)` | Store provider/model/thinking in message JSONB data |
| `res_t ik_repl_init_agent(ik_repl_t *repl, ik_config_t *config)` | Apply defaults to new root agent via `ik_agent_apply_defaults()` |

Structs to update:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_repl_t` | Remove OpenAI-specific client handle | Use provider from agent context instead |

Files to update:

- `src/repl_actions_llm.c` - LLM request routing
- `src/repl_callbacks.c` - Stream event handling
- `src/repl_event_handlers.c` - Message persistence
- `src/repl.c` - Agent initialization

## Behaviors

### LLM Request Routing
- Get provider via `ik_agent_get_provider(agent, &provider)`
- Build normalized `ik_request_t` from conversation state
- Include system messages, user messages, assistant messages
- Include tool definitions if any
- Set `thinking_level` from agent configuration
- Call `provider->vt->stream(provider->impl_ctx, req, callback, ctx)`
- Return ERR_MISSING_CREDENTIALS if provider creation fails
- Return errors from provider vtable as-is

### Stream Callback Handling
- Handle normalized `ik_stream_event_t` types:
  - `IK_STREAM_CONTENT_DELTA` - Append to scrollback
  - `IK_STREAM_TOOL_CALL_DELTA` - Accumulate tool call
  - `IK_STREAM_THINKING_DELTA` - Store thinking content
  - `IK_STREAM_DONE` - Finalize message, save to database
  - `IK_STREAM_ERROR` - Display error, abort request
- Accumulate deltas into complete message
- Update UI progressively during streaming
- Handle partial tool calls and thinking content

### Message Persistence
- Store in JSONB `data` column:
  - `provider` - Provider name (e.g., "anthropic")
  - `model` - Model ID (e.g., "claude-sonnet-4-5-20250929")
  - `thinking_level` - Thinking level (e.g., "med")
  - `thinking` - Thinking content if present
  - `thinking_tokens` - Thinking token count
  - `input_tokens` - Input token count
  - `output_tokens` - Output token count
  - `total_tokens` - Total token count
  - `provider_data` - Provider-specific metadata
- Use existing `ik_db_message_insert()` - JSONB is flexible
- No changes to message.c needed

### Agent Initialization
- New root agent: call `ik_agent_apply_defaults(agent, config)`
- Ensures agent has `config->default_provider` on first creation
- Forked agents already inherit from parent (handled in agent.c)
- Restored agents already load from database (handled in agent.c)

### Backward Compatibility
- Remove direct OpenAI client initialization
- Remove OpenAI-specific callback handling
- Replace with provider abstraction everywhere
- Ensure all existing REPL tests still pass

### Error Handling
- Return ERR_MISSING_CREDENTIALS if provider unavailable
- Return ERR_PROVIDER if vtable call fails
- Display user-friendly error messages in REPL
- Log detailed error information for debugging

## Migration Changes

### Before (rel-06)
```c
// Direct OpenAI calls
ik_openai_stream(repl->llm, messages, callback, ctx);

// OpenAI-specific events
void callback(ik_openai_event_t *event, void *ctx) { ... }
```

### After (rel-07)
```c
// Provider abstraction
ik_provider_t *provider = NULL;
TRY(ik_agent_get_provider(agent, &provider));
provider->vt->stream(provider->impl_ctx, req, callback, ctx);

// Normalized events
void callback(ik_stream_event_t *event, void *ctx) { ... }
```

## Test Scenarios

### Request Routing
- Send message through provider vtable: succeeds
- Provider returns response: displayed in REPL
- Missing credentials: displays error to user
- NULL agent: returns ERR_INVALID_ARG

### Stream Callback
- Content delta: appends to scrollback
- Tool call delta: accumulates correctly
- Thinking delta: stored separately
- Done event: saves complete message
- Error event: displays error, aborts request

### Message Persistence
- Message saved with provider metadata
- Thinking content stored if present
- Token counts stored correctly
- JSONB data queryable

### Agent Initialization
- New root agent: gets config default provider
- Forked agent: inherits parent provider
- Restored agent: loads provider from database
- Config with "anthropic": agent uses anthropic

### Backward Compatibility
- All existing REPL tests pass
- No OpenAI-specific code remains
- Streaming works identically to before
- Message format compatible with old data

## Postconditions

- [ ] All LLM traffic routed through provider vtable
- [ ] Streaming works correctly with normalized events
- [ ] Messages saved with provider/model/thinking metadata
- [ ] New agents initialized with config defaults
- [ ] No direct OpenAI client calls remain
- [ ] All REPL tests pass
- [ ] `make check` passes
