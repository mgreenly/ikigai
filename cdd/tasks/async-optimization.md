# Task: Async Discovery Optimization

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/extended
**Depends on:** commands.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task converts tool discovery from blocking to async, so the terminal appears immediately while discovery runs in background.

## Pre-Read

**Skills:**
- `/load errors` - res_t patterns
- `/load style` - Code style
- `/load memory` - talloc ownership

**Plan:**
- `cdd/plan/architecture.md` - Phase 6 description
- `cdd/plan/tool-discovery-execution.md` - Async primitives
- `cdd/plan/integration-specification.md` - Phase 6 struct changes

**Source:**
- `src/tool_discovery.c` - Current blocking implementation (has async internals)
- `src/repl.c` - Event loop with select()
- `src/repl_init.c` - Current blocking discovery call

## Libraries

Use only existing libraries. No new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] Commands complete
- [ ] Blocking discovery works correctly

## Objective

Expose async discovery primitives and integrate with REPL event loop:

1. Make async functions public (currently static in tool_discovery.c)
2. Add discovery state to REPL context
3. Integrate discovery fds with event loop select()
4. Handle user submit during discovery (wait with spinner)

## Async Primitives to Expose

In `src/tool_discovery.h`, add:

```c
typedef struct ik_tool_discovery_state_t ik_tool_discovery_state_t;

// Start async scan - spawn all tools, return immediately
res_t ik_tool_discovery_start(TALLOC_CTX *ctx,
                               const char *system_dir,
                               const char *user_dir,
                               ik_tool_registry_t *registry,
                               ik_tool_discovery_state_t **out_state);

// Add discovery fds to select() fdsets
void ik_tool_discovery_add_fds(ik_tool_discovery_state_t *state,
                                fd_set *read_fds,
                                int *max_fd);

// Process ready fds after select()
res_t ik_tool_discovery_process_fds(ik_tool_discovery_state_t *state,
                                     fd_set *read_fds);

// Check if scan is complete
bool ik_tool_discovery_is_complete(ik_tool_discovery_state_t *state);

// Finalize scan - cleanup resources
void ik_tool_discovery_finalize(ik_tool_discovery_state_t *state);
```

## Struct Changes

### src/repl.h

Add to `ik_repl_ctx_t`:

```c
ik_tool_discovery_state_t *tool_discovery;  // NULL when not in progress
```

## Integration Changes

### src/repl_init.c

Replace blocking `ik_tool_discovery_run()` with:

```c
shared->tool_registry = ik_tool_registry_create(shared);
res_t start_result = ik_tool_discovery_start(shared,
    PREFIX "/libexec/ikigai",
    "~/.ikigai/tools",
    shared->tool_registry,
    &repl->tool_discovery);
if (is_ok(&start_result)) {
    shared->tool_scan_state = TOOL_SCAN_IN_PROGRESS;
} else {
    shared->tool_scan_state = TOOL_SCAN_FAILED;
    // Continue without tools
}
```

### src/repl.c

In main event loop, when `tool_scan_state == TOOL_SCAN_IN_PROGRESS`:

1. Call `ik_tool_discovery_add_fds()` to add discovery fds to select fdset
2. After select(), call `ik_tool_discovery_process_fds()` for ready fds
3. Check `ik_tool_discovery_is_complete()`:
   - If true: `ik_tool_discovery_finalize()`, set `tool_scan_state = TOOL_SCAN_COMPLETE`
   - Set `repl->tool_discovery = NULL`

### src/repl_actions_llm.c

Before submitting to LLM, check if discovery in progress:

```c
if (repl->shared->tool_scan_state == TOOL_SCAN_IN_PROGRESS) {
    // Show spinner, block until discovery completes
    while (!ik_tool_discovery_is_complete(repl->tool_discovery)) {
        // Wait for discovery (simplified blocking wait)
        // Or integrate with event loop for non-blocking wait
    }
    ik_tool_discovery_finalize(repl->tool_discovery);
    repl->tool_discovery = NULL;
    repl->shared->tool_scan_state = TOOL_SCAN_COMPLETE;
}
```

## Behavior After Change

1. Terminal appears immediately on startup
2. User can type while discovery runs in background
3. If user submits before discovery complete: show spinner, wait
4. Discovery typically completes in <1 second
5. No race conditions - registry immutable after discovery complete

## Testing

1. Startup is visibly faster (terminal appears before tools loaded)
2. Submit during discovery waits correctly
3. All tools still discovered correctly
4. No hangs or race conditions
5. `/refresh` still works (uses blocking API)

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(async-optimization.md): [success|partial|failed] - async discovery

Terminal appears immediately. Discovery runs in background.
User submit waits if discovery in progress.
EOF
)"
```

Report status:
- Success: `/task-done async-optimization.md`
- Partial/Failed: `/task-fail async-optimization.md`

## Postconditions

- [ ] Terminal appears immediately on startup
- [ ] Discovery runs in background
- [ ] Submit waits if discovery in progress
- [ ] All 6 tools still discovered
- [ ] `make check` passes
- [ ] No race conditions or hangs
- [ ] All changes committed
- [ ] Working copy is clean
