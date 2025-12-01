# Fix: Debug Output Missing for Tool Loop Messages

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `coverage.md` - 100% coverage requirement
- `style.md` - Code style conventions
- `naming.md` - Naming conventions

## Files to Explore

### Source files:
- `src/repl.h` - REPL context with `debug_enabled` and `openai_debug_pipe`
- `src/repl.c` - `handle_request_success` where assistant messages are added
- `src/repl_callbacks.c` - `ik_repl_http_completion_callback` receives completion metadata
- `src/repl_tool.c` - `ik_repl_execute_pending_tool` where tool messages are added
- `src/openai/client_multi_request.c` - Current debug output (request only, lines 98-107)
- `src/debug_pipe.h` - Debug pipe infrastructure

### Test files:
- `tests/unit/repl/repl_debug_pipe_integration_test.c` - Existing debug pipe tests

## Situation

### Current Behavior

When `/debug on` is enabled, debug output only shows **outbound API requests**:

```
[openai] [OpenAI Request]
[openai] URL: https://api.openai.com/v1/chat/completions
[openai] Content-Type: application/json
[openai] Body: {"model":"gpt-4o",...}
```

### What's Missing

During a tool loop, the user sees nothing for:

1. **Received assistant response** - No debug output when model returns (whether tool_calls or text)
2. **Tool call message** - The `tool_call` message added to conversation is invisible
3. **Tool result message** - The `tool` role message with result JSON is invisible
4. **Loop continuation request** - Only the body is shown, not that it's a continuation

### Where Debug Output Should Be Added

#### 1. Completion callback (`repl_callbacks.c`)

When `ik_repl_http_completion_callback` receives a response, log:
- Response type (success/error)
- Model name
- Finish reason ("stop" or "tool_calls")
- Completion tokens
- Tool call summary (if present): id, name, arguments

#### 2. Tool execution (`repl_tool.c`)

When `ik_repl_execute_pending_tool` runs, log:
- Tool call message being added: `[openai] << TOOL_CALL: name(arguments)`
- Tool result message being added: `[openai] << TOOL_RESULT: {result JSON}`

#### 3. Assistant message (`repl.c`)

When `handle_request_success` adds assistant response to conversation, log:
- `[openai] << ASSISTANT: {content preview or "(empty)"}`

### Implementation Approach

Use the existing `openai_debug_pipe` for all new output. Check `repl->debug_enabled` before writing (or write unconditionally since the debug manager drains pipes regardless - output only appears in scrollback when `debug_enabled` is true).

Pattern to use (from existing code):
```c
if (repl->debug_enabled && repl->openai_debug_pipe != NULL) {
    fprintf(repl->openai_debug_pipe->write_end,
            "[OpenAI Response]\n"
            "Model: %s\n"
            "Finish reason: %s\n"
            "Tokens: %d\n\n",
            completion->model,
            completion->finish_reason,
            completion->completion_tokens);
    fflush(repl->openai_debug_pipe->write_end);
}
```

### Debug Output Format

Use prefix convention:
- `>>` for outgoing (requests)
- `<<` for incoming (responses)

Example session with `/debug on`:
```
[openai] >> REQUEST: POST /v1/chat/completions
[openai] >> Body: {"model":"gpt-4o",...}
[openai] << RESPONSE: model=gpt-4o, finish=tool_calls, tokens=42
[openai] << TOOL_CALL: glob({"pattern":"*.c"})
[openai] << TOOL_RESULT: {"files":["main.c","util.c"]}
[openai] >> REQUEST: POST /v1/chat/completions (continuation)
[openai] >> Body: {"model":"gpt-4o",...}
[openai] << RESPONSE: model=gpt-4o, finish=stop, tokens=87
[openai] << ASSISTANT: "Here are the C files..."
```

## High-Level Goal

**Add debug output for all message types during tool loops so users can trace the full conversation flow.**

### Required Changes

#### Step 1: Log response in completion callback

In `src/repl_callbacks.c` `ik_repl_http_completion_callback`:

```c
// At start of function, after getting repl pointer:
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->openai_debug_pipe->write_end,
            "<< RESPONSE: type=%s",
            completion->type == IK_HTTP_SUCCESS ? "success" : "error");
    if (completion->type == IK_HTTP_SUCCESS) {
        fprintf(repl->openai_debug_pipe->write_end,
                ", model=%s, finish=%s, tokens=%d",
                completion->model ? completion->model : "(null)",
                completion->finish_reason ? completion->finish_reason : "(null)",
                completion->completion_tokens);
    }
    if (completion->tool_call != NULL) {
        fprintf(repl->openai_debug_pipe->write_end,
                ", tool_call=%s(%s)",
                completion->tool_call->name,
                completion->tool_call->arguments);
    }
    fprintf(repl->openai_debug_pipe->write_end, "\n");
    fflush(repl->openai_debug_pipe->write_end);
}
```

#### Step 2: Log tool messages in tool execution

In `src/repl_tool.c` `ik_repl_execute_pending_tool`:

```c
// After step 1 (add tool_call to conversation):
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->openai_debug_pipe->write_end,
            "<< TOOL_CALL: %s\n",
            summary);
    fflush(repl->openai_debug_pipe->write_end);
}

// After step 3 (add tool result to conversation):
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->openai_debug_pipe->write_end,
            "<< TOOL_RESULT: %s\n",
            result_json);
    fflush(repl->openai_debug_pipe->write_end);
}
```

#### Step 3: Log assistant message in handle_request_success

In `src/repl.c` `handle_request_success`, after adding assistant message to conversation:

```c
// After ik_openai_conversation_add_msg for assistant:
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    const char *preview = repl->assistant_response;
    size_t len = strlen(preview);
    if (len > 80) {
        fprintf(repl->openai_debug_pipe->write_end,
                "<< ASSISTANT: %.77s...\n", preview);
    } else {
        fprintf(repl->openai_debug_pipe->write_end,
                "<< ASSISTANT: %s\n", preview);
    }
    fflush(repl->openai_debug_pipe->write_end);
}
```

#### Step 4: Update request logging prefix

In `src/openai/client_multi_request.c`, change existing output to use `>>` prefix:

```c
if (debug_output != NULL) {
    fprintf(debug_output, ">> REQUEST\n");
    fprintf(debug_output, ">> URL: https://api.openai.com/v1/chat/completions\n");
    fprintf(debug_output, ">> Body: %s\n", json_body);
    fprintf(debug_output, "\n");
    fflush(debug_output);
}
```

### Testing Strategy

1. Add tests in `tests/unit/repl/repl_debug_pipe_integration_test.c` or new file
2. Test scenarios:
   - Response debug output appears on success
   - Response debug output appears on error
   - Tool call debug appears during tool loop
   - Tool result debug appears after execution
   - Assistant message debug appears
3. Verify debug output only goes to scrollback when `debug_enabled == true`

### Memory Management

No new allocations needed. All output uses `fprintf` to existing debug pipe FILE*.

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- With `/debug on`, user sees:
  - Outgoing request bodies (existing)
  - Incoming response metadata (NEW)
  - Tool call messages added to conversation (NEW)
  - Tool result messages added to conversation (NEW)
  - Assistant messages added to conversation (NEW)
