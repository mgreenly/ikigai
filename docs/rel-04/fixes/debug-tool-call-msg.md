# Fix: Debug Tool Call Message

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/repl.h` - REPL context with `openai_debug_pipe`
- `src/repl_tool.c` - `ik_repl_execute_pending_tool` where tool_call message is added

### Test files:
- `tests/unit/repl/repl_tool_test.c` - Tool execution tests

## Situation

### Current Behavior

When a tool_call message is added to the conversation, no debug output is generated.

### Goal

Log when tool_call message is added to conversation:
```
[openai] << TOOL_CALL: glob({"pattern":"*.c"})
```

## High-Level Goal

**Add debug output when tool_call message is added to conversation.**

### Required Changes

In `src/repl_tool.c` `ik_repl_execute_pending_tool`, after adding tool_call to conversation (step 1):

```c
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->openai_debug_pipe->write_end,
            "<< TOOL_CALL: %s\n",
            summary);
    fflush(repl->openai_debug_pipe->write_end);
}
```

### Testing Strategy

Add test verifying tool_call debug output appears when tool is dispatched.

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Tool call message appears in debug output with `<<` prefix
