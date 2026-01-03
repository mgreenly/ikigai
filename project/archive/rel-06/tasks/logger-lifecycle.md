# Task: Logger Lifecycle - First In, Last Out

## Overview

The JSONL logger must be initialized very early in `client.c` and be the very last thing destroyed. This ensures all application events, including startup errors and shutdown, are captured in the log.

**Note**: Panic handler integration is handled separately in `panic-log-attempt.md`. This task focuses on logger lifecycle and establishing the global logger pointer that the panic task will use.

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

This ensures that all command output, test results, and build errors are immediately visible and can be analyzed. Running commands in the background will hide critical diagnostic information.

## Why This Matters

1. **Alternate buffer mode**: ikigai runs in terminal alternate buffer mode where stdout/stderr are invisible to users
2. **Diagnostics**: Without logging, startup failures leave no trace
3. **Completeness**: Session logs should have a clear start and end marker for analysis

## Current State

In `src/client.c`, the initialization order is:
1. Line 19: Create `root_ctx` (talloc root)
2. Line 23-26: Get current working directory (`getcwd`)
3. Line 29-35: Load configuration (`ik_cfg_load`)
4. Line 38: Create logger (`ik_logger_create`)
5. Line 41-47: Create shared context (`ik_shared_ctx_init`)
6. Line 50-59: Create REPL (`ik_repl_init`)

The logger is created AFTER config loading, so config load errors use `error_fprintf(stderr, ...)` which is invisible.

## Requirements

### 1. Logger Early Initialization
- Move logger creation to immediately after `getcwd()` (before config loading)
- Log a "session_start" event immediately after logger creation
- The global logger pointer (`g_panic_logger`) must be set right after creation

**Why after getcwd()**: The logger needs the working directory to create `.ikigai/logs/`. This is the minimal bootstrap required.

### 2. Logger Last Destruction
- Ensure logger cleanup happens after all other cleanup
- Log a "session_end" event just before destruction
- Set `g_panic_logger = NULL` before destroying the logger
- The logger must remain valid through all cleanup paths

### 3. Global Logger Pointer
- Declare `extern ik_logger_t *volatile g_panic_logger` in `panic.h`
- Define `ik_logger_t *volatile g_panic_logger = NULL` in `panic.c`
- Set it in `client.c` after logger creation
- Clear it in `client.c` before logger destruction

This pointer will be used by `panic-log-attempt.md` task later.

### 4. Separate Logger from talloc_free(root_ctx)
Currently, the logger is parented to `root_ctx`, so it gets freed when `talloc_free(root_ctx)` is called. To ensure the logger is destroyed LAST:
- Either: Use a separate talloc context for the logger
- Or: Manually destroy the logger after `talloc_free(root_ctx)`
- Or: Re-parent the logger with `talloc_steal(NULL, logger)` before cleanup

Recommended approach: Create the logger with `talloc_new(NULL)` as its own root, and manually free it at the very end.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/database.md
- .agents/skills/errors.md
- .agents/skills/log.md
- .agents/skills/makefile.md
- .agents/skills/naming.md
- .agents/skills/quality.md
- .agents/skills/scm.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- project/memory.md (talloc ownership)
- project/error_handling.md (PANIC patterns)

## Pre-read Source
- READ: src/client.c (current initialization order)
- READ: src/panic.c (g_term_ctx_for_panic pattern to follow)
- READ: src/panic.h (where g_panic_logger should be declared)
- READ: src/logger.h (logger API)
- READ: src/logger.c (logger implementation, understand ik_logger_*_json functions)

## Implementation Notes

### Actual Logger API (from logger.h)
```c
// Creation - returns logger directly, PANICs on failure
ik_logger_t *ik_logger_create(TALLOC_CTX *ctx, const char *working_dir);

// DI-based JSONL API (use these with explicit logger)
yyjson_mut_doc *ik_log_create(void);
void ik_logger_info_json(ik_logger_t *logger, yyjson_mut_doc *doc);
void ik_logger_error_json(ik_logger_t *logger, yyjson_mut_doc *doc);
// ... etc
```

### Session Start Event Structure
```json
{"level":"info","timestamp":"...","logline":{"event":"session_start","cwd":"/path/to/dir"}}
```

### Session End Event Structure
```json
{"level":"info","timestamp":"...","logline":{"event":"session_end","exit_code":0}}
```

### Example Implementation Sketch
```c
int main(void)
{
    // Minimal bootstrap
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        PANIC("Failed to get current working directory");
    }

    // Logger first (its own talloc root for independent lifetime)
    void *logger_ctx = talloc_new(NULL);
    if (logger_ctx == NULL) PANIC("Failed to create logger context");

    ik_logger_t *logger = ik_logger_create(logger_ctx, cwd);
    g_panic_logger = logger;  // Enable panic logging

    // Log session start
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "session_start");
    yyjson_mut_obj_add_str(doc, root, "cwd", cwd);
    ik_logger_info_json(logger, doc);

    // Now create app root_ctx and continue with normal init
    void *root_ctx = talloc_new(NULL);
    // ... rest of initialization ...

    // At shutdown:
    talloc_free(root_ctx);  // Free all app resources first

    // Log session end
    doc = ik_log_create();
    root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "session_end");
    yyjson_mut_obj_add_int(doc, root, "exit_code", exit_code);
    ik_logger_info_json(logger, doc);

    g_panic_logger = NULL;   // Disable panic logging
    talloc_free(logger_ctx); // Logger last

    return exit_code;
}
```

## Acceptance Criteria
- [ ] Logger created immediately after getcwd(), before config loading
- [ ] Global `g_panic_logger` declared in panic.h, defined in panic.c
- [ ] `g_panic_logger` set after logger creation in client.c
- [ ] "session_start" event logged with cwd immediately after logger creation
- [ ] "session_end" event logged with exit_code just before logger destruction
- [ ] `g_panic_logger = NULL` before logger destruction
- [ ] Logger destruction is the last cleanup operation (separate talloc context)
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Notes
- The `client-remove-stderr.md` task depends on this task to provide an early logger
- The `panic-log-attempt.md` task depends on the `g_panic_logger` global set up here

## Agent Configuration
- Model: sonnet
- Thinking: thinking
