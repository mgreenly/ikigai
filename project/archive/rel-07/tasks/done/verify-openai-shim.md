# Task: Verify OpenAI Shim Stage

**VERIFICATION TASK:** This task does not create code. It verifies previous tasks completed correctly.

**Model:** sonnet/thinking
**Depends on:** verify-infrastructure.md, openai-shim-types.md, openai-shim-request.md, openai-shim-response.md, openai-shim-send.md, openai-shim-compat-tests.md

## Context

**Working directory:** Project root (where `Makefile` lives)

This verification gate ensures the OpenAI shim (adapter wrapping legacy code) works correctly before we build on it. The shim is critical for maintaining backward compatibility during migration.

**Key insight:** The shim wraps the existing `src/openai/` code with the new provider vtable interface. All existing OpenAI functionality must continue to work.

If any verification step fails, return `{"ok": false, "blocked_by": "<specific failure>"}`.

## Pre-Read

Read these files to understand the shim design:
- `scratch/tasks/openai-shim-types.md` - Shim type definitions
- `scratch/tasks/openai-shim-send.md` - Vtable implementation
- `scratch/plan/architecture.md` - Migration strategy

## Verification Protocol

### Step 1: Shim Files Exist

**Shim adapter:**
- [ ] `src/providers/openai/shim.h` exists (or similar naming)
- [ ] `src/providers/openai/shim.c` exists
- [ ] Or the shim is in `src/providers/openai/adapter.c`

**Shim must NOT duplicate functionality:**
- [ ] Shim imports from `src/openai/` (legacy code)
- [ ] Shim does not reimplement HTTP handling

Verify shim delegates to legacy code:
```bash
grep -n "#include.*openai/" src/providers/openai/*.c
```
- [ ] At least one include of legacy openai headers

### Step 2: Shim Compiles

```bash
make all 2>&1 | grep -E "providers/openai"
```

- [ ] Shim files compile without errors
- [ ] No warnings in shim code

### Step 3: Shim Vtable Implementation

Read shim source and verify vtable is fully implemented:

```bash
grep -n "ik_provider_vtable_t\|\.fdset\|\.perform\|\.timeout\|\.info_read\|\.start_request\|\.start_stream" src/providers/openai/*.c
```

- [ ] Vtable struct is defined with all 7 methods
- [ ] `fdset` delegates to `ik_openai_multi_fdset` or similar
- [ ] `perform` delegates to `ik_openai_multi_perform` or similar
- [ ] `timeout` delegates to `ik_openai_multi_timeout` or similar
- [ ] `info_read` delegates to `ik_openai_multi_info_read` or similar
- [ ] `start_request` wraps legacy request building
- [ ] `start_stream` wraps legacy streaming

### Step 4: Async Pattern Preserved

