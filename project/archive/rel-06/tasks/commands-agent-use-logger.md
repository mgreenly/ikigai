# Task: Replace Debug Pipe with Logger in commands_agent.c

## Overview

Replace the single `fprintf(repl->shared->db_debug_pipe->write_end, ...)` call in `src/commands_agent.c` with a JSONL logger call. The debug pipe was a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Current State

`src/commands_agent.c` has exactly ONE debug pipe usage at lines 97-101, in the `handle_fork_prompt()` function:

```c
// Lines 96-103 in handle_fork_prompt()
if (is_err(&db_res)) {     // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {     // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
        fprintf(repl->shared->db_debug_pipe->write_end,     // LCOV_EXCL_LINE
                "Warning: Failed to persist user message to database: %s\n",     // LCOV_EXCL_LINE
                error_message(db_res.err));     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    talloc_free(db_res.err);     // LCOV_EXCL_LINE
}     // LCOV_EXCL_LINE
```

**Note**: There is NO fflush call after this fprintf (unlike some other files).

## Requirements

1. Add `#include "logger.h"` to the includes (it's not currently included)
2. Replace the debug pipe fprintf with a JSONL logger call using the DI-based logger API
3. Use `repl->shared->logger` (the injected logger instance), NOT the legacy global API
4. Include structured data: event type, operation, error message
5. Preserve the LCOV_EXCL annotations for code coverage

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
- READ: src/commands_agent.c (find the single debug pipe fprintf call at line 98)
- READ: src/logger.h (JSONL logger API - note both legacy and DI-based functions)
- READ: src/logger.c (understand ik_logger_*_json usage - the DI-based versions)
- READ: src/shared.h (see logger field: `ik_logger_t *logger`)

## Implementation Pattern

### Add Include
Add to the includes section (after existing includes):
```c
#include "logger.h"
```

### Replace Debug Pipe Call
Replace the entire if block (lines 97-101):
```c
if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {     // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
    fprintf(repl->shared->db_debug_pipe->write_end,     // LCOV_EXCL_LINE
            "Warning: Failed to persist user message to database: %s\n",     // LCOV_EXCL_LINE
            error_message(db_res.err));     // LCOV_EXCL_LINE
}     // LCOV_EXCL_LINE
```

With (using DI-based logger via `repl->shared->logger`):
```c
yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_warning");     // LCOV_EXCL_LINE
yyjson_mut_obj_add_str(log_doc, log_root, "operation", "fork_prompt_persist");     // LCOV_EXCL_LINE
yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));     // LCOV_EXCL_LINE
ik_logger_warn_json(repl->shared->logger, log_doc);     // LCOV_EXCL_LINE
```

**Important API notes:**
- Use `ik_logger_warn_json(repl->shared->logger, doc)` (DI-based), NOT `ik_log_warn_json(doc)` (legacy global)
- The logger takes ownership of the doc and frees it
- Variable names use `log_doc`/`log_root` to avoid conflicts with other variables in scope

## Acceptance Criteria
- [ ] `#include "logger.h"` added to includes
- [ ] No debug pipe fprintf calls remain in commands_agent.c
- [ ] Warning logged via DI-based JSONL logger (`ik_logger_warn_json`)
- [ ] Log entry includes structured data: event, operation, error
- [ ] LCOV_EXCL annotations preserved (this is error-path code)
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Dependencies

This task is part of the "Debug Pipe Migration" group. Prerequisites:
- `logger-lifecycle.md` - ensures logger is properly initialized early

## Scope

This is a minimal task - only ONE fprintf call to replace. Should be quick to complete.

## Agent Configuration
- Model: sonnet
- Thinking: thinking
