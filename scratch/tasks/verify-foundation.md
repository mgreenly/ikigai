# Task: Verify Foundation Stage

**VERIFICATION TASK:** This task does not create code. It verifies previous tasks completed correctly.

**Model:** sonnet/thinking
**Depends on:** provider-types.md, credentials-core.md, credentials-migrate.md, database-migration.md, configuration.md, http-client.md, sse-parser.md, provider-factory.md, request-builders.md, error-core.md

## Context

**Working directory:** Project root (where `Makefile` lives)

This is a **verification gate**. It has full agent context to read files, run commands, and analyze code. Its purpose is to catch issues before 40+ downstream tasks execute against a broken foundation.

If any verification step fails, return `{"ok": false, "blocked_by": "<specific failure>"}` to prevent downstream tasks from running.

## Pre-Read

Read these files to understand what was supposed to be created:
- `scratch/tasks/provider-types.md` - Expected types and vtable
- `scratch/tasks/credentials-core.md` - Expected credentials API
- `scratch/tasks/http-client.md` - Expected async HTTP client
- `scratch/tasks/sse-parser.md` - Expected SSE parser
- `scratch/tasks/error-core.md` - Expected error utilities
- `scratch/plan/architecture.md` - Async architecture constraint

## Verification Protocol

### Step 1: File Existence

Verify all expected files were created:

**Provider types:**
- [ ] `src/providers/provider.h` exists
- [ ] `src/providers/provider_types.h` exists (forward declarations)

**Credentials:**
- [ ] `src/credentials.h` exists
- [ ] `src/credentials.c` exists

**HTTP client:**
- [ ] `src/providers/common/http_multi.h` exists
- [ ] `src/providers/common/http_multi.c` exists

**SSE parser:**
- [ ] `src/providers/common/sse_parser.h` exists
- [ ] `src/providers/common/sse_parser.c` exists

**Error utilities:**
- [ ] `src/providers/error.h` exists
- [ ] `src/providers/error.c` exists

**Request builders:**
- [ ] `src/providers/request.h` exists
- [ ] `src/providers/request.c` exists

**Provider factory:**
- [ ] `src/providers/factory.h` exists
- [ ] `src/providers/factory.c` exists

**Database migration:**
- [ ] `migrations/005-provider-fields.sql` exists

**Configuration:**
- [ ] `etc/ikigai/credentials.example.json` exists

If any file is missing, report which file and which task should have created it.

### Step 2: Clean Compilation

```bash
make clean && make all 2>&1
```

- [ ] Exit code is 0
- [ ] Capture any warnings from new files

Grep the output for warnings in new directories:
```bash
make all 2>&1 | grep -E "(src/providers/|src/credentials)" | grep -i warning
```

- [ ] No warnings in new code (empty output)

If compilation fails, report the first error with file and line number.

### Step 3: Provider Types Symbol Verification

Read `src/providers/provider.h` and verify these symbols exist:

**Enums (7 required):**

| Enum | Required Values |
|------|-----------------|
| `ik_thinking_level_t` | IK_THINKING_NONE=0, IK_THINKING_LOW=1, IK_THINKING_MED=2, IK_THINKING_HIGH=3 |
| `ik_finish_reason_t` | IK_FINISH_STOP, IK_FINISH_LENGTH, IK_FINISH_TOOL_USE, IK_FINISH_CONTENT_FILTER, IK_FINISH_ERROR, IK_FINISH_UNKNOWN |
| `ik_content_type_t` | IK_CONTENT_TEXT, IK_CONTENT_TOOL_CALL, IK_CONTENT_TOOL_RESULT, IK_CONTENT_THINKING |
| `ik_role_t` | IK_ROLE_USER, IK_ROLE_ASSISTANT, IK_ROLE_TOOL |
| `ik_tool_choice_t` | IK_TOOL_AUTO, IK_TOOL_NONE, IK_TOOL_REQUIRED, IK_TOOL_SPECIFIC |
| `ik_error_category_t` | IK_ERR_AUTH, IK_ERR_RATE_LIMIT, IK_ERR_INVALID_ARG, IK_ERR_NOT_FOUND, IK_ERR_SERVER, IK_ERR_TIMEOUT, IK_ERR_CONTENT_FILTER, IK_ERR_NETWORK, IK_ERR_UNKNOWN |
| `ik_stream_event_type_t` | IK_STREAM_START, IK_STREAM_TEXT_DELTA, IK_STREAM_THINKING_DELTA, IK_STREAM_TOOL_CALL_START, IK_STREAM_TOOL_CALL_DELTA, IK_STREAM_TOOL_CALL_DONE, IK_STREAM_DONE, IK_STREAM_ERROR |

