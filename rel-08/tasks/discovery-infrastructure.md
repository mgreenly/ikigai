# Task: Discovery Infrastructure

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/extended
**Depends on:** remove-internal-tools.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task builds the external tool infrastructure: registry, discovery, execution, and wrapper. After completion, ikigai discovers tools at startup and can execute them.

## Pre-Read

**Skills:**
- `/load errors` - res_t, PANIC usage
- `/load style` - Code style conventions
- `/load memory` - talloc ownership patterns
- `/load naming` - ik_MODULE_THING naming

**Plan (CRITICAL - read all):**
- `cdd/plan/architecture.md` - Struct definitions, function signatures
- `cdd/plan/memory-management.md` - Ownership hierarchy
- `cdd/plan/tool-discovery-execution.md` - Discovery protocol, execution flow
- `cdd/plan/integration-specification.md` - Struct changes, call site changes

**Source:**
- `src/shared.h` - Will add tool_registry field
- `src/repl_init.c` - Will call discovery at startup
- `src/repl_tool.c` - Will replace stub with real execution

## Libraries

Use only:
- `yyjson` (vendored) - JSON parsing
- `talloc` (system library) - Memory management
- POSIX fork/exec/pipe - Process spawning
- POSIX select() - I/O multiplexing (for discovery)

Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] Internal tools removed (stubs in place)
- [ ] All 6 external tools exist in `libexec/ikigai/`

## Objective

Create external tool infrastructure with blocking discovery API:
1. Tool registry (`src/tool_registry.c/.h`)
2. Tool discovery (`src/tool_discovery.c/.h`) - async internals, blocking public API
3. External executor (`src/tool_external.c/.h`)
4. Response wrapper (`src/tool_wrapper.c/.h`)
5. Integration with shared context and REPL

## New Files to Create

### src/tool_registry.h

```c
#ifndef IK_TOOL_REGISTRY_H
#define IK_TOOL_REGISTRY_H

#include <talloc.h>
#include <stdbool.h>
#include <stddef.h>
#include "vendor/yyjson/yyjson.h"

typedef struct {
    char *name;              // "bash", "file_read", etc.
    char *path;              // Full path to executable
    yyjson_doc *schema_doc;  // Parsed schema
    yyjson_val *schema_root; // Root of schema
} ik_tool_registry_entry_t;

typedef struct {
    ik_tool_registry_entry_t *entries;
    size_t count;
    size_t capacity;
} ik_tool_registry_t;

ik_tool_registry_t *ik_tool_registry_create(TALLOC_CTX *ctx);
ik_tool_registry_entry_t *ik_tool_registry_lookup(ik_tool_registry_t *registry, const char *name);
yyjson_mut_val *ik_tool_registry_build_all(ik_tool_registry_t *registry, yyjson_mut_doc *doc);

#endif
```

### src/tool_discovery.h

```c
#ifndef IK_TOOL_DISCOVERY_H
#define IK_TOOL_DISCOVERY_H

#include <talloc.h>
#include "error.h"
#include "tool_registry.h"

typedef enum {
    TOOL_SCAN_NOT_STARTED,
    TOOL_SCAN_IN_PROGRESS,
    TOOL_SCAN_COMPLETE,
    TOOL_SCAN_FAILED
} tool_scan_state_t;

// Blocking discovery - spawns all tools from ALL THREE directories, waits for completion
// Scans system_dir AND user_dir AND project_dir (all three)
// Override precedence: Project > User > System
res_t ik_tool_discovery_run(TALLOC_CTX *ctx,
                             const char *system_dir,   // PREFIX/libexec/ikigai/
                             const char *user_dir,     // ~/.ikigai/tools/
                             const char *project_dir,  // $PWD/.ikigai/tools/
                             ik_tool_registry_t *registry);

#endif
```

### src/tool_external.h

```c
#ifndef IK_TOOL_EXTERNAL_H
#define IK_TOOL_EXTERNAL_H

#include <talloc.h>
#include "error.h"

// Execute external tool - returns JSON output or error
res_t ik_tool_external_exec(TALLOC_CTX *ctx,
                             const char *tool_path,
                             const char *arguments_json);

#endif
```

### src/tool_wrapper.h

```c
#ifndef IK_TOOL_WRAPPER_H
#define IK_TOOL_WRAPPER_H

#include <talloc.h>
#include <stdint.h>

char *ik_tool_wrap_success(TALLOC_CTX *ctx, const char *tool_result_json);
char *ik_tool_wrap_failure(TALLOC_CTX *ctx, const char *error, const char *error_code,
                           int32_t exit_code, const char *stdout_captured, const char *stderr_captured);

#endif
```

