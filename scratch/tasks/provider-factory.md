# Task: Create Provider Factory

**Layer:** 1
**Model:** sonnet/thinking
**Depends on:** http-client.md, sse-parser.md, credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load memory` - Talloc patterns
- `/load errors` - Result types

**Source:**
- `src/credentials.h` - Credentials loading API
- `src/providers/provider.h` - Provider types (from provider-types.md)

**Plan:**
- `scratch/plan/architecture.md` - Factory pattern reference

## Objective

Create `src/providers/provider_common.c` - the provider factory that dispatches to provider-specific factories based on provider name. Handles credential loading from environment variables or credentials.json, validates provider names, and creates appropriate provider instances.

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
| `ik_openai_create` | openai-core.md | Create OpenAI provider |
| `ik_anthropic_create` | anthropic-core.md | Create Anthropic provider |
| `ik_google_create` | google-core.md | Create Google provider |

These are declared as `extern` for forward reference. Linking succeeds only when provider implementations exist.

## Directory Structure

```
src/providers/
├── provider_common.h
└── provider_common.c

tests/unit/providers/
└── provider_common_test.c
```

## Test Scenarios

Create `tests/unit/providers/provider_common_test.c`:

- Environment variable mapping: Each provider maps to correct env var name
- Unknown provider env var: Returns NULL for unknown providers
- Provider validation: Valid names return true, invalid/NULL return false
- Provider list: All three providers present in list
- Unknown provider creation: Returns ERR_INVALID_ARG
- No credentials: Returns error mentioning environment variable
- Successful creation: (Integration test - requires provider implementations)

Note: Testing successful provider creation requires linking against provider-specific factories or stubs. Unit tests focus on validation, mapping, and error paths.

## Postconditions

- [ ] `src/providers/provider_common.h` exists with API
- [ ] `src/providers/provider_common.c` implements factory
- [ ] Makefile updated with new source/header
- [ ] `ik_provider_env_var()` returns correct env var names
- [ ] `ik_provider_is_valid()` validates provider names
- [ ] `ik_provider_list()` returns all supported providers
- [ ] `ik_provider_create()` returns error for missing credentials
- [ ] Compiles without warnings (may have unresolved symbols until provider implementations exist)
- [ ] Unit tests pass
- [ ] `make check` passes
