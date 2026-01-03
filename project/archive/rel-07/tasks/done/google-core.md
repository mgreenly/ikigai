# Task: Google Provider Core Structure

**Model:** sonnet/thinking
**Depends on:** provider-types.md, credentials-core.md, error-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

**Critical Architecture Constraint:** The application uses a select()-based event loop. ALL provider operations MUST be non-blocking:
- Use curl_multi (NOT curl_easy)
- Expose fdset() for select() integration
- Expose perform() for incremental processing
- NEVER block the main thread

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns

**Source:**
- `src/providers/provider.h` - Vtable and type definitions
- `src/providers/common/error.h` - Shared error utilities
- `src/credentials.h` - Credentials API

**Plan:**
- `scratch/plan/thinking-abstraction.md` - Thinking budget calculation
- `scratch/plan/provider-interface.md` - Async vtable specification (fdset/perform/timeout/info_read/start_request/start_stream)
- `scratch/plan/architecture.md` - Event loop integration pattern

## Objective

Create Google (Gemini) provider directory structure, headers, factory registration, and thinking budget calculation. This establishes the skeleton that subsequent tasks will fill in.

## Key Differences from Anthropic

| Aspect | Anthropic | Google |
|--------|-----------|--------|
| API Key | Header `x-api-key` | Query param `?key=` |
| Thinking | Token budget only | Budget (2.5) or Level (3.x) |
| Tool IDs | Provider generates | We must generate |
| Roles | user/assistant | user/model |

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out_provider)` | Create Google provider instance, returns OK/ERR |
| `ik_gemini_series_t ik_google_model_series(const char *model)` | Determine which Gemini series (2.5, 3, or OTHER) |
| `int32_t ik_google_thinking_budget(const char *model, ik_thinking_level_t level)` | Calculate thinking budget for Gemini 2.5 models in tokens, returns -1 if not applicable |
| `const char *ik_google_thinking_level_str(ik_thinking_level_t level)` | Get thinking level string ("LOW"/"HIGH") for Gemini 3 models, NULL if NONE |
| `bool ik_google_supports_thinking(const char *model)` | Check if model supports thinking mode |
| `bool ik_google_can_disable_thinking(const char *model)` | Check if thinking can be disabled for model |
| `res_t ik_google_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category)` | Parse error response, map to category, extract details |
| `int32_t ik_google_get_retry_after(const char *body)` | Extract retryDelay from response body JSON, returns seconds or -1 |
| `res_t ik_google_validate_thinking(const char *model, ik_thinking_level_t level)` | Validate thinking level for model, returns OK or ERR with message |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_gemini_series_t` | IK_GEMINI_2_5, IK_GEMINI_3, IK_GEMINI_OTHER | Enum for model series identification |
| `ik_google_budget_t` | model_pattern (string), min_budget (int32), max_budget (int32) | Model-specific thinking budget limits for Gemini 2.5 |
| `ik_google_ctx_t` | ctx (TALLOC_CTX*), api_key (string), base_url (string) | Internal provider implementation context |

## Behaviors

**Model Series Detection:**
- When model name contains "gemini-3", classify as IK_GEMINI_3
- When model name contains "gemini-2.5" or "gemini-2.0", classify as IK_GEMINI_2_5
- Otherwise classify as IK_GEMINI_OTHER
- NULL model names return IK_GEMINI_OTHER

**Thinking Budget Calculation (Gemini 2.5):**
- Match model against budget table patterns (gemini-2.5-pro, gemini-2.5-flash, gemini-2.5-flash-lite)
- Use model-specific min/max token budgets
- Map thinking levels to budget range: NONE=min, LOW=min+range/3, MED=min+2*range/3, HIGH=max
- gemini-2.5-pro: min=128, max=32768 (cannot disable)
- gemini-2.5-flash: min=0, max=24576 (can disable)
- gemini-2.5-flash-lite: min=512, max=24576 (cannot fully disable)
- Unknown 2.5 models default to min=0, max=24576

