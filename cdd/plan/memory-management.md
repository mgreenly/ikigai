# Memory Management Specification

Ownership rules and talloc patterns for the external tool system. All new code follows the project's hierarchical memory model using talloc.

## Ownership Hierarchy

```
root_ctx (main.c)
└── shared_ctx (ik_shared_ctx_t)
    ├── tool_registry (ik_tool_registry_t)
    │   ├── entries[] (ik_tool_registry_entry_t)
    │   │   ├── name (char *, talloc child of entry)
    │   │   ├── path (char *, talloc child of entry)
    │   │   └── schema_doc (yyjson_doc *, uses talloc allocator)
    │   └── ... more entries
    └── tool_discovery (ik_tool_discovery_state_t, Phase 6 only)
        ├── pending[] (per-tool state)
        │   ├── stdout_buf (char *, talloc child)
        │   └── stderr_buf (char *, talloc child)
        └── ... fds, counts
```

When `shared_ctx` is freed, the entire tool subsystem is cleaned up automatically.

## Registry Memory

### Registry Creation

```c
ik_tool_registry_t *ik_tool_registry_create(TALLOC_CTX *ctx);
```

**Ownership:** Registry is allocated as child of `ctx`. Caller passes `shared` context.

**Implementation:**
```c
ik_tool_registry_t *ik_tool_registry_create(TALLOC_CTX *ctx)
{
    ik_tool_registry_t *registry = talloc_zero(ctx, ik_tool_registry_t);
    if (registry == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // entries array allocated as child of registry
    registry->capacity = 16;
    registry->entries = talloc_array(registry, ik_tool_registry_entry_t, registry->capacity);
    if (registry->entries == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return registry;
}
```

### Registry Entry Strings

Entry string fields (`name`, `path`) are talloc children of the registry (not the entry struct, since entries live in an array that may be reallocated).

**Pattern:**
```c
// When adding entry during discovery
entry->name = talloc_strdup(registry, tool_name);
entry->path = talloc_strdup(registry, tool_path);
if (entry->name == NULL || entry->path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
```

**Rationale:** If entries array is reallocated (`talloc_realloc`), strings survive because they're children of registry, not of the array memory.

### Schema Document Integration

Schema documents use the project's talloc-based JSON allocator (`src/json_allocator.c`). This integrates yyjson memory with talloc hierarchy.

**Pattern:**
```c
// When parsing schema during discovery
yyjson_alc alc = ik_make_talloc_allocator(registry);
yyjson_doc *doc = yyjson_read_opts(schema_json, schema_len, 0, &alc, NULL);
if (doc == NULL) {
    // Parse failure - skip tool, log debug message
    return;
}
entry->schema_doc = doc;
entry->schema_root = yyjson_doc_get_root(doc);
```

**Key insight:** Because the allocator uses `registry` as context, all JSON memory is a child of registry. When registry is freed, schema documents are freed automatically. No destructor needed.

**Do NOT use:**
```c
// WRONG - leaks memory when registry freed
yyjson_doc *doc = yyjson_read(schema_json, schema_len, 0);
```

## Discovery Memory

### Discovery State Ownership

```c
res_t ik_tool_discovery_start(TALLOC_CTX *ctx,
                               const char *system_dir,
                               const char *user_dir,
                               ik_tool_registry_t *registry,
                               ik_tool_discovery_state_t **out_state);
```

**Ownership:** Discovery state allocated as child of `ctx`. Caller passes `shared` context.

**Internal allocations:** All buffers (stdout collection, stderr collection, path strings) are children of the discovery state.

**Finalization:**
```c
void ik_tool_discovery_finalize(ik_tool_discovery_state_t *state)
{
    // Close any open fds
    for (size_t i = 0; i < state->pending_count; i++) {
        if (state->pending[i].stdout_fd >= 0) close(state->pending[i].stdout_fd);
        if (state->pending[i].stderr_fd >= 0) close(state->pending[i].stderr_fd);
    }

    // Free state and all children (buffers, strings)
    talloc_free(state);
}
```

### Blocking Discovery Wrapper

```c
res_t ik_tool_discovery_run(TALLOC_CTX *ctx,
                             const char *system_dir,
                             const char *user_dir,
                             ik_tool_registry_t *registry);
```

**Ownership:** No state returned to caller. Internal state created and destroyed within function.

**Error allocation:** Errors allocated on `ctx` (the shared context). This ensures errors survive the function return.

**Pattern:**
```c
res_t ik_tool_discovery_run(TALLOC_CTX *ctx, ...)
{
    // Create temporary context for discovery state
    // Child of ctx so errors allocated on ctx survive
    ik_tool_discovery_state_t *state;
    res_t result = ik_tool_discovery_start(ctx, system_dir, user_dir, registry, &state);
    if (is_err(&result)) return result;  // Error on ctx, survives

    // ... select loop ...

    ik_tool_discovery_finalize(state);  // Cleans up state
    return OK(NULL);
}
```

## External Execution Memory

### Execution Function

```c
res_t ik_tool_external_exec(TALLOC_CTX *ctx,
                             const char *tool_path,
                             const char *arguments_json);
```

**Return value:** `OK(char *)` where the string is allocated on `ctx`.

**Caller responsibility:** Result string is owned by `ctx`. Caller may `talloc_steal()` to change ownership.

**Internal pattern:**
```c
res_t ik_tool_external_exec(TALLOC_CTX *ctx, const char *tool_path, const char *arguments_json)
{
    // Create pipes, fork, exec...

    // Collect stdout into buffer
    char *stdout_buf = talloc_strdup(ctx, "");
    // ... read loop appending to stdout_buf ...

    // On success
    return OK(stdout_buf);  // Caller owns via ctx

    // On failure
    return ERR(ctx, ERR_IO, "Tool execution failed: %s", strerror(errno));
}
```