## Implementation Notes

### Registry

- Allocate entries array as child of registry (not individual entries)
- String fields (name, path) are children of registry (survive array realloc)
- Schema docs use talloc allocator via `ik_make_talloc_allocator(registry)`
- `ik_tool_registry_build_all()` returns canonical JSON Schema format

### Discovery

**CRITICAL: Scan ALL THREE directories (all three scanned every time, any/all/none may exist):**
- `PREFIX/libexec/ikigai/` - **System tools** shipped with ikigai
- `~/.ikigai/tools/` - **User tools** (personal, global to user)
- `$PWD/.ikigai/tools/` - **Project tools** (project-specific, local to working directory)

**For each executable in ALL THREE directories:**
- Spawn with `--schema`, 1s timeout
- Parse schema, add to unified registry
- **Override precedence: Project > User > System** (most specific wins)
  - Same tool name in multiple dirs: project beats user, user beats system
- Failures are logged but don't abort discovery
- Missing/empty directories handled gracefully (no error)

### External Executor

- Fork/exec with stdin/stdout/stderr pipes
- Write arguments JSON to stdin, close
- Read stdout with 30s timeout (use alarm/SIGALRM or poll)
- Return JSON output or error

### Wrapper

- Success: `{"tool_success": true, "result": {...}}`
- Failure: `{"tool_success": false, "error": "...", "error_code": "...", ...}`

## Struct Changes

### src/shared.h

Add fields to `ik_shared_ctx_t`:

```c
tool_scan_state_t tool_scan_state;
ik_tool_registry_t *tool_registry;
```

### src/shared.c

Initialize in `ik_shared_ctx_init()`:

```c
shared->tool_scan_state = TOOL_SCAN_NOT_STARTED;
shared->tool_registry = NULL;
```

## Integration Points

### src/repl_init.c

In `ik_repl_init()`, after shared context init:

```c
shared->tool_registry = ik_tool_registry_create(shared);

// CRITICAL: ALL THREE directories are scanned (system AND user AND project)
// Override precedence: Project > User > System (most specific wins)
res_t disc_result = ik_tool_discovery_run(shared,
    PREFIX "/libexec/ikigai",  // System tools directory (shipped with ikigai)
    "~/.ikigai/tools",          // User tools directory (global to user)
    ".ikigai/tools",            // Project tools directory (local to $PWD)
    shared->tool_registry);

if (is_ok(&disc_result)) {
    shared->tool_scan_state = TOOL_SCAN_COMPLETE;
} else {
    shared->tool_scan_state = TOOL_SCAN_FAILED;
    // Log warning, continue without tools
}
```

### src/repl_tool.c - Thread Worker (Async Path)

Replace stub in `tool_thread_worker()` (~line 43):

```c
ik_tool_registry_t *registry = args->agent->shared->tool_registry;
ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, args->tool_name);

char *wrapped_result;
if (entry == NULL) {
    wrapped_result = ik_tool_wrap_failure(args->ctx,
        "Tool not found", "TOOL_NOT_FOUND", -1, "", "");
} else {
    res_t exec_result = ik_tool_external_exec(args->ctx, entry->path, args->arguments);
    if (is_ok(&exec_result)) {
        wrapped_result = ik_tool_wrap_success(args->ctx, exec_result.ok);
    } else {
        wrapped_result = ik_tool_wrap_failure(args->ctx,
            exec_result.err->msg, "TOOL_CRASHED", -1, "", "");
    }
}
args->agent->tool_thread_result = wrapped_result;
```

### src/repl_tool.c - Sync Path

Replace stub in `ik_repl_execute_pending_tool()` (~line 88) with the same pattern:

```c
ik_tool_registry_t *registry = repl->current->shared->tool_registry;
ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(registry, tc->name);

char *wrapped_result;
if (entry == NULL) {
    wrapped_result = ik_tool_wrap_failure(repl,
        "Tool not found", "TOOL_NOT_FOUND", -1, "", "");
} else {
    res_t exec_result = ik_tool_external_exec(repl, entry->path, tc->arguments);
    if (is_ok(&exec_result)) {
        wrapped_result = ik_tool_wrap_success(repl, exec_result.ok);
    } else {
        wrapped_result = ik_tool_wrap_failure(repl,
            exec_result.err->msg, "TOOL_CRASHED", -1, "", "");
    }
}
char *result_json = wrapped_result;
```

