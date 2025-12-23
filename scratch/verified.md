# Verified Fixes

Gaps identified during task review that have been fixed.

## 2025-12-23

### Error category enum prefix inconsistency

**Files:** `scratch/tasks/verify-foundation.md`

**Issue:** Smoke test code in Step 13 used `IK_ERR_AUTH` but enum table and plan docs use `IK_ERR_CAT_AUTH` prefix.

**Fix:** Changed line 269 from `IK_ERR_AUTH` to `IK_ERR_CAT_AUTH`.

### Vtable missing `cancel` method

**Files:** `scratch/tasks/provider-types.md`, `scratch/tasks/verify-foundation.md`

**Issue:** Plan doc `01-architecture/provider-interface.md` defines `cancel` method in vtable for Ctrl+C handling, but `provider-types.md` didn't include it.

**Fix:** Added `cancel` method to vtable definition in provider-types.md and verification checks in verify-foundation.md.

### Wrong struct name in verify-foundation Step 3

**Files:** `scratch/tasks/verify-foundation.md`

**Issue:** Step 3 listed `ik_http_completion_t` as a struct to verify in `provider.h`, but that struct belongs in `http_multi.h`. The members listed were actually for `ik_provider_completion_t`.

**Fix:** Changed `ik_http_completion_t` to `ik_provider_completion_t` in the struct verification list.

### Plan internal inconsistency: overview.md vs provider-interface.md

**Files:** `scratch/plan/01-architecture/overview.md`

**Issue:** Overview line 241 said completion callback receives `ik_http_completion_t`, but provider-interface.md defines `ik_provider_completion_cb_t` as taking `ik_provider_completion_t`.

**Fix:** Changed `ik_http_completion_t` to `ik_provider_completion_t` in overview.md.

### verify-foundation.md missing ik_http_completion_t check and wrong fields

**Files:** `scratch/tasks/verify-foundation.md`

**Issue:** Step 7 (HTTP Client API) didn't verify `ik_http_completion_t` struct. Step 12 referenced it with wrong fields (success, http_status, response - which are `ik_provider_completion_t` fields).

**Fix:** Added `ik_http_completion_t` check to Step 7 with correct fields per http-client.md. Updated Step 12 to reference Step 7 instead of listing wrong fields.

### Wrong completion type in provider interface tests

**Files:** `scratch/tasks/tests-openai-basic.md`, `scratch/tasks/tests-common-utilities.md`

**Issue:** tests-openai-basic.md said provider callbacks receive `ik_http_completion_t` but provider vtable callbacks deliver `ik_provider_completion_t`. tests-common-utilities.md example had wrong field names for `ik_http_completion_t`.

**Fix:** Changed tests-openai-basic.md to use `ik_provider_completion_t`. Fixed tests-common-utilities.md callback example to match actual `ik_http_completion_t` fields.

### Provider tasks incorrectly said "build ik_http_completion_t"

**Files:** `anthropic-streaming.md`, `openai-send-impl.md`, `openai-streaming-chat.md`, `openai-shim-send.md`, `openai-shim-response.md`

**Issue:** Provider implementation tasks said providers "build" or "create" `ik_http_completion_t`. But providers don't build it - the shared HTTP layer builds it. Providers RECEIVE `ik_http_completion_t` and convert to `ik_provider_completion_t`.

**Fix:** Updated all provider tasks to show correct flow:
- Shared HTTP layer delivers `ik_http_completion_t` to provider
- Provider parses response_body, maps errors
- Provider builds `ik_provider_completion_t`
- Provider invokes user callback with `ik_provider_completion_t`

### Finish reason enum name inconsistency

**Files:** `scratch/plan/05-testing/contract-anthropic.md`

**Issue:** contract-anthropic.md used `IK_FINISH_TOOL_CALLS` but the canonical enum in request-response.md defines `IK_FINISH_TOOL_USE`.

**Fix:** Changed `IK_FINISH_TOOL_CALLS` to `IK_FINISH_TOOL_USE` on line 160.

### Google finish reason IK_FINISH_SAFETY doesn't exist

**Files:** `scratch/plan/05-testing/contract-google.md`

**Issue:** contract-google.md referenced `IK_FINISH_SAFETY` for Google's SAFETY and RECITATION finish reasons, but this enum value doesn't exist in the canonical request-response.md. The existing `IK_FINISH_CONTENT_FILTER` has the same semantic meaning.

**Fix:** Changed `IK_FINISH_SAFETY` to `IK_FINISH_CONTENT_FILTER` on lines 177-178.
