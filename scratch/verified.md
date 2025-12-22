# Verified Issues

## Verification Status (2025-12-22)

- **Dependency ordering:** All 62 tasks have correct ordering in order.json
- **Pre-Read references:** All plan files exist; source files are correctly expected to be created by earlier tasks
- **Previously fixed issues:** All 6 P0/P1 issues in verified.md are correctly resolved

---

Issues that have been checked and fixed.

## P0 Issues Fixed (2025-12-22)

### 1. Factory file path mismatch - FIXED

**Problem:** `provider-factory.md` created `src/providers/provider_common.h` and `src/providers/provider_common.c`, but 5 other tasks expected `src/providers/factory.h` and `src/providers/factory.c`.

**Affected tasks:**
- `verify-foundation.md:57-58,201`
- `verify-openai-shim.md:141`
- `tests-provider-core.md:28`
- `verify-providers.md:69`
- `cleanup-openai-adapter.md:49`

**Fix:** Updated `scratch/tasks/provider-factory.md` to create `factory.h` and `factory.c` instead of `provider_common.*`.

### 2. Error file path mismatch - FIXED

**Problem:** `error-core.md` creates files at `src/providers/common/error.h` and `src/providers/common/error.c`, but `verify-foundation.md` expected them at `src/providers/error.h` and `src/providers/error.c`.

**Fix:** Updated `scratch/tasks/verify-foundation.md` Step 1 to expect `src/providers/common/error.h` and `src/providers/common/error.c`.

### 3. verify-infrastructure.md ordering - FIXED

**Problem:** `verify-infrastructure.md` was at position 20 but depended on tasks at LATER positions:
- `tests-mock-infrastructure.md` (position 17)
- `tests-common-utilities.md` (position 18)
- `tests-provider-core.md` (position 19)

**Fix:** Moved `verify-infrastructure.md` to position 21 in `scratch/tasks/order.json`, after all its dependencies.

## P1 Issues Fixed (2025-12-22)

### 4. Callback return type mismatch - FIXED

**Problem:** Plan defines callbacks with `res_t` return type, but test tasks used `void` return type.

**Affected tasks:**
- `tests-anthropic-streaming.md`
- `tests-google-basic.md`
- `tests-openai-streaming.md`
- `tests-common-utilities.md`

**Fix:** Updated all test callbacks to return `res_t` with `return OK(NULL);` and use proper `ik_provider_completion_t` parameter structure.

### 5. HTTP/Provider completion struct type confusion - FIXED

**Problem:** `http-client.md` used `ik_provider_completion_cb_t` (provider-level type) instead of an HTTP-specific callback type.

**Fix:** Updated `scratch/tasks/http-client.md` to use:
- New type `ik_http_completion_cb_t` with signature `void (*)(const ik_http_completion_t *completion, void *ctx)`
- Updated `ik_http_multi_add_request` to use `ik_http_completion_cb_t`

This creates clean separation: HTTP layer uses `ik_http_completion_t`/`ik_http_completion_cb_t`, provider layer uses `ik_provider_completion_t`/`ik_provider_completion_cb_t`.

### 6. Error enum naming prefix mismatch - FIXED

**Problem:** `error-core.md` used `IK_ERR_CAT_*` prefix but `provider-types.md` defined `IK_ERR_*` (without `CAT_`).

**Fix:** Updated `scratch/tasks/error-core.md` to use consistent naming:
- `IK_ERR_CAT_AUTH` → `IK_ERR_AUTH`
- `IK_ERR_CAT_RATE_LIMIT` → `IK_ERR_RATE_LIMIT`
- `IK_ERR_CAT_INVALID_ARG` → `IK_ERR_INVALID_ARG`
- `IK_ERR_CAT_NOT_FOUND` → `IK_ERR_NOT_FOUND`
- `IK_ERR_CAT_SERVER` → `IK_ERR_SERVER`
- `IK_ERR_CAT_TIMEOUT` → `IK_ERR_TIMEOUT`
- `IK_ERR_CAT_CONTENT_FILTER` → `IK_ERR_CONTENT_FILTER`
- `IK_ERR_CAT_NETWORK` → `IK_ERR_NETWORK`
- `IK_ERR_CAT_UNKNOWN` → `IK_ERR_UNKNOWN`

## Previously Fixed (from fixed.md)

- `verify-providers.md` ordering in `order.json` is correct (moved to position 58, after all test creation tasks)
- `ik_provider_completion_t` type name avoids collision with existing `ik_http_completion_t` in `src/openai/client_multi.h`
- Callback type standardized to `ik_provider_completion_cb_t` across all task and plan files
- `ERR_AGENT_NOT_FOUND` case added to `error_code_str()` switch in `src/error.h`
- `verify-foundation.md` declares dependencies on all credential test tasks
- `order.json` structure is valid (no duplicates, all files exist, valid model/thinking values)
- All `/load` skill references in Pre-Read sections point to existing skills
- All `scratch/plan/*.md` references in Pre-Read sections point to existing files

## P2 Issues Fixed (2025-12-22)

### 7. ik_provider_create signature mismatch - FIXED

**Problem:** Several tasks expected 4-param signature with `api_key`, but `provider-factory.md` uses 3-param (credentials loaded internally via `ik_credentials_get()`).

**Affected tasks:**
- `verify-foundation.md:204`
- `verify-openai-shim.md:244`
- `tests-integration-flows.md:223`
- `openai-core.md:196`
- `google-core.md:201`

**Fix:** Updated all 5 files to use 3-param signature: `ik_provider_create(ctx, name, &out)`

### 8. Test callback type confusion - FIXED

**Problem:** Test files used `ik_http_completion_t` (HTTP-layer) in callbacks, but provider vtable delivers `ik_provider_completion_t` (provider-layer).

**Affected tasks:**
- `tests-anthropic-basic.md:54,57`
- `tests-openai-basic.md:85`
- `tests-google-streaming.md:155`
- `tests-integration-flows.md:208`
- `tests-integration-switching.md:194`

**Fix:** Updated all 5 files to use `ik_provider_completion_t` in test callbacks. Also updated field access patterns to use `completion->success` and `completion->response` (provider-layer fields).
