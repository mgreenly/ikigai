# Task: Anthropic Provider Core Structure

**Model:** sonnet/thinking
**Depends on:** provider-types.md, credentials-core.md, error-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - Talloc-based memory management

**Source:**
- `src/providers/provider.h` - Vtable and type definitions
- `src/providers/common/error.h` - Shared error utilities
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/README.md` - Critical constraint: select()-based event loop, ALL HTTP must be non-blocking
- `scratch/plan/provider-interface.md` - Complete async vtable specification with fdset/perform/timeout/info_read pattern
- `scratch/plan/architecture.md` - Anthropic provider responsibilities and event loop integration
- `scratch/plan/configuration.md` - Provider architecture and factory pattern

## Objective

Create Anthropic provider directory structure, headers, factory registration, and thinking budget calculation. This establishes the skeleton that subsequent tasks will fill in with request serialization, response parsing, and streaming implementations.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out_provider)` | Creates Anthropic provider instance, returns OK with provider or ERR on failure |
| `int32_t ik_anthropic_thinking_budget(const char *model, ik_thinking_level_t level)` | Calculates thinking budget tokens for given model and level, returns -1 if unsupported |
| `bool ik_anthropic_supports_thinking(const char *model)` | Checks if model supports extended thinking |
| `res_t ik_anthropic_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category)` | Parse error response, map to category, extract details |
| `int32_t ik_anthropic_get_retry_after(const char **headers)` | Extract retry-after header value in seconds, returns -1 if not present |
| `res_t ik_anthropic_validate_thinking(const char *model, ik_thinking_level_t level)` | Validate thinking level for model, returns OK or ERR with message |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_anthropic_ctx_t` | ctx, api_key, base_url | Provider implementation context, stores configuration |
| `ik_anthropic_budget_t` | model_pattern, min_budget, max_budget | Model-specific thinking budget limits |

## Behaviors

### Provider Creation
- When `ik_anthropic_create()` is called, allocate provider and implementation context
- Set base_url to "https://api.anthropic.com"
- Copy API key into implementation context
- Initialize vtable with function pointers for async event loop integration:
  - `fdset()` - populate fd_sets for select()
  - `perform()` - process pending I/O (non-blocking)
  - `timeout()` - get recommended select() timeout
  - `info_read()` - check for completed transfers
  - `start_request()` - initiate non-streaming request (returns immediately)
  - `start_stream()` - initiate streaming request (returns immediately)
- Return OK with fully configured provider

### Thinking Budget Calculation
- When `ik_anthropic_thinking_budget()` is called, find budget limits for model
- For NONE level: return min_budget (1024 tokens)
- For LOW level: return min_budget + range/3
- For MED level: return min_budget + 2*range/3
- For HIGH level: return max_budget
- Use model prefix matching: "claude-sonnet-4-5" → 64000 max, "claude-haiku-4-5" → 32000 max
- Default to 32000 max for unknown models
- Return -1 if model doesn't support thinking

### Factory Registration
- Register provider in `ik_provider_create()` dispatch table
- When name is "anthropic", call `ik_anthropic_create()`
- Return ERR(INVALID_ARG) for unknown provider names

### Thinking Validation
- `ik_anthropic_validate_thinking(model, level)` validates model/level compatibility
- Claude models that support thinking: All levels valid (NONE/LOW/MED/HIGH)
- Claude models that don't support thinking: Only NONE is valid, others return ERR
- Non-Claude models: Only NONE is valid, others return ERR
- NULL model: Return ERR(INVALID_ARG)
- Returns OK(NULL) if valid, ERR with descriptive message if invalid

### Error Handling

**Error Response Format:**
```json
{
  "type": "error",
  "error": {
    "type": "rate_limit_error",
    "message": "Your request was rate-limited"
  }
}
```

**HTTP Status to Category Mapping:**

| HTTP Status | Provider Type | Category |
|-------------|---------------|----------|
| 401 | `authentication_error` | `IK_ERR_CAT_AUTH` |
| 403 | `permission_error` | `IK_ERR_CAT_AUTH` |
| 429 | `rate_limit_error` | `IK_ERR_CAT_RATE_LIMIT` |
| 400 | `invalid_request_error` | `IK_ERR_CAT_INVALID_ARG` |
| 404 | `not_found_error` | `IK_ERR_CAT_NOT_FOUND` |
| 500 | `api_error` | `IK_ERR_CAT_SERVER` |
| 529 | `overloaded_error` | `IK_ERR_CAT_SERVER` |

**`ik_anthropic_handle_error()` Behavior:**
- Parse JSON body using yyjson
- Extract `error.type` and `error.message` fields
- Map HTTP status to category using table above
- Return OK with category set, or ERR if body parsing fails

**`ik_anthropic_get_retry_after()` Behavior:**
- Scan headers array for "retry-after: N" (case-insensitive key)
- Parse integer value (seconds)
- Return parsed value, or -1 if header not found

### Directory Structure
Create the following files:
- `src/providers/anthropic/anthropic.h` - Public interface
- `src/providers/anthropic/anthropic.c` - Factory and vtable
- `src/providers/anthropic/thinking.h` - Thinking budget API (internal)
- `src/providers/anthropic/thinking.c` - Thinking budget implementation
- `src/providers/anthropic/error.h` - Error handling API (internal)
- `src/providers/anthropic/error.c` - Error handling implementation

### Vtable
- The async vtable functions are forward-declared but not fully implemented:
  - `fdset()`, `perform()`, `timeout()`, `info_read()` - event loop integration
  - `start_request()` - non-blocking request initiation (replaces blocking `send()`)
  - `start_stream()` - non-blocking stream initiation (replaces blocking `stream()`)
- They will be implemented in subsequent tasks (anthropic-request.md, anthropic-streaming.md)

## Test Scenarios

### Thinking Budget Tests
- Sonnet 4.5 with NONE level returns 1024
- Sonnet 4.5 with LOW level returns 22016 (1024 + 62976/3)
- Sonnet 4.5 with MED level returns 43008 (1024 + 2*62976/3)
- Sonnet 4.5 with HIGH level returns 64000
- Haiku 4.5 with HIGH level returns 32000
- Unknown model with HIGH level returns 32000 (default)
- Non-Claude model returns -1

### Support Tests
- `ik_anthropic_supports_thinking("claude-sonnet-4-5")` returns true
- `ik_anthropic_supports_thinking("claude-opus-4-5-20251101")` returns true
- `ik_anthropic_supports_thinking("gpt-4o")` returns false
- `ik_anthropic_supports_thinking(NULL)` returns false

### Provider Creation Tests
- Create provider with valid API key returns OK
- Provider has name "anthropic"
- Provider has non-NULL vtable with async methods (fdset, perform, timeout, info_read, start_request, start_stream)
- Provider has non-NULL impl_ctx
- Talloc cleanup frees all resources

### Factory Registration Tests
- `ik_provider_create()` with "anthropic" calls `ik_anthropic_create()`
- Factory returns valid provider
- Unknown provider name returns ERR(INVALID_ARG)

### Thinking Validation Tests
- claude-sonnet-4-5 with NONE → OK
- claude-sonnet-4-5 with LOW/MED/HIGH → OK (supports thinking)
- claude-3-opus with NONE → OK
- claude-3-opus with LOW → ERR (older model doesn't support thinking)
- gpt-4o with NONE → OK (non-Claude, but NONE is always valid)
- gpt-4o with LOW → ERR (non-Claude model doesn't support Anthropic thinking)
- NULL model → ERR(INVALID_ARG)

### Error Handling Tests
- 401 status maps to IK_ERR_CAT_AUTH
- 429 status maps to IK_ERR_CAT_RATE_LIMIT
- 500 status maps to IK_ERR_CAT_SERVER
- 529 status maps to IK_ERR_CAT_SERVER (overloaded)
- Parse error body: extract error.type and error.message
- Invalid JSON body returns ERR

### Retry-After Tests
- Header "retry-after: 20" returns 20
- Header "Retry-After: 60" returns 60 (case-insensitive)
- Missing header returns -1
- Empty headers array returns -1

## Postconditions

- [ ] `src/providers/anthropic/` directory exists
- [ ] `anthropic.h` declares `ik_anthropic_create()`
- [ ] `thinking.h` declares budget calculation functions
- [ ] `thinking.c` implements budget calculation with correct values for all levels
- [ ] `error.h` declares `ik_anthropic_handle_error()` and `ik_anthropic_get_retry_after()`
- [ ] `error.c` implements error handling with correct status mappings
- [ ] `ik_anthropic_create()` returns valid provider structure
- [ ] Provider registered in `ik_provider_create()` dispatch (in provider_common.c)
- [ ] Makefile updated with new sources
- [ ] Compiles without warnings
- [ ] All thinking budget tests pass
- [ ] All provider creation tests pass
- [ ] All error handling tests pass
- [ ] Changes committed to git with message: `task: anthropic-core.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).