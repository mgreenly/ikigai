# Fix: Tool Loop Wiring - Tool Calls Not Passed to REPL

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `coverage.md` - 100% coverage requirement
- `style.md` - Code style conventions
- `naming.md` - Naming conventions

## Files to Explore

### Source files (in order of importance):
- `src/openai/client_multi.h` - `ik_http_completion_t` struct definition (MISSING tool_call field)
- `src/openai/client_multi.c` - `ik_openai_multi_info_read` function (MISSING tool_call transfer)
- `src/openai/client_multi_callbacks.h` - `http_write_ctx_t` struct (HAS tool_call field)
- `src/repl_callbacks.c` - `ik_repl_http_completion_callback` (MISSING tool_call handling)
- `src/repl.c` - `handle_request_success` (MISSING tool execution)
- `src/repl.h` - REPL context structure (MAY NEED tool_call field)
- `src/tool.h` - `ik_tool_call_t` struct and `ik_tool_dispatch` declaration
- `src/tool_dispatcher.c` - Tool dispatcher implementation (EXISTS but NEVER CALLED)
- `src/openai/client.c` - `ik_openai_msg_create_tool_call` function
- `src/openai/client_msg.c` - Message creation helpers

### User story reference:
- `docs/rel-04/user-stories/02-single-glob-call.md` - Expected flow with Request A, Response A, Tool Result A, Request B, Response B

### Test files (may need updates):
- `tests/unit/openai/client_multi_callbacks_coverage_test.c`
- `tests/integration/tool_loop_limit_test.c`
- `tests/integration/tool_choice_*.c`

## Situation

### Symptom

When user asks the model to use a tool (e.g., "use the glob tool to show me all src/*.c files"), the model responds with a tool_call, but:
1. The tool is never executed
2. No tool result is added to conversation
3. The follow-up request is sent with incomplete conversation state
4. The model cannot see the tool result and fails to answer

Debug output shows `[openai] Body: {...}` for the request, then just `[openai]` with no response content.

### Architecture Overview

The tool loop flow SHOULD work as:
```
User message → API Request → Model responds with tool_call →
Execute tool → Add tool_call + result to conversation →
Follow-up request → Model responds with final answer
```

The data flows through these layers:
1. **SSE Stream** → `http_write_callback` parses chunks
2. **`http_write_ctx_t`** → accumulates `tool_call` (in `client_multi_callbacks.h:26`)
3. **`ik_http_completion_t`** → SHOULD contain tool_call for completion callback
4. **REPL callback** → SHOULD store tool_call and trigger execution
5. **`handle_request_success`** → SHOULD execute tool and update conversation

### Current Bug (3 Missing Pieces)

#### 1. `ik_http_completion_t` is missing `tool_call` field

In `src/openai/client_multi.h:34-42`:
```c
typedef struct {
    ik_http_status_type_t type;
    int32_t http_code;
    int32_t curl_code;
    char *error_message;
    char *model;
    char *finish_reason;
    int32_t completion_tokens;
    // ❌ MISSING: ik_tool_call_t *tool_call;
} ik_http_completion_t;
```

#### 2. Tool call never transferred to completion struct

In `src/openai/client_multi.c:139-146`, metadata is transferred from `write_ctx` to `completion`:
```c
/* Transfer metadata from write context */
if (completed->write_ctx->model != NULL) {
    completion.model = talloc_steal(multi, completed->write_ctx->model);
}
if (completed->write_ctx->finish_reason != NULL) {
    completion.finish_reason = talloc_steal(multi, completed->write_ctx->finish_reason);
}
completion.completion_tokens = completed->write_ctx->completion_tokens;
// ❌ MISSING: transfer of completed->write_ctx->tool_call
```

#### 3. Tool never executed in REPL

In `src/repl.c:145-207` (`handle_request_success`):
- The function checks `finish_reason == "tool_calls"` to continue the loop
- But it NEVER:
  - Receives the tool_call data
  - Calls `ik_tool_dispatch()` to execute the tool
  - Adds the tool_call message to conversation
  - Adds the tool result message to conversation

The tool loop just sends another request with incomplete conversation.

### Why `ik_tool_dispatch` Exists But Is Never Called

`src/tool_dispatcher.c` has a complete implementation of `ik_tool_dispatch()` that:
- Parses tool arguments JSON
- Routes to `ik_tool_exec_glob`, `ik_tool_exec_file_read`, etc.
- Returns tool result JSON

But grep shows it's NEVER called from the REPL code:
```bash
grep -r "ik_tool_dispatch" src/  # Only shows definition, no calls
```

## High-Level Goal

**Wire up the complete tool loop so tool calls are executed and results flow back to the model.**

### Dependencies (May Need Implementation First)

Before wiring up the tool loop, verify these components exist:

