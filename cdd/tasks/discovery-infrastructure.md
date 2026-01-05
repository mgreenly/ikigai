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

// Blocking discovery - spawns all tools, waits for completion
res_t ik_tool_discovery_run(TALLOC_CTX *ctx,
                             const char *system_dir,
                             const char *user_dir,
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

- Scan `PREFIX/libexec/ikigai/` and `~/.ikigai/tools/`
- For each executable, spawn with `--schema`, 1s timeout
- Parse schema, add to registry
- User tools override system tools (same name)
- Failures are logged but don't abort discovery

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
res_t disc_result = ik_tool_discovery_run(shared,
    PREFIX "/libexec/ikigai",
    "~/.ikigai/tools",
    shared->tool_registry);
if (is_ok(&disc_result)) {
    shared->tool_scan_state = TOOL_SCAN_COMPLETE;
} else {
    shared->tool_scan_state = TOOL_SCAN_FAILED;
    // Log warning, continue without tools
}
```

### src/repl_tool.c

Replace stub in `tool_thread_worker()`:

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

### src/providers/request_tools.c

Replace stub with registry iteration:

```c
if (registry != NULL) {
    for (size_t i = 0; i < registry->count; i++) {
        ik_tool_registry_entry_t *entry = &registry->entries[i];
        // Extract name, description, parameters from entry->schema_root
        // Call ik_request_add_tool() for each
    }
}
```

## Makefile Changes

Add new source files to CLIENT_SOURCES, MODULE_SOURCES:

```
src/tool_registry.c
src/tool_discovery.c
src/tool_external.c
src/tool_wrapper.c
```

## Test Scenarios

1. `make check` - All tests pass
2. Start ikigai - Registry populates with 6 tools
3. `/tool` command (once implemented) lists tools
4. LLM request includes tools array
5. Tool call executes external process

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
- [ ] repl_init.c calls discovery
- [ ] repl_tool.c uses registry + external exec
- [ ] `make clean && make` succeeds
- [ ] `make check` passes
- [ ] 6 tools discovered at startup
- [ ] All changes committed
- [ ] Working copy is clean