### Function Signature Change: ik_request_build_from_conversation

The `registry` parameter must be added to make tools available during request building.

**File: src/providers/request.h**

Update declaration:
```c
// Old:
res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent, ik_request_t **out);

// New:
res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent,
                                          ik_tool_registry_t *registry,
                                          ik_request_t **out);
```

Add forward declaration at top of header:
```c
typedef struct ik_tool_registry ik_tool_registry_t;
```

### Call Site Updates (ALL FOUR REQUIRED)

Update every call site to pass the registry. Note: Some call sites use the direct function, others use the wrapper function (trailing underscore).

**1. src/repl_actions_llm.c (~line 148) - DIRECT FUNCTION:**
```c
// Old:
result = ik_request_build_from_conversation(agent, agent, &req);

// New:
result = ik_request_build_from_conversation(agent, agent, agent->shared->tool_registry, &req);
```

**2. src/repl_tool_completion.c (~line 55) - WRAPPER FUNCTION:**
```c
// Old:
result = ik_request_build_from_conversation_(agent, agent, (void **)&req);

// New:
result = ik_request_build_from_conversation_(agent, agent, agent->shared->tool_registry, (void **)&req);
```

**3. src/commands_fork.c (~line 109) - WRAPPER FUNCTION:**
```c
// Old:
res = ik_request_build_from_conversation_(repl->current, repl->current, (void **)&req);

// New:
res = ik_request_build_from_conversation_(repl->current, repl->current, repl->current->shared->tool_registry, (void **)&req);
```

**4. src/wrapper_internal.h and src/wrapper_internal.c - WRAPPER FUNCTION SIGNATURE:**

The wrapper function `ik_request_build_from_conversation_()` MUST be updated to accept and forward the registry parameter.

**src/wrapper_internal.h - Update inline implementation (~line 52-55):**
```c
// Old:
MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)
{
    return ik_request_build_from_conversation(ctx, agent, (ik_request_t **)req_out);
}

// New:
MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    return ik_request_build_from_conversation(ctx, agent, (ik_tool_registry_t *)registry, (ik_request_t **)req_out);
}
```

**src/wrapper_internal.h - Update declaration (~line 111):**
```c
// Old:
MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out);

// New:
MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out);
```

**src/wrapper_internal.c - Update implementation (~line 59-62):**
```c
// Old:
MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)
{
    return ik_request_build_from_conversation(ctx, agent, (ik_request_t **)req_out);
}

// New:
MOCKABLE res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    return ik_request_build_from_conversation(ctx, agent, (ik_tool_registry_t *)registry, (ik_request_t **)req_out);
}
```

**Note:** Add forward declaration or include for `ik_tool_registry_t` in wrapper_internal.h if needed.

### Test Mock Updates (ALL FOUR REQUIRED)

The wrapper function `ik_request_build_from_conversation_()` is mocked in test files. Each mock MUST be updated to match the new signature, or `make check` will fail with compilation errors.

**1. tests/unit/commands/cmd_fork_error_test.c (~line 43):**
```c
// Old:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)

// New:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    (void)registry;  // Suppress unused warning
    // ... rest of mock implementation
}
```

**2. tests/unit/commands/cmd_fork_coverage_test_mocks.c (~line 72):**
```c
// Old:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)

// New:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    (void)registry;  // Suppress unused warning
    // ... rest of mock implementation
}
```

**3. tests/unit/commands/cmd_fork_basic_test.c (~line 46):**
```c
// Old:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)

// New:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    (void)registry;  // Suppress unused warning
    // ... rest of mock implementation
}
```

**4. tests/unit/repl/repl_tool_completion_test.c (~line 98):**
```c
// Old:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)

// New:
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void *registry, void **req_out)
{
    (void)registry;  // Suppress unused warning
    // ... rest of mock implementation
}
```

**Verification:** After updating all 4 test mocks, `make check` must pass without compilation errors.

### src/providers/request_tools.c

Update function to accept registry parameter and iterate it.

**Reference:** `cdd/plan/integration-specification.md` → "Function Signature Change: ik_request_build_from_conversation" and "Function Signature Change: ik_request_add_tool" and "Schema Extraction from Registry"

**Pseudocode for registry iteration:**

