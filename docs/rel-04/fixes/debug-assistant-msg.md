# Fix: Debug Assistant Message

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/repl.h` - REPL context with `openai_debug_pipe` and `assistant_response`
- `src/repl.c` - `handle_request_success` where assistant message is added

### Test files:
- `tests/unit/repl/repl_debug_pipe_integration_test.c` - Existing debug pipe tests

## Situation

### Current Behavior

When an assistant message is added to the conversation, no debug output is generated.

### Goal

Log when assistant message is added to conversation (truncated to 80 chars):
```
[openai] << ASSISTANT: Here are the C files in your project...
```

Or for longer responses:
```
[openai] << ASSISTANT: This is a very long response that will be truncated after seventy-seven ch...
```

## High-Level Goal

**Add debug output when assistant message is added to conversation.**

### Required Changes

In `src/repl.c` `handle_request_success`, **inside** the existing `if (repl->assistant_response != NULL && strlen(...) > 0)` block, after adding assistant message to conversation:

```c
// Inside the existing non-NULL/non-empty check, after ik_openai_conversation_add_msg:
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    size_t len = strlen(repl->assistant_response);
    if (len > 80) {
        fprintf(repl->openai_debug_pipe->write_end,
                "<< ASSISTANT: %.77s...\n", repl->assistant_response);
    } else {
        fprintf(repl->openai_debug_pipe->write_end,
                "<< ASSISTANT: %s\n", repl->assistant_response);
    }
    fflush(repl->openai_debug_pipe->write_end);
}
```

Note: This goes inside the existing guard, so NULL/empty checks are already handled. Pure tool-call responses (no text) won't produce an `<< ASSISTANT:` line - that's correct since no assistant message is added to the conversation in that case.

### Testing Strategy

1. Add test for short assistant message debug output
2. Add test for long assistant message truncation

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Assistant message appears in debug output with `<<` prefix
- Long messages are truncated with `...`
