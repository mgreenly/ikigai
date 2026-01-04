# Integration Specification

Complete specification for integrating new external tool system with existing code. This document specifies exact struct changes, function signature changes, and the complete data flow.

## Registry Ownership

The tool registry lives in `ik_shared_ctx_t` because:
1. Registry is shared across all agents (same tools available to all)
2. Registry is read-only during request/execution (populated once at startup)
3. Follows existing pattern for shared resources (cfg, db, history)

### Struct Change: ik_shared_ctx_t

**File:** `src/shared.h`

**Add fields:**

| Field | Type | Initial Value | Purpose |
|-------|------|---------------|---------|
| `tool_registry` | `ik_tool_registry_t *` | `NULL` | Tool registry, NULL until discovery complete |
| `tool_scan_state` | `tool_scan_state_t` | `TOOL_SCAN_NOT_STARTED` | Discovery state |

**Initialization:** Set initial values in `ik_shared_ctx_init()`.

**Discovery:** REPL calls discovery on startup, populates `tool_registry`.

## Schema Building Integration

### Call Chain (Current)

```
ik_repl_actions_llm.c (or similar)
  └─> ik_openai_multi_request.c:ik_openai_multi_start_request_ex()
      └─> ik_providers/openai/request_chat.c:ik_openai_serialize_chat_request(parent, request, tool_choice)
          └─> ik_tool_build_all(doc)  ← BEING REPLACED
```

### Function Signature Change: ik_openai_serialize_chat_request

**File:** `src/providers/openai/request.h` and `src/providers/openai/request_chat.c`

**Current signature:**

| Parameter | Type |
|-----------|------|
| parent | `void *` |
| request | `const ik_openai_request_t *` |
| tool_choice | `ik_tool_choice_t` |

**New signature:** Add `registry` parameter:

| Parameter | Type | Notes |
|-----------|------|-------|
| parent | `void *` | |
| request | `const ik_openai_request_t *` | |
| tool_choice | `ik_tool_choice_t` | |
| registry | `ik_tool_registry_t *` | NEW: can be NULL |

**Behavioral change:**
- **Before:** Calls `ik_tool_build_all(doc)` to get static tool definitions
- **After:** If registry is non-NULL, calls `ik_tool_registry_build_all(registry, doc)`. If NULL, uses empty tools array.

### Call Site Changes

**File:** Caller of `ik_openai_serialize_chat_request()`

**Change:** Pass registry (from shared context) as fourth argument to `ik_openai_serialize_chat_request()`.

**Problem:** The caller may not have direct access to shared context.

**Solution:** Pass registry through the call chain from the shared context.

### Option A: Pass registry through call chain (Recommended)

**Implementation:** Thread the registry parameter through the request building functions.

**Approach:** Add registry parameter to request building functions in the call chain.

**Usage:** Pass `shared->tool_registry` from the REPL context down to `ik_openai_serialize_chat_request()`.

**Recommendation:** This makes the dependency explicit and avoids storing duplicate references.

## Tool Execution Integration

### Call Chain (Current)

```
LLM returns tool_call
  └─> REPL detects pending_tool_call
      └─> ik_agent_start_tool_execution(agent)
          └─> Creates tool_thread_args_t { ctx, tool_name, arguments, agent }
          └─> pthread_create(tool_thread_worker, args)
              └─> ik_tool_dispatch(ctx, tool_name, arguments)  ← BEING REPLACED
```

### Struct Change: tool_thread_args_t

**File:** `src/repl_tool.c`

**No change needed.** Registry accessed via existing field: `args->agent->shared->tool_registry`

### Function Change: tool_thread_worker

**File:** `src/repl_tool.c`

**Current behavior:** Calls `ik_tool_dispatch(ctx, tool_name, arguments)` directly.

**New behavior:**

1. Look up tool in registry via `ik_tool_registry_lookup(registry, tool_name)`
2. **If tool not found:** Call `ik_tool_wrap_failure()` with error "Tool 'X' not found" and code `TOOL_NOT_FOUND`
3. **If tool found:** Call `ik_tool_external_exec(ctx, entry->path, arguments)`
4. **If execution fails:** Call `ik_tool_wrap_failure()` with error details, code (`TOOL_CRASHED`, `TOOL_TIMEOUT`, or `INVALID_OUTPUT`), exit code, stdout, stderr
5. **If execution succeeds:** Call `ik_tool_wrap_success()` with tool's JSON output
6. Store result in `agent->tool_thread_result`
7. Signal completion under mutex (existing pattern)

**Registry access:** Via `args->agent->shared->tool_registry`

### Sync Path: ik_repl_execute_pending_tool

**File:** `src/repl_tool.c` (line ~63)

Same pattern as thread worker. Replace `ik_tool_dispatch` call with registry lookup + external exec + wrapper.

## Response Format

### Wrapper Response Contract

**Success envelope:**
```json
{"tool_success": true, "result": <tool_output>}
```

**Failure envelope:**
```json
{"tool_success": false, "error": "...", "error_code": "...", "exit_code": N, "stdout": "...", "stderr": "..."}
```