1. **`ik_openai_msg_create_tool_result`** - Function to create canonical tool_result messages
   - Check `src/openai/client.h` and `src/openai/client_msg.c`
   - If missing, implement per `docs/rel-04/tasks/tool-result-msg.md`
   - Signature: `ik_openai_msg_t *ik_openai_msg_create_tool_result(void *parent, const char *tool_call_id, const char *content)`

2. **Serializer handling for `tool_result`** - Transform canonical to OpenAI wire format
   - In `src/openai/client.c` `ik_openai_serialize_request`, check if `role == "tool_result"` is handled
   - OpenAI expects: `{"role": "tool", "tool_call_id": "...", "content": "..."}`
   - Currently only `tool_call` is transformed; other roles pass through unchanged
   - If missing, add transformation similar to `tool_call` handling

### Required Changes

#### Step 1: Add `tool_call` to `ik_http_completion_t`

In `src/openai/client_multi.h`, add include and field:
```c
// Add at top with other includes:
#include "tool.h"  // for ik_tool_call_t

// Update struct:
typedef struct {
    ik_http_status_type_t type;
    int32_t http_code;
    int32_t curl_code;
    char *error_message;
    char *model;
    char *finish_reason;
    int32_t completion_tokens;
    ik_tool_call_t *tool_call;  // ADD THIS
} ik_http_completion_t;
```

#### Step 2: Transfer `tool_call` in `client_multi.c`

In `ik_openai_multi_info_read`, around line 146, add:
```c
/* Transfer tool_call */
if (completed->write_ctx->tool_call != NULL) {
    completion.tool_call = talloc_steal(multi, completed->write_ctx->tool_call);
} else {
    completion.tool_call = NULL;
}
```

Also free it at end (around lines 200-208) with other metadata cleanup.

#### Step 3: Store tool_call in REPL context

In `src/repl.h`, add to `ik_repl_ctx_t`:
```c
ik_tool_call_t *pending_tool_call;  // Tool call awaiting execution
```

In `src/repl_callbacks.c` `ik_repl_http_completion_callback`:
```c
// Store tool_call if present
if (completion->tool_call != NULL) {
    repl->pending_tool_call = talloc_steal(repl, completion->tool_call);
} else {
    repl->pending_tool_call = NULL;
}
```

#### Step 4: Execute tool in `handle_request_success`

In `src/repl.c` `handle_request_success`, before the tool loop check (around line 147):

```c
// Execute pending tool call if present
if (repl->pending_tool_call != NULL) {
    ik_tool_call_t *tc = repl->pending_tool_call;

    // 1. Add tool_call message to conversation (canonical format)
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    ik_openai_msg_t *tc_msg = ik_openai_msg_create_tool_call(
        repl->conversation, tc->id, "function", tc->name, tc->arguments, summary);
    ik_openai_conversation_add_msg(repl->conversation, tc_msg);
    talloc_free(summary);

    // 2. Execute tool
    res_t tool_result = ik_tool_dispatch(repl, tc->name, tc->arguments);
    char *result_json = tool_result.ok;  // tool_dispatch always returns OK with JSON

    // 3. Add tool result message to conversation
    // NOTE: ik_openai_msg_create_tool_result may not exist yet - check dependencies
    // If it doesn't exist, create it or use a simpler approach:
    //   - Create message with role="tool" (OpenAI format directly)
    //   - OR implement canonical tool_result message creation first
    ik_openai_msg_t *result_msg = ik_openai_msg_create_tool_result(
        repl->conversation, tc->id, result_json);
    ik_openai_conversation_add_msg(repl->conversation, result_msg);

    // 4. Display tool call and result in scrollback
    ik_scrollback_append_line(repl->scrollback, summary, strlen(summary));
    // Consider using event_render for consistent formatting

    // 5. Persist to database (if configured)
    // Similar to user/assistant message persistence in handle_newline_action_

    // 6. Clear pending tool call
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = NULL;
}
```

**Important**: This code must run BEFORE the existing `ik_repl_should_continue_tool_loop()` check so the tool result is in the conversation before the follow-up request is sent.

#### Step 5: Add includes

In `src/repl.c`, add if not present:
```c
#include "tool.h"  // for ik_tool_dispatch, ik_tool_call_t
```

### Memory Management Notes

- `tool_call` is initially owned by `write_ctx` (parented to parser)
- Transfer ownership with `talloc_steal` to `multi` for completion callback
- Transfer ownership with `talloc_steal` to `repl` for storage
- Free with `talloc_free` after execution

### Testing Strategy

1. Existing integration tests in `tests/integration/tool_loop_limit_test.c` should start passing
2. May need to mock `ik_tool_dispatch` for unit tests
3. E2E test: Send message requiring glob tool, verify:
   - Tool call parsed correctly
   - Tool executed
   - Result added to conversation
   - Follow-up request contains tool_call + tool result messages
   - Final response answers the question

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- User story 02-single-glob-call works end-to-end:
  - "Find all C files in src/" triggers glob tool
  - Tool executes and returns file list
  - Model summarizes results to user
- Debug output shows both Request A and Request B (with tool messages)
