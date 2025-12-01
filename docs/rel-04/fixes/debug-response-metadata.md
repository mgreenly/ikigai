# Fix: Debug Response Metadata

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `coverage.md` - 100% coverage requirement
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/repl.h` - REPL context with `debug_enabled` and `openai_debug_pipe`
- `src/repl_callbacks.c` - `ik_repl_http_completion_callback` receives completion metadata
- `src/debug_pipe.h` - Debug pipe infrastructure

### Test files:
- `tests/unit/repl/repl_debug_pipe_integration_test.c` - Existing debug pipe tests

## Situation

### Current Behavior

No debug output when LLM response is received. Users only see outgoing requests.

### Goal

Log response metadata when completion callback fires:
- Response type (success/error)
- Model name
- Finish reason ("stop" or "tool_calls")
- Completion tokens
- Tool call summary if present

Target format:
```
[openai] << RESPONSE: type=success, model=gpt-4o, finish=tool_calls, tokens=42, tool_call=glob({"pattern":"*.c"})
```

## High-Level Goal

**Add debug output for response metadata in completion callback.**

### Required Changes

In `src/repl_callbacks.c` `ik_repl_http_completion_callback`, add at start after getting repl pointer:

```c
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

### Testing Strategy

1. Add test for response debug output on success
2. Add test for response debug output on error
3. Add test for response with tool_call info
4. Verify output only appears when debug_enabled

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Response metadata appears in debug output with `<<` prefix
