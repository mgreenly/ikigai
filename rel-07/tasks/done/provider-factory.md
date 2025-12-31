# Task: Create Provider Factory

**Model:** sonnet/thinking
**Depends on:** http-client.md, sse-parser.md, credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

**Critical Architecture Constraint:** The application uses a select()-based event loop. All providers created by this factory MUST be async/non-blocking. Each provider initializes with a curl_multi handle for event loop integration.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns
- `/load errors` - Result types

**Source:**
- `src/credentials.h` - Credentials loading API
- `src/providers/provider.h` - Provider types and async vtable (from provider-types.md)

**Plan:**
- `scratch/plan/architecture.md` - Factory pattern and async provider initialization
- `scratch/plan/provider-interface.md` - Async vtable interface (fdset, perform, start_stream, etc.)

## Objective

Create `src/providers/factory.c` - the provider factory that dispatches to provider-specific factories based on provider name. Handles credential loading from environment variables or credentials.json, validates provider names, and creates appropriate provider instances with curl_multi handles for async operation.

## Interface

### Functions to Implement

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_provider_env_var` | `const char *(const char *provider)` | Get environment variable name for provider |
| `ik_provider_create` | `res_t (TALLOC_CTX *ctx, const char *name, ik_provider_t **out)` | Create provider instance with credentials |
| `ik_provider_is_valid` | `bool (const char *name)` | Check if provider name is valid |
| `ik_provider_list` | `const char **(void)` | Get NULL-terminated list of supported providers |

## Behaviors

### Environment Variable Mapping

Map provider names to environment variable names:
- "openai" → "OPENAI_API_KEY"
- "anthropic" → "ANTHROPIC_API_KEY"
- "google" → "GOOGLE_API_KEY"
- Unknown providers → NULL

### Provider Validation

Check if provider name is in supported list ("openai", "anthropic", "google"). Case-sensitive matching. NULL name returns false.

### Provider List

Return static NULL-terminated array of supported provider names. Array is constant, not allocated.

### Provider Creation

1. Validate provider name using `ik_provider_is_valid()`
2. Load credentials using `ik_credentials_load()`
3. Get API key for provider using `ik_credentials_get()`
4. If no credentials, return ERR mentioning environment variable
5. Dispatch to provider-specific factory:
   - "openai" → `ik_openai_create(ctx, api_key, out)`
   - "anthropic" → `ik_anthropic_create(ctx, api_key, out)`
   - "google" → `ik_google_create(ctx, api_key, out)`
6. Return result from factory

### Error Cases

- Unknown provider: ERR_INVALID_ARG with "Unknown provider: X"
- Missing credentials: ERR_INVALID_ARG with message mentioning environment variable and credentials file path
- Factory errors: propagate from provider-specific factory

### Memory Management

Provider instances allocated via talloc on provided context. Credentials loaded on temporary context during creation.

## Provider-Specific Factories

External functions (implemented in later tasks):

| Function | Task | Purpose |
|----------|------|---------|
| `ik_openai_create` | openai-core.md | Create OpenAI provider with curl_multi |
| `ik_anthropic_create` | anthropic-core.md | Create Anthropic provider with curl_multi |
| `ik_google_create` | google-core.md | Create Google provider with curl_multi |

These are declared as `extern` for forward reference. Linking succeeds only when provider implementations exist.

### Provider Requirements

Each provider-specific factory MUST:
1. Initialize a curl_multi handle for async HTTP operations
2. Populate the async vtable with these methods:
   - `fdset()` - Populate fd_sets for select() integration
   - `perform()` - Non-blocking I/O processing
   - `timeout()` - Get recommended select() timeout
   - `info_read()` - Process completed transfers, invoke callbacks
   - `start_request()` - Initiate non-streaming request (returns immediately)
   - `start_stream()` - Initiate streaming request (returns immediately)
   - `cleanup()` - Release resources (optional if talloc handles all cleanup)
3. Store the API key in provider context for request authorization

## Directory Structure

```
src/providers/
├── factory.h
└── factory.c

tests/unit/providers/
└── factory_test.c
```

## Test Scenarios

Create `tests/unit/providers/factory_test.c`:

- Environment variable mapping: Each provider maps to correct env var name
- Unknown provider env var: Returns NULL for unknown providers
- Provider validation: Valid names return true, invalid/NULL return false
- Provider list: All three providers present in list
- Unknown provider creation: Returns ERR_INVALID_ARG
- No credentials: Returns error mentioning environment variable
- Successful creation: (Integration test - requires provider implementations)

Note: Testing successful provider creation requires linking against provider-specific factories or stubs. Unit tests focus on validation, mapping, and error paths.

## Postconditions

- [ ] `src/providers/factory.h` exists with API
- [ ] `src/providers/factory.c` implements factory
- [ ] Makefile updated with new source/header
- [ ] `ik_provider_env_var()` returns correct env var names
- [ ] `ik_provider_is_valid()` validates provider names
- [ ] `ik_provider_list()` returns all supported providers
- [ ] `ik_provider_create()` returns error for missing credentials
- [ ] Compiles without warnings (may have unresolved symbols until provider implementations exist)
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: provider-factory.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).