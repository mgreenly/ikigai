# Fix: Debug Tool Result Message

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
- `src/repl_tool.c` - `ik_repl_execute_pending_tool` where tool result message is added

### Test files:
- `tests/unit/repl/repl_tool_test.c` - Tool execution tests

## Situation

### Current Behavior

When a tool_result message is added to the conversation, no debug output is generated.

### Goal

Log when tool_result message is added to conversation:
```
[openai] << TOOL_RESULT: {"files":["main.c","util.c"]}
```

## High-Level Goal

**Add debug output when tool_result message is added to conversation.**

### Required Changes

In `src/repl_tool.c` `ik_repl_execute_pending_tool`, after adding tool result to conversation (step 3):

```c
if (repl->openai_debug_pipe != NULL && repl->openai_debug_pipe->write_end != NULL) {
    fprintf(repl->openai_debug_pipe->write_end,
            "<< TOOL_RESULT: %s\n",
            result_json);
    fflush(repl->openai_debug_pipe->write_end);
}
```

### Testing Strategy

Add test verifying tool_result debug output appears after tool execution.

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Tool result message appears in debug output with `<<` prefix