**Structs (11 required):**
- [ ] `ik_usage_t` - with input_tokens, output_tokens, thinking_tokens, cached_tokens, total_tokens
- [ ] `ik_thinking_config_t` - with level, include_summary
- [ ] `ik_content_block_t` - with type and union of text/tool_call/tool_result/thinking
- [ ] `ik_message_t` - with role, content blocks array
- [ ] `ik_tool_def_t` - with name, description, parameters, strict
- [ ] `ik_request_t` - with system_prompt, messages, model, thinking, tools, max_output_tokens
- [ ] `ik_response_t` - with content blocks, finish_reason, usage, model
- [ ] `ik_stream_event_t` - with type and union of event data
- [ ] `ik_http_completion_t` - with success, http_status, response, error_category, error_message, retry_after_ms
- [ ] `ik_provider_vtable_t` - with async methods (see Step 4)
- [ ] `ik_provider_t` - with name, vt, impl_ctx

**Callback types (2 required):**
- [ ] `ik_stream_cb_t` - signature: `res_t (*)(const ik_stream_event_t *, void *)`
- [ ] `ik_completion_cb_t` - signature: `res_t (*)(const ik_http_completion_t *, void *)`

### Step 4: Async Architecture Compliance

**CRITICAL:** The architecture requires ALL HTTP operations to be non-blocking.

Verify `ik_provider_vtable_t` has EXACTLY these methods:

**Event loop integration (4 required):**
- [ ] `fdset` - signature: `res_t (*)(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd)`
- [ ] `perform` - signature: `res_t (*)(void *ctx, int *running_handles)`
- [ ] `timeout` - signature: `res_t (*)(void *ctx, long *timeout_ms)`
- [ ] `info_read` - signature: `void (*)(void *ctx, ik_logger_t *logger)`

**Request initiation (2 required):**
- [ ] `start_request` - signature: `res_t (*)(void *ctx, const ik_request_t *req, ik_completion_cb_t cb, void *cb_ctx)`
- [ ] `start_stream` - signature: `res_t (*)(void *ctx, const ik_request_t *req, ik_stream_cb_t stream_cb, void *stream_ctx, ik_completion_cb_t completion_cb, void *completion_ctx)`

**Cleanup (1 required):**
- [ ] `cleanup` - signature: `void (*)(void *ctx)`

**Must NOT exist (blocking methods):**
- [ ] No `send` method
- [ ] No `stream` method (without `start_` prefix)

Grep for blocking patterns in provider code:
```bash
grep -rn "curl_easy_perform" src/providers/
grep -rn "sleep\s*(" src/providers/
grep -rn "usleep\s*(" src/providers/
```
- [ ] All three greps return empty (no blocking calls)

### Step 5: Error Code Verification

Read `src/error.h` and verify new error codes exist:

- [ ] `ERR_PROVIDER` is defined (value 9)
- [ ] `ERR_MISSING_CREDENTIALS` is defined (value 10)
- [ ] `ERR_NOT_IMPLEMENTED` is defined (value 11)

Read `src/error.c` and verify `error_code_str()` handles all three:
- [ ] Case for ERR_PROVIDER returns "Provider error"
- [ ] Case for ERR_MISSING_CREDENTIALS returns "Missing credentials"
- [ ] Case for ERR_NOT_IMPLEMENTED returns "Not implemented"

### Step 6: Credentials API Verification

Read `src/credentials.h` and verify:

- [ ] Function `ik_credentials_get()` is declared
- [ ] Signature accepts: context, provider name string, output pointer for API key
- [ ] Returns `res_t`

Verify credentials supports all three providers:
```bash
grep -E "(openai|anthropic|google)" src/credentials.c
```
- [ ] All three provider names appear in the source

### Step 7: HTTP Client API Verification

Read `src/providers/common/http_multi.h` and verify:

- [ ] `ik_http_multi_t` type is defined
- [ ] `ik_http_multi_create()` function declared
- [ ] `ik_http_multi_fdset()` function declared
- [ ] `ik_http_multi_perform()` function declared
- [ ] `ik_http_multi_timeout()` function declared
- [ ] `ik_http_multi_info_read()` function declared
- [ ] `ik_http_multi_add_request()` function declared

### Step 8: SSE Parser API Verification

Read `src/providers/common/sse_parser.h` and verify:

- [ ] `ik_sse_parser_t` type is defined
- [ ] `ik_sse_parser_create()` function declared
- [ ] `ik_sse_parser_feed()` function declared
- [ ] `ik_sse_parser_reset()` function declared
- [ ] Callback type for parsed events is defined

### Step 9: Provider Factory API Verification

Read `src/providers/factory.h` and verify:

- [ ] `ik_provider_create()` function declared
- [ ] Signature: `res_t ik_provider_create(TALLOC_CTX *ctx, const char *provider_name, const char *api_key, ik_provider_t **out)`
- [ ] Or similar signature that accepts provider name and returns provider instance

### Step 10: Request Builder API Verification

Read `src/providers/request.h` and verify:

