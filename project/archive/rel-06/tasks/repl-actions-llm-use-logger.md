# Task: Replace Debug Pipe with Logger in repl_actions_llm.c

## Overview

Replace `fprintf(repl->shared->db_debug_pipe->write_end, ...)` calls in `src/repl_actions_llm.c` with JSONL logger calls. The debug pipe was a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Current State

`src/repl_actions_llm.c` has ONE debug pipe fprintf location in the `send_to_llm_()` function:

**Lines 95-101** - User message persistence failure:
```c
if (is_err(&db_res)) {
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist user message to database: %s\n",
                error_message(db_res.err));
    }
    talloc_free(db_res.err);
}
```

**Note**: There is NO fflush call to remove - the current code does not flush after writing.

**Note**: `#include "logger.h"` is NOT present and must be added (after existing includes around line 19).

## Requirements

1. Add `#include "logger.h"` to the includes
2. Replace the debug pipe fprintf block with `ik_log_warn_json()` call
3. Include structured data: event type, function context, operation, error message
4. Completely remove the debug pipe null checks and fprintf block

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
- READ: src/repl_actions_llm.c (find debug pipe fprintf calls)
- READ: src/logger.h (JSONL logger API)
- READ: src/logger.c (understand ik_log_*_json usage)
- READ: src/debug_pipe.h (understand what's being replaced)

## Implementation

### Step 1: Add include

Add this include after the existing includes (around line 19, after `#include <string.h>`):
```c
#include "logger.h"
```

### Step 2: Replace debug pipe block

Replace this entire block (lines 95-101):
```c
if (is_err(&db_res)) {
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist user message to database: %s\n",
                error_message(db_res.err));
    }
    talloc_free(db_res.err);
}
```

With:
```c
if (is_err(&db_res)) {
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");
    yyjson_mut_obj_add_str(log_doc, log_root, "context", "send_to_llm");
    yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_user_message");
    yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));
    ik_log_warn_json(log_doc);
    talloc_free(db_res.err);
}
```

## Acceptance Criteria
- [ ] `#include "logger.h"` added to includes
- [ ] Debug pipe fprintf block replaced with ik_log_warn_json()
- [ ] No debug pipe references remain in send_to_llm_() function
- [ ] Log entry includes structured data: event, context, operation, error
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Dependencies

This task is part of the "Debug Pipe Migration" group. It depends on:
- **logger-lifecycle.md** (must be done first) - Sets up global logger initialization

This task can be done in parallel with other debug pipe migration tasks:
- commands-mark-use-logger.md
- commands-use-logger.md
- commands-agent-use-logger.md
- repl-event-handlers-use-logger.md
- repl-tool-use-logger.md
- repl-callbacks-use-logger.md

## Notes

- Uses the **legacy global logger API** (`ik_log_warn_json`) rather than the DI-based API (`ik_logger_warn_json`)
- This is intentional for this migration phase - keeps changes minimal and consistent with other migration tasks
- The debug pipe null checks are completely removed since the global logger handles NULL gracefully
- There is only ONE debug pipe usage in this file, in the `send_to_llm_()` function

## Agent Configuration
- Model: sonnet
- Thinking: thinking
