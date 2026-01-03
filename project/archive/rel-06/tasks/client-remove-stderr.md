# Task: Remove stderr Output from client.c

## Overview

Replace all `error_fprintf(stderr, ...)` calls in `src/client.c` with JSONL logger calls. The application runs in terminal alternate buffer mode where stderr is invisible to users - all error output must go through the logger.

## Why This Matters

1. **Alternate buffer mode**: ikigai runs in terminal alternate buffer mode where stdout/stderr are invisible
2. **Consistency**: All application output should go through the structured JSONL logger
3. **Diagnostics**: Errors written to stderr are lost; errors in the log can be reviewed

## Execution Constraints

**CRITICAL**: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.

## Current State

`src/client.c` uses `error_fprintf(stderr, ...)` at these exact locations:
- **Line 31**: Config loading error (`cfg_result.err`)
- **Line 44**: Shared context init error (`result.err`)
- **Line 56**: REPL init error (`result.err`) - after terminal cleanup
- **Line 72**: REPL run error (`result.err`) - after full cleanup

The `error_fprintf()` function (from `error.h`) formats error messages like:
```
Error: <message> [<file>:<line>]
```

**Important**: After `logger-lifecycle.md` is completed, the logger will be created BEFORE config loading, so `logger` will be available at all four error locations.

## Requirements

1. Replace each `error_fprintf(stderr, err)` with the DI-based JSONL logger call
2. Use `ik_logger_error_json(logger, doc)` (NOT the legacy `ik_log_error_json()`)
3. Include the same information in the JSON structure:
   - Error message
   - Error code
   - Source file and line from the err_t
4. The `logger` variable will be available after `logger-lifecycle.md` moves logger creation earlier

### Special Case: Line 72 (Post-Cleanup Error)

The error at line 72 occurs AFTER `ik_repl_cleanup()` when the terminal has been restored to primary buffer. The comment even says "Print error AFTER cleanup (terminal restored to primary buffer)".

However, after `logger-lifecycle.md`, the logger will still be valid at this point (it's destroyed LAST). So this error should also go through the logger for consistency. The user can view the log after the session ends.

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
- project/error_handling.md (err_t structure, error_message() function)

## Pre-read Source
- READ: src/client.c (find all error_fprintf calls, understand control flow)
- READ: src/error.h (err_t structure, error_fprintf definition, error_message())
- READ: src/logger.h (JSONL logger API - note DI functions vs legacy)
- READ: src/logger.c (understand ik_log_create, ik_logger_error_json usage)
- READ (after logger-lifecycle.md): The modified client.c to see logger placement

## Implementation Pattern

**Important**: Use the DI-based logger API (`ik_logger_*_json`), NOT the legacy global API (`ik_log_*_json`).

Replace this pattern:
```c
if (is_err(&result)) {
    error_fprintf(stderr, result.err);
    return EXIT_FAILURE;
}
```

With:
```c
if (is_err(&result)) {
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "startup_error");
    yyjson_mut_obj_add_str(doc, root, "message", error_message(result.err));
    yyjson_mut_obj_add_int(doc, root, "code", result.err->code);
    yyjson_mut_obj_add_str(doc, root, "file", result.err->file);
    yyjson_mut_obj_add_int(doc, root, "line", result.err->line);
    ik_logger_error_json(logger, doc);  // DI API - takes ownership, frees doc
    return EXIT_FAILURE;
}
```

### Expected JSON Output
```json
{"level":"error","timestamp":"...","logline":{"event":"startup_error","message":"Failed to load config","code":3,"file":"config.c","line":42}}
```

## Dependencies

**CRITICAL**: This task MUST be done AFTER `logger-lifecycle.md` is complete.

The `logger-lifecycle.md` task:
1. Moves logger creation to immediately after `getcwd()` (before config loading)
2. Ensures `logger` variable is available at all error points
3. Keeps the logger valid until after all cleanup

Without this prerequisite, the first error (config loading) would have no logger available.

## Acceptance Criteria
- [ ] No `error_fprintf(stderr, ...)` calls remain in client.c
- [ ] No `fprintf(stderr, ...)` calls in client.c
- [ ] All 4 errors are logged via `ik_logger_error_json(logger, doc)`
- [ ] Error log entries include: event type, message, code, file, line
- [ ] Remove the obsolete comment about "Print error AFTER cleanup" at line 70
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Agent Configuration
- Model: sonnet
- Thinking: thinking
