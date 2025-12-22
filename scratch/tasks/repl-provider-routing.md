# Task: Update REPL to Route via Provider Vtable

**Model:** sonnet/thinking
**Depends on:** openai-shim-send.md, agent-provider-fields.md, configuration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load source-code` - Map of REPL implementation
- `/load errors` - Result type patterns

**Source:**
- `src/repl_actions_llm.c` - LLM request handling (current implementation)
- `src/repl.c` - Agent initialization
- `src/agent.h` - Agent provider access

**Plan:**
- `scratch/plan/architecture.md` - Integration Points section

## Objective

Update REPL to route all LLM requests through the provider vtable instead of directly calling OpenAI. This task focuses on the request-side changes: building normalized requests, dispatching to providers, and initializing agents with correct defaults.

## Interface

Functions to update:

| Function | Signature | Changes |
|----------|-----------|---------|
| `ik_repl_send_llm_request` | `res_t (ik_repl_t *repl)` | Replace OpenAI calls with provider vtable dispatch |
| `ik_repl_init_agent` | `res_t (ik_repl_t *repl, ik_config_t *config)` | Apply defaults to new root agent |

Structs to update:

| Struct | Change |
|--------|--------|
| `ik_repl_t` | Remove OpenAI-specific client handle, use provider from agent context |

Files to update:

- `src/repl_actions_llm.c` - LLM request routing
- `src/repl.c` - Agent initialization

## Behaviors

### LLM Request Routing (`ik_repl_send_llm_request`)

1. Get provider from agent: `ik_agent_get_provider(agent, &provider)`
2. Build normalized `ik_request_t` from conversation state:
   - Set model from `agent->model`
   - Add system messages from `agent->system_prompt`
   - Add conversation history from replay context
   - Add tool definitions if any
   - Set `thinking_level` from `agent->thinking_level`
3. Call provider vtable: `provider->vt->stream(provider->impl_ctx, req, callback, ctx)`
4. Return errors from provider as-is

**Error Handling:**
- Return `ERR_MISSING_CREDENTIALS` if provider creation fails
- Return `ERR_INVALID_ARG` if agent is NULL
- Pass through provider vtable errors

### Agent Initialization (`ik_repl_init_agent`)

1. Call `ik_agent_apply_defaults(agent, config)` for new root agent
2. Agent gets `config->default_provider` on first creation
3. Forked agents already inherit from parent (handled in agent.c)
4. Restored agents already load from database (handled in agent.c)

### Remove OpenAI Client Handle

Before:
```c
typedef struct ik_repl {
    ik_openai_client_t *llm;  // Remove this
    // ...
} ik_repl_t;
```

After:
```c
typedef struct ik_repl {
    // Provider accessed via agent->provider
    // ...
} ik_repl_t;
```

### Migration Pattern

**Before (rel-06):**
```c
ik_openai_stream(repl->llm, messages, callback, ctx);
```

**After (rel-07):**
```c
ik_provider_t *provider = NULL;
TRY(ik_agent_get_provider(agent, &provider));

ik_request_t *req = NULL;
TRY(ik_request_create(ctx, agent->model, &req));
// ... build request ...

provider->vt->stream(provider->impl_ctx, req, callback, ctx);
```

## Test Scenarios

### Request Routing
- Send message through provider vtable: succeeds
- Provider returns response: passed to callback
- Missing credentials: returns `ERR_MISSING_CREDENTIALS`
- NULL agent: returns `ERR_INVALID_ARG`

### Agent Initialization
- New root agent: gets config default provider
- Config with "anthropic" default: agent uses anthropic
- Agent has provider field set correctly

### Request Building
- Request includes system prompt
- Request includes conversation history
- Request includes tools when defined
- Request includes thinking level

## Postconditions

- [ ] `ik_repl_send_llm_request()` uses provider vtable
- [ ] `ik_repl_t` no longer has OpenAI client handle
- [ ] New agents initialized with config defaults
- [ ] Request built correctly from conversation state
- [ ] No direct OpenAI client calls remain in routing
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: repl-provider-routing.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).