**Field semantics:**
- `tool_success`: Distinguishes ikigai-level success from tool's internal errors
- `result`: Tool's raw JSON output (may itself contain error fields)
- `error_code`: One of `TOOL_TIMEOUT`, `TOOL_CRASHED`, `INVALID_OUTPUT`, `TOOL_NOT_FOUND`

**Wrapper functions:** `ik_tool_wrap_success()`, `ik_tool_wrap_failure()` in `src/tool_wrapper.c`

## Thread Safety

### Invariants

1. **Registry write:** Only during discovery (before first LLM request)
2. **Registry read:** Multiple threads may read concurrently (during tool execution, schema building)
3. **No locking needed:** Registry is immutable after discovery completes

### State Machine

```
TOOL_SCAN_NOT_STARTED
    └─> REPL startup calls ik_tool_discovery_run() (blocking)
        └─> TOOL_SCAN_COMPLETE (success) or TOOL_SCAN_FAILED (error)
```

**Sync behavior (Phases 2-5):** Discovery blocks at startup. By the time terminal appears, registry is populated.

**Thread safety:**
- Discovery runs to completion before event loop starts
- Registry is immutable after discovery completes
- No locking needed - registry is read-only during normal operation

## New Files

| File | Purpose |
|------|---------|
| `src/tool_registry.h` | Registry types and function declarations |
| `src/tool_registry.c` | Registry implementation |
| `src/tool_discovery.h` | Discovery types and function declarations |
| `src/tool_discovery.c` | Discovery implementation |
| `src/tool_external.h` | External execution types and declarations |
| `src/tool_external.c` | External execution implementation |
| `src/tool_wrapper.h` | Response wrapper declarations |
| `src/tool_wrapper.c` | Response wrapper implementation |

## Summary of Changes by File

### Headers to Modify

| File | Change |
|------|--------|
| `src/shared.h` | Add `tool_registry` and `tool_scan_state` fields |
| `src/providers/openai/request.h` | Add `registry` parameter to `ik_openai_serialize_chat_request` |

**Phase 6 adds:**
| `src/repl.h` | Add `tool_discovery` field to `ik_repl_ctx_t` |

### Source Files to Modify

| File | Change |
|------|--------|
| `src/shared.c` | Initialize registry fields in `ik_shared_ctx_init` |
| `src/providers/openai/request_chat.c` | Update `ik_openai_serialize_chat_request` to use registry |
| Caller(s) of serialize_chat_request | Pass registry to serialize_chat_request |
| `src/repl_tool.c` | Replace `ik_tool_dispatch` with registry lookup + external exec |
| `src/repl_init.c` | Call blocking `ik_tool_discovery_run()` at startup |

**Phase 6 adds:**
| `src/repl.c` | Integrate async discovery with event loop |
| `src/repl_actions_llm.c` | Wait for scan if in progress during user submit |

### Files to Delete

See `removal-specification.md` for complete list.

## Discovery Startup Integration

### Initialization Sequence (Phases 2-5: Blocking API)

In `ik_repl_init()`:

1. **After** shared context initialization, **before** event loop starts
2. Create empty registry via `ik_tool_registry_create(shared)`
3. Set `shared->tool_scan_state = TOOL_SCAN_NOT_STARTED`
4. Call `ik_tool_discovery_run()` - **blocks** until all tools scanned
   - Internally uses async primitives (spawn all, select loop)
   - But blocks caller until complete
5. On success: set `tool_scan_state = TOOL_SCAN_COMPLETE`
6. On failure: set `tool_scan_state = TOOL_SCAN_FAILED`, log warning, continue without tools
7. Event loop starts (terminal appears)

**Note:** The async primitives (`_start`, `_add_fds`, `_process_fds`, etc.) are implemented in Phase 2 but kept internal (static). `ik_tool_discovery_run()` is the only public API until Phase 6.

### Phase 6: Expose Async API

**Phase 6 makes existing internals public:**

In `src/tool_discovery.h`:
- Move async functions from static to public declarations

In `ik_repl_init()`:
- Replace `ik_tool_discovery_run()` with `ik_tool_discovery_start()` (non-blocking)
- Set `tool_scan_state = TOOL_SCAN_IN_PROGRESS`
- Store discovery state in `repl->tool_discovery`

In main event loop (`src/repl.c`):
- When `tool_scan_state == TOOL_SCAN_IN_PROGRESS`:
  - Call `ik_tool_discovery_add_fds()` to add discovery fds to select fdset
  - Call `ik_tool_discovery_process_fds()` for ready fds
  - Check `ik_tool_discovery_is_complete()` - if true, finalize and set state

In submit handler (`src/repl_actions_llm.c`):
- If `TOOL_SCAN_IN_PROGRESS`: show spinner, wait for completion

### Struct Change: ik_repl_ctx_t (Phase 6 only)

**File:** `src/repl.h`

**Add field:**

| Field | Type | Purpose |
|-------|------|---------|
| `tool_discovery` | `ik_tool_discovery_state_t *` | Active discovery state, NULL when scan not in progress |
