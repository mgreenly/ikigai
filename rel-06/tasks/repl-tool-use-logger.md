# Task: Replace Debug Pipe with Logger in repl_tool.c

## Overview

Replace `fprintf(repl->shared->openai_debug_pipe->write_end, ...)` calls in `src/repl_tool.c` with JSONL logger calls. The debug pipe was a workaround for capturing output in alternate buffer mode - the JSONL logger is the proper solution.

## Why This Matters

1. **Consistency**: All diagnostic output should use the structured JSONL logger
2. **Observability**: Debug pipe output is transient; logger output is persisted
3. **Simplification**: Removing debug pipes simplifies the architecture

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Current State

`src/repl_tool.c` has FOUR debug pipe fprintf locations in two functions:

### Function: `ik_repl_execute_pending_tool()` (lines 62-122)

**Location 1 - Lines 78-83** - Tool call debug output:
```c
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< TOOL_CALL: %s\n",
            summary);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

**Location 2 - Lines 97-102** - Tool result debug output:
```c
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< TOOL_RESULT: %s\n",
            result_json);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

### Function: `ik_repl_complete_tool_execution()` (lines 195-276)

**Location 3 - Lines 221-226** - Tool call debug output (async completion):
```c
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< TOOL_CALL: %s\n",
            summary);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

**Location 4 - Lines 235-240** - Tool result debug output (async completion):
```c
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< TOOL_RESULT: %s\n",
            result_json);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

**Note**: `#include "logger.h"` is NOT present and must be added (after existing includes around line 16).

## Requirements

1. Add `#include "logger.h"` to the includes
2. Replace all 4 debug pipe fprintf blocks with `ik_log_debug_json()` calls
3. Include structured data: event type, function context, tool summary/result
4. Completely remove the debug pipe null checks, fprintf blocks, and fflush calls

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
- READ: src/repl_tool.c (find all debug pipe fprintf calls)
- READ: src/logger.h (JSONL logger API)
- READ: src/logger.c (understand ik_log_debug_json usage)
- READ: src/debug_pipe.h (understand what's being replaced)

## Implementation

### Step 1: Add include

Add this include after the existing includes (around line 16, after `#include "wrapper.h"`):
```c
#include "logger.h"
```

### Step 2: Replace debug pipe blocks

**Replacement pattern for TOOL_CALL** (locations 1 and 3):

Replace the entire debug pipe block:
```c
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< TOOL_CALL: %s\n",
            summary);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

With:
```c
{
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_call");
    yyjson_mut_obj_add_str(log_doc, log_root, "summary", summary);
    ik_log_debug_json(log_doc);
}
```

**Replacement pattern for TOOL_RESULT** (locations 2 and 4):

Replace the entire debug pipe block:
```c
if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->shared->openai_debug_pipe->write_end,
            "<< TOOL_RESULT: %s\n",
            result_json);
    fflush(repl->shared->openai_debug_pipe->write_end);
}
```

With:
```c
{
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
    yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_result");
    yyjson_mut_obj_add_str(log_doc, log_root, "result", result_json);
    ik_log_debug_json(log_doc);
}
```

## Acceptance Criteria
- [ ] `#include "logger.h"` added to includes
- [ ] All 4 debug pipe fprintf blocks replaced with ik_log_debug_json()
- [ ] No debug pipe references remain in repl_tool.c
- [ ] No fflush calls for debug pipe remain
- [ ] Log entries include structured data: event, summary/result
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Dependencies

This task is part of the "Debug Pipe Migration" group. It depends on:
- **logger-lifecycle.md** (must be done first) - Sets up global logger initialization

This task can be done in parallel with other debug pipe migration tasks:
- commands-mark-use-logger.md
- commands-use-logger.md
- commands-agent-use-logger.md
- repl-actions-llm-use-logger.md
- repl-event-handlers-use-logger.md
- repl-callbacks-use-logger.md

## Notes

- Uses the **legacy global logger API** (`ik_log_debug_json`) rather than the DI-based API (`ik_logger_debug_json`)
- This is intentional for this migration phase - keeps changes minimal and consistent with other migration tasks
- The debug pipe null checks are completely removed since the global logger handles NULL gracefully
- There are FOUR debug pipe usages in this file across two functions
- Both `ik_repl_execute_pending_tool()` and `ik_repl_complete_tool_execution()` have identical debug output patterns

## Agent Configuration
- Model: sonnet
- Thinking: thinking