```
ik_request_build_from_conversation(ctx, agent, registry, out):
    // ... existing setup code (create request, add messages) ...

    if registry != NULL:
        for i = 0 to registry->count - 1:
            entry = &registry->entries[i]
            desc_val = yyjson_obj_get(entry->schema_root, "description")
            description = yyjson_get_str(desc_val)
            params_val = yyjson_obj_get(entry->schema_root, "parameters")
            ik_request_add_tool(req, entry->name, description, params_val)

    // ... rest of function ...
```

## Makefile Changes

Add new source files to CLIENT_SOURCES, MODULE_SOURCES:

```
src/tool_registry.c
src/tool_discovery.c
src/tool_external.c
src/tool_wrapper.c
```

## Test Specification

**Reference:** `cdd/plan/test-specification.md` → "Phase 2: Discovery Infrastructure"

**Unit test files to create:**

### 1. tests/unit/tool_registry/registry_test.c

**Goals:** Test registry data structure creation, lookup, and schema building.

| Test | Goal |
|------|------|
| `test_registry_create` | Returns non-NULL, count=0 |
| `test_registry_lookup_empty` | Lookup on empty returns NULL |
| `test_registry_lookup_not_found` | Non-existent name returns NULL |
| `test_registry_lookup_found` | After add, lookup returns entry |
| `test_registry_build_all_empty` | Empty registry → empty array |
| `test_registry_build_all_entries` | Returns JSON array with schemas |

**Mocking:** None - pure data structure

### 2. tests/unit/tool_discovery/discovery_test.c

**Goals:** Test directory scanning, schema parsing, error handling.

| Test | Goal |
|------|------|
| `test_discovery_empty_dirs` | Empty dirs → success, 0 tools |
| `test_discovery_nonexistent_dir` | Non-existent dir handled gracefully |
| `test_discovery_invalid_schema` | Invalid JSON logged, tool skipped |
| `test_discovery_timeout` | Non-responding tool skipped after 1s |
| `test_discovery_user_overrides_system` | Same name in user dir wins |

**Mocking:** Create temp directories with mock shell scripts that echo JSON

### 3. tests/unit/tool_external/external_exec_test.c

**Goals:** Test tool execution with JSON I/O, timeouts, errors.

| Test | Goal |
|------|------|
| `test_exec_success` | Valid tool returns JSON |
| `test_exec_invalid_json_output` | Non-JSON output → error |
| `test_exec_timeout` | 30s timeout → error |
| `test_exec_nonexistent_path` | Missing tool → error |
| `test_exec_permission_denied` | Non-executable → error |

**Mocking:** Temp shell scripts as mock tools

### 4. tests/unit/tool_wrapper/wrapper_test.c

**Goals:** Test success/failure envelope building.

| Test | Goal |
|------|------|
| `test_wrap_success_simple` | Wraps in success envelope |
| `test_wrap_success_nested` | Nested JSON preserved |
| `test_wrap_failure_all_fields` | All fields populated |
| `test_wrap_failure_null_fields` | NULL optional fields handled |

**Mocking:** None - pure JSON building

### Integration verification:
1. `make check` - All new tests pass
2. Start ikigai - Registry populates with 6 tools
3. LLM request includes tools array
4. Tool call executes external process

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(discovery-infrastructure.md): [success|partial|failed] - external tool infrastructure

Created registry, discovery, executor, wrapper. Integrated with REPL.
6 tools discovered at startup, execution works end-to-end.
EOF
)"
```

Report status:
- Success: `/task-done discovery-infrastructure.md`
- Partial/Failed: `/task-fail discovery-infrastructure.md`

## Postconditions

- [ ] 8 new files created (4 .h + 4 .c)
- [ ] shared.h has tool_registry field
- [ ] request.h has updated ik_request_build_from_conversation signature with registry parameter
- [ ] All 4 call sites updated (repl_actions_llm.c, repl_tool_completion.c, commands_fork.c, wrapper_internal.h/c)
- [ ] All 4 test mock signatures updated (cmd_fork_error_test.c, cmd_fork_coverage_test_mocks.c, cmd_fork_basic_test.c, repl_tool_completion_test.c)
- [ ] repl_init.c calls discovery
- [ ] repl_tool.c uses registry + external exec (BOTH paths: tool_thread_worker AND ik_repl_execute_pending_tool)
- [ ] `make clean && make` succeeds
- [ ] `make check` passes
- [ ] 6 tools discovered at startup
- [ ] All changes committed
- [ ] Working copy is clean
