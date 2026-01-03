# External Process Execution

How external tool execution integrates with ikigai's existing async model.

## Current Tool Execution Model

ikigai already runs tools asynchronously using threads:

```
Main Loop (select)              Thread
─────────────────              ──────
poll tool_thread_complete  ←──  set complete=true
                                    ↑
                               ik_tool_dispatch() [blocking C code]
                                    ↑
ik_agent_start_tool_execution ──→ pthread_create
```

**Key insight:** The thread model already handles blocking operations. External tools just need to block in the thread instead of calling internal C functions.

## Integration Point

**File:** `src/repl_tool.c:43`

```c
// Current: calls internal C dispatcher
res_t result = ik_tool_dispatch(args->ctx, args->tool_name, args->arguments);
```

**Change to:**
```c
// New: look up tool in registry, execute external process
ik_tool_entry_t *entry = ik_tool_registry_lookup(registry, args->tool_name);
if (entry == NULL) {
    // Tool not found - return error
}
res_t result = ik_tool_external_exec(args->ctx, entry->path, args->arguments);
```

No changes to the main select() loop, threading, or completion polling.

## Bidirectional Pipe Pattern

External tools need stdin (params) and stdout (result):

```c
res_t ik_tool_external_exec(TALLOC_CTX *ctx, const char *tool_path, const char *params_json)
{
    int stdin_pipe[2], stdout_pipe[2];
    pipe(stdin_pipe);   // [0]=read, [1]=write
    pipe(stdout_pipe);

    pid_t pid = fork();
    if (pid == 0) {
        // Child: wire pipes, exec tool
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        char *argv[] = {(char *)tool_path, NULL};
        execv(tool_path, argv);
        _exit(127);  // exec failed
    }

    // Parent: close unused ends
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    // Write params to tool's stdin
    write(stdin_pipe[1], params_json, strlen(params_json));
    close(stdin_pipe[1]);  // Signal EOF

    // Read result from tool's stdout (blocking - fine in thread)
    char *result = read_all(ctx, stdout_pipe[0]);
    close(stdout_pipe[0]);

    // Wait for process
    int status;
    waitpid(pid, &status, 0);

    // Wrap result
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return OK(wrap_success(ctx, result));
    } else {
        return OK(wrap_failure(ctx, "Tool crashed", WEXITSTATUS(status)));
    }
}
```

## Timeout Handling

Add timeout with alarm or polling:

```c
// Set deadline
time_t deadline = time(NULL) + timeout_seconds;

// Non-blocking read with select
while (time(NULL) < deadline) {
    fd_set fds;
    FD_SET(stdout_pipe[0], &fds);
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};

    if (select(stdout_pipe[0] + 1, &fds, NULL, NULL, &tv) > 0) {
        // Read available data
    }
}

// Timeout - kill process
kill(pid, SIGKILL);
waitpid(pid, NULL, 0);
return OK(wrap_failure(ctx, "Tool timed out", "TOOL_TIMEOUT"));
```

## Tool Discovery

Discovery runs external processes (`tool --schema`) during startup using async parallel scan integrated with the REPL event loop. See `tool-discovery-execution.md` for the full specification.

## Files to Modify

| File | Change |
|------|--------|
| `src/tool_registry.c` | New: registry data structure |
| `src/tool_external_exec.c` | New: bidirectional pipe execution |
| `src/repl_tool.c:43` | Change dispatch to use registry + external exec |
| `src/repl_init.c` | Add registry creation and discovery |

## Files Unchanged

- `src/repl.c` - Main loop unchanged
- Threading model - Unchanged
- select() integration - Unchanged
- Completion polling - Unchanged

## Alternative Approaches Considered

### Option A: Thread + Blocking Pipes (Recommended)

What we're doing: fork/exec in the existing tool thread, block on pipes.

**Pros:**
- Zero changes to main loop or threading model
- Blocking I/O is simple and correct
- Timeout handling straightforward (kill + waitpid)
- Matches existing tool execution pattern exactly

**Cons:**
- One thread per tool execution (already the case)
- fork() has overhead (negligible for tool call frequency)

### Option B: Add Tool Pipes to select() Loop

Alternative: Add tool stdout pipes to the main select() fdset.

**Pros:**
- No threads needed for tool execution
- Unified async model

**Cons:**
- Requires rewriting tool execution flow
- Complicates main loop (more fd management)
- Must handle partial reads, buffering
- Bidirectional I/O harder (need to write params before reading)
- Existing thread model works fine - why change it?

**Verdict:** Unnecessary complexity. Threads already solve this.

### Option C: popen() with Shell Wrapper

Alternative: Use popen() with shell to handle pipes.

```c
popen("echo '{...}' | /path/to/tool", "r");
```

**Pros:**
- Single function call
- Shell handles stdin piping

**Cons:**
- Shell injection risk (must escape JSON)
- Extra process (shell)
- Less control over error handling
- Can't easily separate stdout/stderr

**Verdict:** Security risk and unnecessary shell overhead.

### Option D: posix_spawn() Instead of fork()

Alternative: Use posix_spawn() for process creation.

**Pros:**
- Faster than fork() on large processes (12x in benchmarks)
- Avoids fork() copy-on-write overhead

**Cons:**
- More complex API (file_actions, attributes)
- fork() overhead negligible at tool-call frequency
- Less portable (though POSIX, some edge cases)

**Verdict:** Premature optimization. fork() is fine. Can switch later if profiling shows need.

## Why Thread + Blocking Pipes Wins

1. **Minimal change** - Swap one function call, keep everything else
2. **Already proven** - Current tool execution uses this exact pattern
3. **Simple code** - fork/exec/pipe is standard Unix, well understood
4. **Correct by construction** - Blocking in thread can't deadlock main loop
5. **Easy timeout** - kill(pid) + waitpid, done

The existing architecture already solved async tool execution. External tools just change what happens inside the thread.

## Summary

External tools plug into the existing thread model:

1. Thread calls `ik_tool_external_exec()` instead of `ik_tool_dispatch()`
2. Function uses fork/exec with pipes (blocking)
3. Thread signals completion as before
4. Main loop harvests result as before

No architectural changes. Just swap internal dispatch for external process execution.