**Thinking Level String (Gemini 3):**
- NONE returns NULL (don't include config)
- LOW and MED both map to "LOW"
- HIGH maps to "HIGH"

**Thinking Validation:**
- `ik_google_validate_thinking(model, level)` validates model/level compatibility
- Gemini 2.5 models that can disable (min=0): All levels valid
- Gemini 2.5 models that cannot disable (min>0): NONE returns ERR, LOW/MED/HIGH valid
- Gemini 3 models: All levels valid (NONE means don't include thinking config)
- Non-thinking models (Gemini 1.5, etc.): Only NONE is valid, others return ERR
- NULL model: Return ERR(INVALID_ARG)
- Returns OK(NULL) if valid, ERR with descriptive message if invalid

**Provider Factory Registration:**
- Register "google" provider in ik_provider_create() dispatch
- Call ik_google_create() when provider name is "google"

**Error Handling:**

Error response format:
```json
{
  "error": {
    "code": 403,
    "message": "Your API key was reported as leaked...",
    "status": "PERMISSION_DENIED"
  }
}
```

Rate limit response includes retryDelay:
```json
{
  "error": {
    "code": 429,
    "status": "RESOURCE_EXHAUSTED",
    "message": "Quota exceeded for requests per minute"
  },
  "retryDelay": "60s"
}
```

HTTP Status to Category Mapping:

| HTTP Status | Provider Status | Category |
|-------------|-----------------|----------|
| 403 | `PERMISSION_DENIED` | `IK_ERR_CAT_AUTH` |
| 429 | `RESOURCE_EXHAUSTED` | `IK_ERR_CAT_RATE_LIMIT` |
| 400 | `INVALID_ARGUMENT` | `IK_ERR_CAT_INVALID_ARG` |
| 404 | `NOT_FOUND` | `IK_ERR_CAT_NOT_FOUND` |
| 500 | `INTERNAL` | `IK_ERR_CAT_SERVER` |
| 503 | `UNAVAILABLE` | `IK_ERR_CAT_SERVER` |
| 504 | `DEADLINE_EXCEEDED` | `IK_ERR_CAT_TIMEOUT` |

`ik_google_get_retry_after()` parses retryDelay string ("60s" format), extracts integer seconds.

**Directory Structure:**
- Create src/providers/google/ directory
- Files: google.h (public), google.c (factory+vtable), thinking.h (internal), thinking.c (implementation), error.h (internal), error.c (implementation)

**Vtable (Async Methods):**
- `fdset()` - Populate fd_sets for select() by calling curl_multi_fdset on provider's multi handle
- `perform()` - Process pending I/O by calling curl_multi_perform (non-blocking)
- `timeout()` - Get recommended timeout for select() from curl_multi_timeout
- `info_read()` - Check for completed transfers and invoke completion callbacks
- `start_request()` - Initiate non-streaming request (returns immediately, completion via callback)
- `start_stream()` - Initiate streaming request (returns immediately, events via callbacks)
- `cleanup` - NULL (talloc handles cleanup)

**Note:** Request implementations (`start_request` forwarding to ik_google_request_impl, `start_stream` forwarding to ik_google_stream_impl) are defined in google-request.md and google-streaming.md respectively.

**API Base URL:**
- Use https://generativelanguage.googleapis.com/v1beta

## Test Scenarios

**Model Series Detection:**
- gemini-2.5-pro returns IK_GEMINI_2_5
- gemini-2.5-flash returns IK_GEMINI_2_5
- gemini-2.0-flash returns IK_GEMINI_2_5
- gemini-3-pro returns IK_GEMINI_3
- gemini-1.5-pro returns IK_GEMINI_OTHER
- NULL returns IK_GEMINI_OTHER

**Thinking Budget Calculation:**
- gemini-2.5-pro with NONE returns 128 (minimum, cannot disable)
- gemini-2.5-pro with LOW returns 11008 (128 + 32640/3)
- gemini-2.5-pro with MED returns 21888 (128 + 2*32640/3)
- gemini-2.5-pro with HIGH returns 32768 (maximum)
- gemini-2.5-flash with NONE returns 0 (can disable)
- gemini-2.5-flash with MED returns 16384
- gemini-3-pro returns -1 (uses levels not budgets)

**Thinking Level Strings:**
- NONE returns NULL
- LOW returns "LOW"
- MED returns "LOW" (maps to LOW)
- HIGH returns "HIGH"

**Thinking Support:**
- gemini-2.5-pro supports thinking (true)
- gemini-3-pro supports thinking (true)
- gemini-1.5-pro does not support thinking (false)
- NULL does not support thinking (false)

**Disable Thinking:**
- gemini-2.5-pro cannot disable (min=128, returns false)
- gemini-2.5-flash can disable (min=0, returns true)
- gemini-2.5-flash-lite cannot disable (min=512, returns false)
- gemini-3-pro cannot disable (uses levels, returns false)

**Thinking Validation:**
- gemini-2.5-flash with NONE → OK (can disable thinking)
- gemini-2.5-flash with LOW/MED/HIGH → OK
- gemini-2.5-pro with NONE → ERR (cannot disable thinking, min=128)
- gemini-2.5-pro with LOW/MED/HIGH → OK
- gemini-3-pro with NONE → OK (NONE means don't send thinking config)
- gemini-3-pro with LOW/MED/HIGH → OK
- gemini-1.5-pro with NONE → OK
- gemini-1.5-pro with LOW → ERR (doesn't support thinking)
- NULL model → ERR(INVALID_ARG)

**Provider Creation:**
- Create with valid API key returns OK
- Provider name is "google"
- Vtable has async methods: fdset, perform, timeout, info_read, start_request, start_stream
- Implementation context contains copied api_key and base_url

**Factory Registration:**
- `ik_provider_create(ctx, "google", &provider)` dispatches to `ik_google_create()` (credentials loaded internally)

**Error Handling:**
- 403 status maps to IK_ERR_CAT_AUTH
- 429 status maps to IK_ERR_CAT_RATE_LIMIT
- 504 status maps to IK_ERR_CAT_TIMEOUT
- Parse error body: extract error.status and error.message
- Invalid JSON body returns ERR

**Retry-After:**
- Body with retryDelay "60s" returns 60
- Body with retryDelay "30s" returns 30
- Body without retryDelay returns -1
- Invalid JSON returns -1

## Postconditions

- [ ] `src/providers/google/` directory exists
- [ ] `google.h` declares `ik_google_create()`
- [ ] `thinking.h` and `thinking.c` implement budget/level calculation
- [ ] `error.h` declares `ik_google_handle_error()` and `ik_google_get_retry_after()`
- [ ] `error.c` implements error handling with correct status mappings
- [ ] `ik_google_model_series()` correctly identifies 2.5 vs 3 models
- [ ] `ik_google_thinking_budget()` returns correct values for 2.5 models
- [ ] `ik_google_thinking_level_str()` returns LOW/HIGH for 3 models
- [ ] `ik_google_create()` returns valid provider structure
- [ ] Provider registered in `ik_provider_create()` dispatch
- [ ] Makefile updated with new sources
- [ ] `make build/tests/unit/providers/google/thinking_test` succeeds
- [ ] All thinking tests pass
- [ ] All error handling tests pass
- [ ] Changes committed to git with message: `task: google-core.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).