# Task: Replace Debug Pipe with Logger in commands_mark.c

## Overview

Replace `fprintf(repl->shared->db_debug_pipe->write_end, ...)` calls in `src/commands_mark.c` with JSONL logger calls. The debug pipe was a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Current State

`src/commands_mark.c` has TWO debug pipe fprintf locations:

1. **Lines 99-103** in `ik_cmd_mark()`:
   ```c
   if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
       fprintf(repl->shared->db_debug_pipe->write_end,
               "Warning: Failed to persist mark event to database: %s\n",
               error_message(db_res.err));
   }
   ```

2. **Lines 158-162** in `ik_cmd_rewind()`:
   ```c
   if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
       fprintf(repl->shared->db_debug_pipe->write_end,
               "Warning: Failed to persist rewind event to database: %s\n",
               error_message(db_res.err));
   }
   ```

**Note**: There are NO fflush calls to remove - the current code does not flush after writing.

## Requirements

1. Add `#include "logger.h"` at the top of the file
2. Replace BOTH debug pipe fprintf blocks with `ik_log_warn_json()` calls
3. Include structured data: event type, operation, error message
4. Completely remove the debug pipe null checks and fprintf blocks

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
- READ: src/commands_mark.c (find debug pipe fprintf calls)
- READ: src/logger.h (JSONL logger API)
- READ: src/logger.c (understand ik_log_warn_json usage)
- READ: src/debug_pipe.h (understand what's being replaced)

## Implementation Pattern

### Location 1: ik_cmd_mark() (lines 97-104)

Replace this entire block:
```c
if (is_err(&db_res)) {
    // Log error but don't crash - memory state is authoritative
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist mark event to database: %s\n",
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
    yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_mark");
    yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));
    ik_log_warn_json(log_doc);
    talloc_free(db_res.err);
}
```

### Location 2: ik_cmd_rewind() (lines 156-164)

Replace this entire block:
```c
if (is_err(&db_res)) {
    // Log error but don't crash - memory state is authoritative
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist rewind event to database: %s\n",
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
    yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_rewind");
    yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));
    ik_log_warn_json(log_doc);
    talloc_free(db_res.err);
}
```

### Include Addition

Add this include near the top of the file (after existing includes):
```c
#include "logger.h"
```

## Acceptance Criteria
- [ ] `#include "logger.h"` added to commands_mark.c
- [ ] ik_cmd_mark() uses ik_log_warn_json() instead of debug pipe fprintf
- [ ] ik_cmd_rewind() uses ik_log_warn_json() instead of debug pipe fprintf
- [ ] No debug pipe references remain in commands_mark.c
- [ ] Log entries include structured data: event, operation, error
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Dependencies

This task is part of the "Debug Pipe Migration" group. It depends on:
- **logger-lifecycle.md** (must be done first) - Sets up global logger initialization

This task can be done in parallel with other debug pipe migration tasks:
- commands-use-logger.md
- commands-agent-use-logger.md
- repl-actions-llm-use-logger.md
- etc.

## Notes

- Uses the **legacy global logger API** (`ik_log_warn_json`) rather than the DI-based API (`ik_logger_warn_json`)
- This is intentional for this migration phase - keeps changes minimal and consistent with other migration tasks
- The debug pipe null checks are completely removed since the global logger handles NULL gracefully

## Agent Configuration
- Model: sonnet
- Thinking: thinking