### Response Wrapper Functions

```c
char *ik_tool_wrap_success(TALLOC_CTX *ctx, const char *tool_result_json);
char *ik_tool_wrap_failure(TALLOC_CTX *ctx, const char *error, const char *error_code,
                           int exit_code, const char *stdout_captured, const char *stderr_captured);
```

**Return value:** JSON string allocated on `ctx`.

**Pattern:**
```c
char *ik_tool_wrap_success(TALLOC_CTX *ctx, const char *tool_result_json)
{
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    // ... build wrapper object ...

    char *result = yyjson_mut_write(doc, 0, NULL);  // Uses talloc via alc
    // doc and all JSON memory freed with ctx
    return result;
}
```

## Thread Execution Memory

Tool execution in background threads follows the existing pattern from `src/repl_tool.c`.

### Memory Flow

```
1. Main thread: Create tool_thread_ctx as child of agent
   └── agent->tool_thread_ctx = talloc_new(agent);

2. Main thread: Copy inputs into thread context
   └── args->tool_name = talloc_strdup(agent->tool_thread_ctx, tc->name);
   └── args->arguments = talloc_strdup(agent->tool_thread_ctx, tc->arguments);

3. Worker thread: Execute tool, allocate result into thread context
   └── res_t result = ik_tool_external_exec(args->ctx, ...);
   └── agent->tool_thread_result = result.ok;  // String on tool_thread_ctx

4. Main thread: Steal result before freeing context
   └── char *result_json = talloc_steal(agent, agent->tool_thread_result);
   └── talloc_free(agent->tool_thread_ctx);  // Frees args, copies, but not result
```

### Integration with External Tools

The change from internal `ik_tool_dispatch()` to external execution:

**Before (internal tools):**
```c
res_t result = ik_tool_dispatch(args->ctx, args->tool_name, args->arguments);
agent->tool_thread_result = result.ok;
```

**After (external tools):**
```c
// Look up tool
ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(
    args->agent->shared->tool_registry, args->tool_name);

char *wrapped_result;
if (entry == NULL) {
    // Tool not found
    wrapped_result = ik_tool_wrap_failure(args->ctx,
        "Tool not found", "TOOL_NOT_FOUND", -1, "", "");
} else {
    // Execute external tool
    res_t exec_result = ik_tool_external_exec(args->ctx, entry->path, args->arguments);
    if (is_ok(&exec_result)) {
        wrapped_result = ik_tool_wrap_success(args->ctx, exec_result.ok);
    } else {
        wrapped_result = ik_tool_wrap_failure(args->ctx,
            exec_result.err->msg, "TOOL_CRASHED", ...);
    }
}
agent->tool_thread_result = wrapped_result;
```

**Key invariant:** All allocations go into `args->ctx` (which is `tool_thread_ctx`). Main thread steals the final result before freeing context.

## Error Allocation Rules

Following the project's error handling patterns (see `errors` skill):

### Rule: Errors Allocated on Caller's Context

All functions that return `res_t` allocate errors on the `ctx` parameter, never on internal temporary contexts.

**Correct:**
```c
res_t ik_tool_discovery_run(TALLOC_CTX *ctx, ...)
{
    TALLOC_CTX *tmp = talloc_new(ctx);  // Temporary for internal work

    // If something fails, error goes on ctx (not tmp)
    if (scan_failed) {
        talloc_free(tmp);
        return ERR(ctx, ERR_IO, "Discovery failed");  // Error survives
    }

    talloc_free(tmp);
    return OK(NULL);
}
```

**Incorrect:**
```c
res_t ik_tool_discovery_run(TALLOC_CTX *ctx, ...)
{
    TALLOC_CTX *tmp = talloc_new(ctx);

    res_t result = some_operation(tmp, ...);  // Error allocated on tmp
    if (is_err(&result)) {
        talloc_free(tmp);  // FREES THE ERROR!
        return result;     // USE-AFTER-FREE
    }
}
```

### Reparenting Errors from Sub-operations

When calling functions that allocate errors on a context you need to free:

```c
res_t ik_foo_init(TALLOC_CTX *ctx, ...)
{
    TALLOC_CTX *tmp = talloc_new(ctx);

    res_t result = ik_bar_init(tmp, ...);
    if (is_err(&result)) {
        talloc_steal(ctx, result.err);  // Rescue error to survivor
        talloc_free(tmp);
        return result;
    }

    // Success path...
    talloc_free(tmp);
    return OK(...);
}
```

## Summary Table

| Component | Allocated On | Freed By |
|-----------|--------------|----------|
| `ik_tool_registry_t` | `shared` | `talloc_free(shared)` |
| Registry `entries[]` | `registry` | Automatic (child) |
| Entry `name`, `path` | `registry` | Automatic (child) |
| Entry `schema_doc` | `registry` (via allocator) | Automatic (child) |
| `ik_tool_discovery_state_t` | `shared` | `_finalize()` → `talloc_free()` |
| Discovery buffers | `state` | Automatic (child) |
| `ik_tool_external_exec` result | Caller's `ctx` | Caller (or steal) |
| Wrapper function results | Caller's `ctx` | Caller (or steal) |
| Thread execution result | `tool_thread_ctx` | `talloc_steal()` then `talloc_free(ctx)` |
| Errors from failable functions | Caller's `ctx` | Caller |
