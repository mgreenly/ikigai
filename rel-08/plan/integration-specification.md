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
ik_repl_actions_llm.c:ik_repl_action_submit_to_llm()
  └─> ik_request_build_from_conversation(agent, agent, &req)  [src/providers/request_tools.c:246]
      └─> Hard-coded tool definitions (lines 283-298)  ← BEING REPLACED
      └─> ik_request_add_tool() for each tool
      └─> Populates req->tools[] array
  └─> Provider serializer (e.g., ik_anthropic_serialize_request_stream)
      └─> Reads req->tools[] and req->tool_count (already populated)
```

### Function Signature Change: ik_request_build_from_conversation

**File:** `src/providers/request.h` and `src/providers/request_tools.c`

**Current signature:**

| Parameter | Type |
|-----------|------|
| ctx | `TALLOC_CTX *` |
| agent | `void *` |
| out | `ik_request_t **` |

**New signature:** Add `registry` parameter:

| Parameter | Type | Notes |
|-----------|------|-------|
| ctx | `TALLOC_CTX *` | |
| agent | `void *` | |
| registry | `ik_tool_registry_t *` | NEW: can be NULL |
| out | `ik_request_t **` | |

**Behavioral change:**
- **Before:** Hard-codes tool definitions in lines 283-298, calls `ik_request_add_tool()` for each
- **After:** If registry is non-NULL, iterates `registry->entries[0..count-1]` and calls `ik_request_add_tool()` for each entry. If NULL, uses empty tools array.

### Function Signature Change: ik_request_add_tool

**File:** `src/providers/request_tools.c`

The current function uses hard-coded `ik_tool_param_def_t[]` arrays. Replace with yyjson-based signature:

**Current signature (being deleted with internal tools):**

```c
void ik_request_add_tool(ik_request_t *req, const char *name, const char *description,
                          const ik_tool_param_def_t *params, size_t param_count);
```

**New signature:**

```c
void ik_request_add_tool(ik_request_t *req, const char *name, const char *description,
                          yyjson_val *parameters_schema);
