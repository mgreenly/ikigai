# Task: Replace Debug Pipes with Logger in repl_event_handlers.c

## Overview

Replace all debug pipe fprintf calls in `src/repl_event_handlers.c` with JSONL logger calls. This file uses both `db_debug_pipe` and `openai_debug_pipe`. The debug pipes were a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

This ensures that all build output, test results, and error messages are immediately visible and can be analyzed. Running tools in the background will hide critical diagnostic information.

## Current State

`src/repl_event_handlers.c` uses debug pipes in two locations:

### Location 1: `persist_assistant_msg()` (lines 158-164)
```c
if (is_err(&db_res)) {
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist assistant message to database: %s\n",
                error_message(db_res.err));
    }
    talloc_free(db_res.err);
}
```

### Location 2: `handle_request_success()` (lines 205-211)
```c
if (repl->shared->openai_debug_pipe && repl->shared->openai_debug_pipe->write_end) {
    size_t len = strlen(repl->current->assistant_response);
    fprintf(repl->shared->openai_debug_pipe->write_end,
            len > 80 ? "<< ASSISTANT: %.77s...\n" : "<< ASSISTANT: %s\n",
            repl->current->assistant_response);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

## Requirements

1. Replace all debug pipe fprintf calls with appropriate JSONL logger calls
2. Use the **DI-based logger pattern** (NOT legacy global functions)
   - Logger is accessed via `repl->shared->logger`
   - Use `ik_logger_warn_json(repl->shared->logger, doc)` for DB warnings
   - Use `ik_logger_debug_json(repl->shared->logger, doc)` for OpenAI debug
3. Include structured data appropriate to each event type
4. Remove the entire debug pipe conditional blocks (not just the fprintf)
5. Keep the `talloc_free(db_res.err)` in Location 1 (the error must still be freed)

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
- READ: src/repl_event_handlers.c (find all debug pipe fprintf calls - lines 158-166 and 205-211)
- READ: src/logger.h (JSONL logger API - focus on DI-based ik_logger_*_json functions)
- READ: src/repl.c lines 155-162 (example of DI-based logger usage pattern)
- READ: src/shared.c lines 88-94 (another DI-based logger usage example)

## Implementation Pattern

### Location 1 Replacement: DB warning in `persist_assistant_msg()`

**Before:**
```c
if (is_err(&db_res)) {
    if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->db_debug_pipe->write_end,
                "Warning: Failed to persist assistant message to database: %s\n",
                error_message(db_res.err));
    }
    talloc_free(db_res.err);
}
```

**After:**
```c
if (is_err(&db_res)) {
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(log_doc, root, "event", "db_persist_failed");
    yyjson_mut_obj_add_str(log_doc, root, "operation", "persist_assistant_msg");
    yyjson_mut_obj_add_str(log_doc, root, "error", error_message(db_res.err));
    ik_logger_warn_json(repl->shared->logger, log_doc);
    talloc_free(db_res.err);
}
```

### Location 2 Replacement: OpenAI response in `handle_request_success()`

**Before:**
```c
if (repl->shared->openai_debug_pipe && repl->shared->openai_debug_pipe->write_end) {
    size_t len = strlen(repl->current->assistant_response);
    fprintf(repl->shared->openai_debug_pipe->write_end,
            len > 80 ? "<< ASSISTANT: %.77s...\n" : "<< ASSISTANT: %s\n",
            repl->current->assistant_response);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

**After:**
```c
yyjson_mut_doc *log_doc = ik_log_create();
yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
yyjson_mut_obj_add_str(log_doc, root, "event", "assistant_response");
yyjson_mut_obj_add_int(log_doc, root, "length", (int64_t)strlen(repl->current->assistant_response));
// Truncate long responses for log readability (same as original behavior)
if (strlen(repl->current->assistant_response) > 80) {
    char truncated[81];
    snprintf(truncated, sizeof(truncated), "%.77s...", repl->current->assistant_response);
    yyjson_mut_obj_add_str(log_doc, root, "preview", truncated);
} else {
    yyjson_mut_obj_add_str(log_doc, root, "content", repl->current->assistant_response);
}
ik_logger_debug_json(repl->shared->logger, log_doc);
```

**Note:** The logger functions take ownership of the doc and free it internally.

## Dependencies

This task is part of the "Debug Pipe Migration" group. There are no hard dependencies within this group - each file can be migrated independently. However, the Logger Infrastructure tasks should be completed first (they are earlier in order.json).

## Acceptance Criteria
- [ ] No db_debug_pipe fprintf calls remain in repl_event_handlers.c
- [ ] No openai_debug_pipe fprintf calls remain in repl_event_handlers.c
- [ ] No fflush calls for debug pipes remain
- [ ] All output logged via JSONL logger with appropriate levels
- [ ] Log entries include structured event data
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Testing

1. Build and run tests:
   ```bash
   make clean && make check
   ```

2. Run lint:
   ```bash
   make lint
   ```

3. Verify no debug pipe references remain:
   ```bash
   grep -n "debug_pipe" src/repl_event_handlers.c
   # Should return no matches
   ```

## Important Notes

- Do NOT remove the debug_pipe infrastructure from other files - this task only migrates repl_event_handlers.c
- Do NOT change the debug_pipe fields in shared.h - they are still used by other files
- The logger already includes "logger.h" - no new #include needed

## Agent Configuration
- Model: sonnet
- Thinking: thinking
