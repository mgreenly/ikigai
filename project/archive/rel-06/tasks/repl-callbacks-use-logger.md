# Task: Replace Debug Pipe with Logger in repl_callbacks.c

## Overview

Replace `fprintf(repl->shared->openai_debug_pipe->write_end, ...)` calls in `src/repl_callbacks.c` with JSONL logger calls. The debug pipe was a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Dependencies

This task should be done AFTER `logger-lifecycle.md` to ensure robust logger infrastructure.

## Current State

`src/repl_callbacks.c` function `ik_repl_http_completion_callback()` (lines 101-195) has debug pipe output at lines 109-128:

```c
// Debug output for response metadata
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< RESPONSE: type=%s",
            completion->type == IK_HTTP_SUCCESS ? "success" : "error");
    if (completion->type == IK_HTTP_SUCCESS) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                ", model=%s, finish=%s, tokens=%d",
                completion->model ? completion->model : "(null)",
                completion->finish_reason ? completion->finish_reason : "(null)",
                completion->completion_tokens);
    }
    if (completion->tool_call != NULL) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                ", tool_call=%s(%s)",
                completion->tool_call->name,
                completion->tool_call->arguments);
    }
    fprintf(repl->shared->openai_debug_pipe->write_end, "\n");
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

This outputs response metadata like: `<< RESPONSE: type=success, model=gpt-4, finish=stop, tokens=150`

## Requirements

1. Replace the entire debug pipe block with a single `ik_logger_debug_json()` call (DI pattern)
2. Access logger via: `repl->shared->logger` (see `src/shared.h` line 35)
3. Include structured data: event type, response type, all metadata fields
4. Remove the NULL check for debug_pipe - the logger API handles NULL logger gracefully
5. Remove the fflush call (logger handles flushing internally)

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
- READ: src/repl_callbacks.c (find all debug pipe fprintf calls)
- READ: src/logger.h (JSONL logger API)
- READ: src/logger.c (understand ik_log_debug_json usage)
- READ: src/debug_pipe.h (understand what's being replaced)

## Implementation Pattern

Replace the entire debug pipe block (lines 109-128) with:

```c
// Log response metadata via JSONL logger
{
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "openai_response");
    yyjson_mut_obj_add_str(doc, root, "type",
                           completion->type == IK_HTTP_SUCCESS ? "success" : "error");

    if (completion->type == IK_HTTP_SUCCESS) {
        yyjson_mut_obj_add_str(doc, root, "model",
                               completion->model ? completion->model : "(null)");
        yyjson_mut_obj_add_str(doc, root, "finish_reason",
                               completion->finish_reason ? completion->finish_reason : "(null)");
        yyjson_mut_obj_add_int(doc, root, "completion_tokens", completion->completion_tokens);
    }

    if (completion->tool_call != NULL) {
        yyjson_mut_obj_add_str(doc, root, "tool_call_name", completion->tool_call->name);
        yyjson_mut_obj_add_str(doc, root, "tool_call_args", completion->tool_call->arguments);
    }

    // DI pattern: use explicit logger from shared context
    ik_logger_debug_json(repl->shared->logger, doc);
}
```

**Key points**:
- Use `ik_logger_debug_json(logger, doc)` (DI pattern), NOT `ik_log_debug_json(doc)` (legacy global)
- Access logger via `repl->shared->logger`
- No NULL check needed - `ik_logger_debug_json()` handles NULL logger gracefully (see logger.c line 448)
- Wrap in a block `{}` to scope the local variables
- The logger API takes ownership of `doc` and frees it

## Acceptance Criteria
- [ ] No debug pipe fprintf calls remain in repl_callbacks.c
- [ ] No fflush calls for debug pipe remain
- [ ] All OpenAI response debug info logged via JSONL logger using DI pattern
- [ ] Log entries include structured event data (event, type, model, finish_reason, completion_tokens, tool_call info)
- [ ] Uses `ik_logger_debug_json(repl->shared->logger, doc)` NOT `ik_log_debug_json(doc)`
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Verification

After implementation, the log output (in `.ikigai/logs/current.log`) should contain entries like:
```json
{"level":"debug","timestamp":"2024-...","logline":{"event":"openai_response","type":"success","model":"gpt-4","finish_reason":"stop","completion_tokens":150}}
```

## Notes

- This is one of several "debug pipe migration" tasks - see also `commands-use-logger.md`, `repl-actions-llm-use-logger.md`, etc.
- The debug_pipe infrastructure will be removed in a later cleanup task once all migrations are complete
- Don't remove the `#include "debug_pipe.h"` yet - other code may still use it

## Agent Configuration
- Model: sonnet
- Thinking: thinking
