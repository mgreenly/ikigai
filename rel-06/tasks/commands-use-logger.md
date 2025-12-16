# Task: Replace Debug Pipe with Logger in commands.c

## Overview

Replace `fprintf(repl->shared->db_debug_pipe->write_end, ...)` calls in `src/commands.c` with JSONL logger calls. The debug pipe was a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Current State

`src/commands.c` has TWO debug pipe fprintf locations, both in the `cmd_clear()` function:

1. **Lines 205-212** - Clear event persistence failure:
   ```c
   if (is_err(&db_res)) {
       // Log error but don't crash - memory state is authoritative
       if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
           fprintf(repl->shared->db_debug_pipe->write_end,
                   "Warning: Failed to persist clear event to database: %s\n",
                   error_message(db_res.err));
       }
       talloc_free(db_res.err);
   }
   ```

2. **Lines 225-233** - System message persistence failure:
   ```c
   if (is_err(&system_res)) {
       // Log error but don't crash - memory state is authoritative
       if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
           fprintf(repl->shared->db_debug_pipe->write_end,
                   "Warning: Failed to persist system message to database: %s\n",
                   error_message(system_res.err));
       }
       talloc_free(system_res.err);
   }
   ```

**Note**: There are NO fflush calls to remove - the current code does not flush after writing.

**Note**: `#include "logger.h"` is already present (line 15), no need to add it.

## Requirements

1. Replace BOTH debug pipe fprintf blocks with `ik_log_warn_json()` calls
2. Include structured data: event type, command, operation, error message
3. Completely remove the debug pipe null checks and fprintf blocks

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

## Pre-read Source
- READ: src/commands.c (find debug pipe fprintf calls)
- READ: src/logger.h (JSONL logger API)
- READ: src/logger.c (understand ik_log_*_json usage)
- READ: src/debug_pipe.h (understand what's being replaced)

## Implementation Pattern

### Location 1: Clear event persistence failure (lines 205-212)

Replace this entire block:
```c
if (is_err(&db_res)) {
    // Log error but don't crash - memory state is authoritative
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist clear event to database: %s\n",
                error_message(db_res.err));
    }
    talloc_free(db_res.err);
}
```

With:
```c
if (is_err(&db_res)) {
    // Log error but don't crash - memory state is authoritative
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");
    yyjson_mut_obj_add_str(log_doc, log_root, "command", "clear");
    yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_clear");
    yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));
    ik_log_warn_json(log_doc);
    talloc_free(db_res.err);
}
```

### Location 2: System message persistence failure (lines 225-233)

Replace this entire block:
```c
if (is_err(&system_res)) {
    // Log error but don't crash - memory state is authoritative
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist system message to database: %s\n",
                error_message(system_res.err));
    }
    talloc_free(system_res.err);
}
```

With:
```c
if (is_err(&system_res)) {
    // Log error but don't crash - memory state is authoritative
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");
    yyjson_mut_obj_add_str(log_doc, log_root, "command", "clear");
    yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_system_message");
    yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(system_res.err));
    ik_log_warn_json(log_doc);
    talloc_free(system_res.err);
}
```

## Acceptance Criteria
- [ ] Clear event persistence failure uses ik_log_warn_json() instead of debug pipe fprintf
- [ ] System message persistence failure uses ik_log_warn_json() instead of debug pipe fprintf
- [ ] No debug pipe references remain in cmd_clear() function
- [ ] Log entries include structured data: event, command, operation, error
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Dependencies

This task is part of the "Debug Pipe Migration" group. It depends on:
- **logger-lifecycle.md** (must be done first) - Sets up global logger initialization

This task can be done in parallel with other debug pipe migration tasks:
- commands-mark-use-logger.md
- commands-agent-use-logger.md
- repl-actions-llm-use-logger.md
- etc.

## Notes

- Uses the **legacy global logger API** (`ik_log_warn_json`) rather than the DI-based API (`ik_logger_warn_json`)
- This is intentional for this migration phase - keeps changes minimal and consistent with other migration tasks
- The debug pipe null checks are completely removed since the global logger handles NULL gracefully
- Both locations are in the `cmd_clear()` function which handles the `/clear` command

## Agent Configuration
- Model: sonnet
- Thinking: thinking
