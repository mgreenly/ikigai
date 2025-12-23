# Task: Define OpenAI Shim Types

**Model:** sonnet/none
**Depends on:** provider-types.md, credentials-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load memory` - talloc ownership patterns
- `/load errors` - Result types

**Source:**
- `src/providers/provider.h` - Provider vtable and types (created by provider-types.md)
- `src/openai/client.h` - Existing OpenAI types to understand
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/architecture.md` - Migration Strategy section

## Objective

Define the shim context structure and factory function signature for the OpenAI adapter. This is a types-only task - no transformation logic, no HTTP calls. The shim context holds references to credentials and any state needed to bridge the new provider interface to the existing OpenAI code.

## Interface

### Structs to Define

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_openai_shim_ctx_t` | creds (ik_credentials_t*), api_key (char*), multi (ik_openai_multi_t*) | Holds OpenAI-specific context for vtable callbacks |

### Functions to Declare

| Function | Purpose |
|----------|---------|
| `res_t ik_openai_create(TALLOC_CTX *ctx, ik_credentials_t *creds, ik_provider_t **out)` | Factory function (declaration only, stub impl) |
| `void ik_openai_shim_destroy(void *impl_ctx)` | Cleanup shim context (declaration only, stub impl) |

### Files to Create

- `src/providers/openai/shim.h` - Shim context struct and function declarations
- `src/providers/openai/shim.c` - Stub implementations that return ERR or do nothing

## Behaviors

### Factory Function Stub
- Allocate shim context on provider's talloc context
- Obtain API key via `ik_credentials_get(creds, "openai")`
- Return ERR_MISSING_CREDENTIALS if key not found
- Store API key in context
- Initialize multi-handle: `ik_openai_multi_create(shim, &shim->multi)`
- Create provider with vtable pointing to stub send/stream functions
- Stub send/stream return `ERR(ctx, ERR_NOT_IMPLEMENTED, "Not yet implemented")`

### Destroy Function
- NULL-safe (no-op if ctx is NULL)
- talloc_free handles all cleanup

### Memory Management
- Shim context is child of provider
- API key string is child of shim context
- Single talloc_free on provider releases everything

## Test Scenarios

- Create shim context with valid credentials: succeeds, api_key populated
- Create shim context with missing credentials: returns ERR_MISSING_CREDENTIALS
- Destroy NULL context: no crash
- Destroy valid context: no memory leaks
- Call stub send(): returns ERR_NOT_IMPLEMENTED
- Call stub stream(): returns ERR_NOT_IMPLEMENTED

## Postconditions

- [ ] `src/providers/openai/shim.h` exists and compiles
- [ ] `src/providers/openai/shim.c` exists and compiles
- [ ] Factory function creates provider with valid vtable
- [ ] Missing credentials returns correct error
- [ ] Unit tests pass
- [ ] `make check` passes
- [ ] No changes to `src/openai/` files
- [ ] Changes committed to git with message: `task: openai-shim-types.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).