- [ ] `ik_request_create()` function declared
- [ ] `ik_request_set_system()` function declared
- [ ] `ik_request_add_message()` function declared
- [ ] `ik_request_set_thinking()` function declared
- [ ] `ik_request_add_tool()` function declared
- [ ] `ik_response_create()` function declared

### Step 11: Database Schema Verification

Run database migration test:
```bash
make build/tests/unit/db/migration_test && ./build/tests/unit/db/migration_test
```
- [ ] Test passes

Verify schema version is 5:
```bash
grep "schema_version" migrations/005-provider-fields.sql
```
- [ ] Migration updates schema_version to 5

### Step 12: Cross-Task Contract Verification

Read downstream task files and verify their expectations are met:

**From `anthropic-core.md`:**
- Expects `ik_provider_vtable_t` with fdset, perform, start_stream → verify in provider.h
- Expects `ik_error_category_t` with AUTH, RATE_LIMIT → verify enum values

**From `http-client.md`:**
- Expects `ik_stream_cb_t` callback type → verify in provider.h
- Expects `ik_http_completion_t` struct → verify has success, http_status, response fields

**From `repl-provider-routing.md`:**
- Expects `ik_provider_t` with name, vt, impl_ctx → verify struct fields
- Expects `ik_request_build_from_conversation()` or similar → verify in request.h

**From `openai-shim-send.md`:**
- Expects `ik_provider_create()` → verify in factory.h

### Step 13: Integration Smoke Test

Create a minimal C file that uses all foundation APIs and verify it compiles:

```c
// /tmp/verify_foundation.c
#include "providers/provider.h"
#include "providers/factory.h"
#include "providers/request.h"
#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "credentials.h"
#include "error.h"

// Verify enums compile
static void check_enums(void) {
    ik_thinking_level_t t = IK_THINKING_NONE;
    ik_finish_reason_t f = IK_FINISH_STOP;
    ik_error_category_t e = IK_ERR_AUTH;
    ik_stream_event_type_t s = IK_STREAM_START;
    (void)t; (void)f; (void)e; (void)s;
}

// Verify callback signatures
static res_t my_stream_cb(const ik_stream_event_t *event, void *ctx) {
    (void)event; (void)ctx;
    return OK(NULL);
}

static res_t my_completion_cb(const ik_http_completion_t *completion, void *ctx) {
    (void)completion; (void)ctx;
    return OK(NULL);
}

static void check_callbacks(void) {
    ik_stream_cb_t scb = my_stream_cb;
    ik_completion_cb_t ccb = my_completion_cb;
    (void)scb; (void)ccb;
}

// Verify vtable structure
static void check_vtable(void) {
    ik_provider_vtable_t vt = {
        .fdset = NULL,
        .perform = NULL,
        .timeout = NULL,
        .info_read = NULL,
        .start_request = NULL,
        .start_stream = NULL,
        .cleanup = NULL
    };
    (void)vt;
}

int main(void) {
    check_enums();
    check_callbacks();
    check_vtable();
    return 0;
}
```

Write this file and compile:
```bash
gcc -c /tmp/verify_foundation.c -I src/ -o /tmp/verify_foundation.o 2>&1
```

- [ ] Compilation succeeds (exit code 0)
- [ ] No errors or warnings

### Step 14: Unit Tests Pass

Run all unit tests for foundation code:
```bash
make check-unit 2>&1
```

- [ ] All tests pass
- [ ] No test failures in providers/, credentials, or error tests

## Postconditions

- [ ] All 14 verification steps pass
- [ ] No compilation errors in new code
- [ ] No warnings in new code
- [ ] All expected symbols exist with correct signatures
- [ ] Async architecture constraint is enforced
- [ ] Cross-task contracts are satisfied

## Success Criteria

Return `{"ok": true}` if ALL verification steps pass.

Return `{"ok": false, "blocked_by": "<specific failure>"}` if any step fails.

**Failure report format:**
```json
{
  "ok": false,
  "blocked_by": "Step N: <step name> - <what was expected> vs <what was found>",
  "affected_downstream": ["task1.md", "task2.md", ...]
}
```

**Example failures:**

```json
{
  "ok": false,
  "blocked_by": "Step 4: Async Architecture - ik_provider_vtable_t missing 'info_read' method. Found: fdset, perform, timeout, start_request, start_stream, cleanup",
  "affected_downstream": ["anthropic-core.md", "google-core.md", "openai-core.md", "repl-provider-routing.md"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 1: File Existence - src/providers/common/sse_parser.h not found. Should have been created by sse-parser.md",
  "affected_downstream": ["anthropic-streaming.md", "google-streaming.md"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 13: Smoke Test - Compilation failed: src/providers/provider.h:45: error: unknown type name 'ik_stream_event_t'",
  "affected_downstream": ["ALL downstream tasks"]
}
```