```

| Parameter | Type | Notes |
|-----------|------|-------|
| req | `ik_request_t *` | Request being built |
| name | `const char *` | Tool name |
| description | `const char *` | Tool description |
| parameters_schema | `yyjson_val *` | The "parameters" object from tool schema (pointer, not copied) |

**Lifetime:** `parameters_schema` points into registry entry's `schema_doc`. Valid as long as registry exists.

### Schema Extraction from Registry

Extract fields from `ik_tool_registry_entry_t` using yyjson:

| Field | Extraction |
|-------|------------|
| name | `entry->name` (already a string) |
| description | `yyjson_get_str(yyjson_obj_get(entry->schema_root, "description"))` |
| parameters | `yyjson_obj_get(entry->schema_root, "parameters")` |

### Call Site Changes

**File:** `src/repl_actions_llm.c` (line ~148)

**Change:** Pass registry (from shared context) to `ik_request_build_from_conversation()`.

**Current call:**
```c
result = ik_request_build_from_conversation(agent, agent, &req);
```

**New call:**
```c
result = ik_request_build_from_conversation(agent, agent, agent->shared->tool_registry, &req);
```

### Option A: Pass registry through call chain (Recommended)

**Implementation:** Pass the registry directly to request building at the call site.

**Approach:** Add registry parameter to `ik_request_build_from_conversation()`.

**Usage:** Pass `agent->shared->tool_registry` at each call site:
- `src/repl_actions_llm.c:148`
- `src/repl_tool_completion.c:55` (via wrapper)
- `src/commands_fork.c:109` (via wrapper)

**Recommendation:** This makes the dependency explicit and keeps tool population in request building, not serialization.

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
| `src/providers/request.h` | Add `registry` parameter to `ik_request_build_from_conversation` |

**Phase 6 adds:**
| `src/repl.h` | Add `tool_discovery` field to `ik_repl_ctx_t` |

### Source Files to Modify

| File | Change |
|------|--------|
| `src/shared.c` | Initialize registry fields in `ik_shared_ctx_init` |
| `src/providers/request_tools.c` | Update `ik_request_build_from_conversation` to use registry instead of hard-coded tools |
| `src/repl_actions_llm.c` | Pass `agent->shared->tool_registry` to `ik_request_build_from_conversation` |
| `src/repl_tool_completion.c` | Pass registry to `ik_request_build_from_conversation` (via wrapper) |
| `src/commands_fork.c` | Pass registry to `ik_request_build_from_conversation` (via wrapper) |
| `src/wrapper_internal.h` | Update wrapper to accept registry parameter |
| `src/repl_tool.c` | Replace `ik_tool_dispatch` with registry lookup + external exec |
| `src/repl_init.c` | Call blocking `ik_tool_discovery_run()` at startup |

**Phase 6 adds:**
| `src/repl.c` | Integrate async discovery with event loop |
| `src/repl_actions_llm.c` | Wait for scan if in progress during user submit |

### Files to Delete

See `removal-specification.md` for complete list.

## Multi-Provider Integration

This section clarifies how Anthropic and Google providers work with the tool system. Provider serializers do NOT call `ik_tool_build_all()` or any tool building function directly. Instead, they read from `req->tools[]` which is populated by `ik_request_build_from_conversation()`.

### Key Architecture Point

**Tool population happens in request building, not serialization:**

1. `ik_request_build_from_conversation()` populates `req->tools[]` with tool definitions
2. Provider serializers read `req->tools` and `req->tool_count` to serialize tools
3. Each provider transforms the tool schema to its own format during serialization

This means the registry integration happens in ONE place (`ik_request_build_from_conversation`), not in each provider serializer.

### Anthropic Provider

**File:** `src/providers/anthropic/request.c`

**Function:** `ik_anthropic_serialize_request_stream()`

**Current behavior:** Reads `req->tools` and `req->tool_count` from the request struct, transforms to Anthropic format.

**No signature change needed.** The provider already reads from `req->tools[]`, so once `ik_request_build_from_conversation()` populates tools from the registry, Anthropic serialization works automatically.

**Schema transformation (existing):** Anthropic uses `input_schema` key for tool parameters. The serializer:
1. Wraps each tool in `{name, description, input_schema}` format
2. Keeps `additionalProperties` as-is (passthrough)
3. Keeps `required` array as-is

### Google Provider

**File:** `src/providers/google/request.c`

**Function:** `ik_google_serialize_request()`

**Current behavior:** Reads `req->tools` and `req->tool_count` from the request struct, transforms to Google format.

**No signature change needed.** The provider already reads from `req->tools[]`, so once `ik_request_build_from_conversation()` populates tools from the registry, Google serialization works automatically.

**Schema transformation (existing):** Google uses `functionDeclarations` array format. The serializer:
1. Wraps tools in `{tools: [{functionDeclarations: [...]}]}` structure
2. Removes `additionalProperties` field (Gemini doesn't support it)
3. Keeps `required` array as-is

### Call Chain: All Providers

```
ik_repl_actions_llm.c:ik_repl_action_submit_to_llm()
  └─> ik_request_build_from_conversation(agent, agent, registry, &req)
      └─> for (i = 0; i < registry->count; i++) iterate entries  ← NEW
      └─> ik_request_add_tool() for each tool
      └─> req->tools[] now populated
  └─> Provider-specific serializer (Anthropic/Google/OpenAI)
      └─> Reads req->tools[] (already populated)
      └─> Transforms to provider-specific JSON format
```

### OpenAI Provider Note

OpenAI provider may use a different call path through `ik_openai_serialize_chat_request()`. If this function calls `ik_tool_build_all()` directly (bypassing `req->tools`), it will need to be updated to either:
1. Read from `req->tools[]` like other providers, OR
2. Accept a registry parameter and call `ik_tool_registry_build_all(registry, doc)`

### Summary of Provider Schema Differences

| Provider | Wrapper Format | Schema Key | `additionalProperties` | `required` |
|----------|----------------|------------|------------------------|------------|
| OpenAI | `{type: "function", function: {...}}` | `parameters` | Add `false` | Add ALL properties |
| Anthropic | `{name, description, input_schema}` | `input_schema` | Passthrough | Keep as-is |
| Google | `{functionDeclarations: [...]}` | `parameters` | Remove | Keep as-is |

Reference: See `cdd/plan/architecture.md` "Schema Transformation" section for complete provider format details.

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
