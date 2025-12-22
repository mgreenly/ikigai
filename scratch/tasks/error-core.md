# Task: Common Error Utilities

**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/error.h` - Existing error types
- `src/providers/provider.h` - Provider types including ik_error_category_t

**Plan:**
- `scratch/plan/error-handling.md` - Error category definitions and user messages

## Objective

Implement shared error utilities for categorizing provider errors, checking retryability, and generating user-facing error messages. These utilities are used by all provider-specific error handlers.

## Interface

### Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_error_category_name` | `const char *(ik_error_category_t category)` | Convert category enum to string for logging |
| `ik_error_is_retryable` | `bool (ik_error_category_t category)` | Check if error category should be retried |
| `ik_error_user_message` | `char *(TALLOC_CTX *ctx, const char *provider, ik_error_category_t category, const char *detail)` | Generate user-facing error message |

## Behaviors

### Category to String Mapping

Return static string literals (no allocation needed):

| Category | String |
|----------|--------|
| `IK_ERR_CAT_AUTH` | "authentication" |
| `IK_ERR_CAT_RATE_LIMIT` | "rate_limit" |
| `IK_ERR_CAT_INVALID_ARG` | "invalid_argument" |
| `IK_ERR_CAT_NOT_FOUND` | "not_found" |
| `IK_ERR_CAT_SERVER` | "server_error" |
| `IK_ERR_CAT_TIMEOUT` | "timeout" |
| `IK_ERR_CAT_CONTENT_FILTER` | "content_filter" |
| `IK_ERR_CAT_NETWORK` | "network_error" |
| `IK_ERR_CAT_UNKNOWN` | "unknown" |

### Retryable Categories

Return `true` for categories that benefit from retry:
- `IK_ERR_CAT_RATE_LIMIT` - retry with delay
- `IK_ERR_CAT_SERVER` - retry with backoff
- `IK_ERR_CAT_TIMEOUT` - retry immediately
- `IK_ERR_CAT_NETWORK` - retry with backoff

Return `false` for all other categories (AUTH, INVALID_ARG, NOT_FOUND, CONTENT_FILTER, UNKNOWN).

### User Message Generation

Generate helpful messages based on category. Use talloc_asprintf on provided context.

**AUTH:**
```
Authentication failed for {provider}. Check your API key in {ENV_VAR} or ~/.config/ikigai/credentials.json
```
Where ENV_VAR is derived from provider name (ANTHROPIC_API_KEY, OPENAI_API_KEY, GOOGLE_API_KEY).

**RATE_LIMIT:**
```
Rate limit exceeded for {provider}. {detail}
```

**INVALID_ARG:**
```
Invalid request to {provider}: {detail}
```

**NOT_FOUND:**
```
Model not found on {provider}: {detail}
```

**SERVER:**
```
{provider} server error. This is temporary, retrying may succeed. {detail}
```

**TIMEOUT:**
```
Request to {provider} timed out. Check network connection.
```

**CONTENT_FILTER:**
```
Content blocked by {provider} safety filters: {detail}
```

**NETWORK:**
```
Network error connecting to {provider}: {detail}
```

**UNKNOWN:**
```
{provider} error: {detail}
```

If detail is NULL or empty, omit the detail portion.

### Memory Management

- `ik_error_category_name()` returns static strings, no allocation
- `ik_error_user_message()` allocates on provided talloc context

## Directory Structure

```
src/providers/common/
├── error.h      - Function declarations
└── error.c      - Implementation

tests/unit/providers/common/
└── error_test.c - Unit tests
```

## Test Scenarios

### Category Name Tests
- All 9 categories map to expected strings
- Unknown/invalid category returns "unknown"

### Retryability Tests
- RATE_LIMIT, SERVER, TIMEOUT, NETWORK return true
- AUTH, INVALID_ARG, NOT_FOUND, CONTENT_FILTER, UNKNOWN return false

### User Message Tests
- AUTH message includes provider name and env var hint
- RATE_LIMIT message includes provider and detail
- NULL detail produces clean message without trailing colon
- Empty detail ("") treated same as NULL
- All categories produce non-NULL messages
- Messages allocated on provided context (verify with talloc_get_size)

## Postconditions

- [ ] `src/providers/common/error.h` exists with declarations
- [ ] `src/providers/common/error.c` implements all functions
- [ ] `ik_error_category_name()` returns correct strings for all categories
- [ ] `ik_error_is_retryable()` correctly identifies retryable categories
- [ ] `ik_error_user_message()` generates helpful messages
- [ ] Makefile updated with new sources
- [ ] Compiles without warnings
- [ ] All unit tests pass
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: error-core.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).