Verify shim maintains async behavior (doesn't introduce blocking):

```bash
grep -n "curl_easy_perform\|sleep\|usleep" src/providers/openai/*.c
```

- [ ] No blocking calls in shim code

Verify shim uses curl_multi (not curl_easy):
```bash
grep -n "curl_multi\|ik_openai_multi" src/providers/openai/*.c
```
- [ ] References to curl_multi or legacy multi-handle functions

### Step 5: Request Transformation

Verify shim transforms `ik_request_t` to legacy format:

Read shim and check for transformation:
```bash
grep -n "ik_request_t\|ik_cfg_t\|openai_model\|openai_temperature" src/providers/openai/*.c
```

- [ ] Shim accepts `ik_request_t` as input
- [ ] Shim builds `ik_cfg_t` or equivalent for legacy code
- [ ] Model name is extracted from request
- [ ] System prompt is handled correctly

### Step 6: Response Transformation

Verify shim transforms legacy response to `ik_response_t`:

```bash
grep -n "ik_response_t\|ik_content_block\|ik_finish_reason" src/providers/openai/*.c
```

- [ ] Shim creates `ik_response_t` from legacy response
- [ ] Content blocks are populated
- [ ] Finish reason is mapped correctly
- [ ] Usage statistics are extracted

### Step 7: Streaming Event Transformation

Verify shim transforms legacy streaming to normalized events:

```bash
grep -n "ik_stream_event_t\|IK_STREAM_\|stream_cb" src/providers/openai/*.c
```

- [ ] Shim emits `ik_stream_event_t` events
- [ ] Events use normalized types (IK_STREAM_TEXT_DELTA, etc.)
- [ ] Stream callback is invoked during perform()

### Step 8: Error Mapping

Verify shim maps legacy errors to `ik_error_category_t`:

```bash
grep -n "ik_error_category_t\|IK_ERR_\|error_category" src/providers/openai/*.c
```

- [ ] HTTP 401 maps to IK_ERR_CAT_AUTH
- [ ] HTTP 429 maps to IK_ERR_CAT_RATE_LIMIT
- [ ] HTTP 500+ maps to IK_ERR_CAT_SERVER

### Step 9: Factory Registration

Verify shim is registered with provider factory:

```bash
grep -n "openai" src/providers/factory.c
```

- [ ] Factory recognizes "openai" provider name
- [ ] Factory creates shim instance for "openai"

### Step 10: Legacy Tests Still Pass

**CRITICAL:** Existing OpenAI tests must still pass.

Run legacy OpenAI tests:
```bash
make check-unit 2>&1 | grep -E "openai.*PASS|openai.*FAIL|client_multi|client_http"
```

- [ ] All legacy `tests/unit/openai/` tests pass
- [ ] No regressions in existing functionality

Count legacy test results:
```bash
make check-unit 2>&1 | grep -c "tests/unit/openai.*PASS"
```
- [ ] All expected tests pass (should match pre-shim count)

### Step 11: Shim Compatibility Tests Pass

Run shim-specific compatibility tests:
```bash
make build/tests/unit/providers/openai/shim_compat_test && ./build/tests/unit/providers/openai/shim_compat_test
```

- [ ] All compatibility tests pass

Verify tests cover key scenarios:
```bash
grep -c "START_TEST" tests/unit/providers/openai/*shim*test*.c 2>/dev/null || grep -c "START_TEST" tests/unit/providers/openai/*compat*test*.c
```
- [ ] Multiple test cases exist

### Step 12: Vtable Equivalence Test

Verify shim vtable produces same results as direct legacy calls:

Look for equivalence tests:
```bash
grep -n "equivalent\|same.*result\|legacy.*match" tests/unit/providers/openai/*.c
```

- [ ] Tests compare shim output to legacy output
- [ ] Tests verify streaming events match
- [ ] Tests verify response structure matches

### Step 13: Memory Management Verification

Verify shim properly manages memory:

```bash
grep -n "talloc_free\|talloc_steal\|TALLOC_CTX" src/providers/openai/*.c
```

- [ ] Shim uses talloc for allocations
- [ ] Contexts are properly parented
- [ ] No memory leaks in transformation code

Run with valgrind if available:
```bash
make BUILD=valgrind check-unit 2>&1 | grep -E "providers/openai|definitely lost"
```
- [ ] No memory leaks reported

### Step 14: Callback Context Handling

Verify shim preserves callback contexts correctly:

```bash
grep -n "cb_ctx\|user_ctx\|callback.*ctx" src/providers/openai/*.c
```

- [ ] User context is passed through to callbacks
- [ ] Context is not freed prematurely
- [ ] Stream and completion callbacks receive correct context

### Step 15: End-to-End Shim Test

Verify complete request/response cycle through shim:

Look for integration test:
```bash
grep -A 30 "test.*end_to_end\|test.*full_cycle\|test.*complete_request" tests/unit/providers/openai/*.c
```

Test should:
- [ ] Create provider via factory with "openai"
- [ ] Build `ik_request_t`
- [ ] Call `start_request` or `start_stream`
- [ ] Drive event loop (fdset/perform/info_read)
- [ ] Verify callback receives valid `ik_response_t`

### Step 16: Cross-Task Contract Verification

Verify shim meets expectations of downstream tasks:

**From `agent-provider-fields.md`:**
- Expects `ik_provider_create(ctx, "openai", &provider)` to work (credentials loaded internally)
- Verify: Factory creates working shim for "openai"

**From `repl-provider-routing.md`:**
- Expects provider vtable to be callable
- Verify: All vtable methods are non-NULL and callable

**From `openai-equivalence-validation.md`:**
- Expects shim to produce identical results to legacy code
- Verify: Equivalence tests pass

## Postconditions

- [ ] All 16 verification steps pass
- [ ] Shim compiles without warnings
- [ ] Shim implements complete vtable
- [ ] Shim maintains async behavior
- [ ] All legacy OpenAI tests still pass
- [ ] Shim compatibility tests pass
- [ ] No memory leaks
- [ ] Callbacks work correctly

## Success Criteria

Return `{"ok": true}` if ALL verification steps pass.

Return `{"ok": false, "blocked_by": "<specific failure>"}` if any step fails.

**Example failures:**

```json
{
  "ok": false,
  "blocked_by": "Step 10: Legacy Tests - 3 legacy OpenAI tests now fail after shim introduction. Failures: test_client_multi_add_request, test_http_handler_streaming, test_sse_parser_chunks",
  "affected_downstream": ["ALL tasks - shim breaks existing functionality"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 3: Shim Vtable - info_read method is NULL in vtable. Shim only implements fdset, perform, timeout, start_request, start_stream.",
  "affected_downstream": ["repl-provider-routing.md", "repl-streaming-updates.md"]
}
```

```json
{
  "ok": false,
  "blocked_by": "Step 4: Async Pattern - Found curl_easy_perform() in src/providers/openai/shim.c:142. Shim must use curl_multi to maintain async behavior.",
  "affected_downstream": ["All provider and REPL tasks"]
}
```
