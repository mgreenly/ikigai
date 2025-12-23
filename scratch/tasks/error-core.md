# Task: Common Error Utilities

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Provide complete context.

**Model:** sonnet/thinking
**Depends on:** provider-types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL HTTP operations MUST be non-blocking. Retry delays are implemented via the event loop's timeout mechanism, NOT blocking sleep() calls.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/error.h` - Existing error types
- `src/providers/provider.h` - Provider types including ik_error_category_t
- `src/openai/client_multi.c` - Reference implementation for async event loop integration

**Plan:**
- `scratch/plan/error-handling.md` - Error category definitions, retry flow, timer integration with event loop
- `scratch/plan/provider-interface.md` - timeout() method for retry delays
- `scratch/plan/README.md` - Critical constraint: select()-based event loop

## Objective

Implement shared error utilities for categorizing provider errors, checking retryability, generating user-facing error messages, and calculating retry delays. These utilities are used by all provider-specific error handlers.

**Important:** Retry logic in this async architecture works differently than traditional blocking code:
- Errors are delivered via completion callbacks, NOT as return values from `start_request()`/`start_stream()`
- Retry delays are NOT implemented with blocking `sleep()` or `usleep()` calls
- Instead, the provider's `timeout()` method returns the delay, and the event loop's `select()` honors it
- The provider tracks retry state internally and initiates retry during `perform()` when the timer expires

## Interface

### Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `ik_error_category_name` | `const char *(ik_error_category_t category)` | Convert category enum to string for logging |
| `ik_error_is_retryable` | `bool (ik_error_category_t category)` | Check if error category should be retried |
| `ik_error_user_message` | `char *(TALLOC_CTX *ctx, const char *provider, ik_error_category_t category, const char *detail)` | Generate user-facing error message |
| `ik_error_calc_retry_delay_ms` | `int64_t (int32_t attempt, int64_t provider_suggested_ms)` | Calculate retry delay for async retry via event loop |

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
- `IK_ERR_CAT_RATE_LIMIT` - retry with delay (use provider's suggested delay via `timeout()`)
- `IK_ERR_CAT_SERVER` - retry with exponential backoff (delay returned via `timeout()`)
- `IK_ERR_CAT_TIMEOUT` - retry immediately (minimal delay via `timeout()`)
- `IK_ERR_CAT_NETWORK` - retry with exponential backoff (delay returned via `timeout()`)

Return `false` for all other categories (AUTH, INVALID_ARG, NOT_FOUND, CONTENT_FILTER, UNKNOWN).

**Note:** When `ik_error_is_retryable()` returns true, the caller (provider adapter) should:
1. Store the error and retry state internally
2. Have `timeout()` return the calculated delay
3. On next `perform()` call after delay expires, initiate retry
4. Invoke the completion callback only after success or max retries exhausted

This integrates with the select()-based event loop without blocking.

### Retry Delay Calculation

`ik_error_calc_retry_delay_ms(attempt, provider_suggested_ms)` calculates the delay for async retry:

**Parameters:**
- `attempt`: Current retry attempt number (1, 2, or 3)
- `provider_suggested_ms`: Provider's suggested delay from error response (-1 if not provided)

**Algorithm:**
1. If `provider_suggested_ms > 0`: Use provider's suggested delay (e.g., from `retry-after` header)
2. Otherwise: Calculate exponential backoff with jitter:
   - Base delay: 1000ms * (2 ^ (attempt - 1)) = 1000ms, 2000ms, 4000ms
   - Add random jitter: 0-1000ms (prevents thundering herd)
   - Attempt 1: 1000ms + jitter(0-1000ms)
   - Attempt 2: 2000ms + jitter(0-1000ms)
   - Attempt 3: 4000ms + jitter(0-1000ms)

**Return value:** Delay in milliseconds (always positive)

**Usage by provider adapters:**
```c
// In provider's error handling after completion callback receives error:
if (ik_error_is_retryable(err->category) && retry_count < MAX_RETRIES) {
    retry_count++;
    retry_delay_ms = ik_error_calc_retry_delay_ms(retry_count, err->retry_after_ms);
    retry_at = now_ms() + retry_delay_ms;
    // Provider's timeout() will return (retry_at - now) until retry time
    // Provider's perform() will initiate retry when retry_at is reached
}
```

The delay is returned via the provider's `timeout()` method to the REPL's select() call. The REPL does NOT call sleep(); instead, select() naturally wakes after the timeout.

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

### Retry Delay Calculation Tests
- Provider suggested delay: When `provider_suggested_ms > 0`, returns that value exactly
- Exponential backoff: When `provider_suggested_ms <= 0`:
  - Attempt 1: delay is between 1000ms and 2000ms (base 1000ms + jitter 0-1000ms)
  - Attempt 2: delay is between 2000ms and 3000ms (base 2000ms + jitter 0-1000ms)
  - Attempt 3: delay is between 4000ms and 5000ms (base 4000ms + jitter 0-1000ms)
- Jitter distribution: Multiple calls with same attempt produce different results (randomness test)
- Always returns positive value (never 0 or negative)
- Provider suggested delay of -1 triggers exponential backoff
- Provider suggested delay of 0 triggers exponential backoff

## Postconditions

- [ ] `src/providers/common/error.h` exists with declarations
- [ ] `src/providers/common/error.c` implements all functions
- [ ] `ik_error_category_name()` returns correct strings for all categories
- [ ] `ik_error_is_retryable()` correctly identifies retryable categories
- [ ] `ik_error_user_message()` generates helpful messages
- [ ] `ik_error_calc_retry_delay_ms()` returns correct delays for event loop integration
- [ ] No blocking sleep() or usleep() calls in implementation
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