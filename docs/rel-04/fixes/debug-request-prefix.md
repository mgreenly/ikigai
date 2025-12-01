# Fix: Debug Request Prefix Convention

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/openai/client_multi_request.c` - Current debug output (lines 98-107)
- `src/debug_pipe.h` - Debug pipe infrastructure

### Test files:
- `tests/unit/repl/repl_debug_pipe_integration_test.c` - Existing debug pipe tests

## Situation

### Current Behavior

Debug output for requests uses `[OpenAI Request]` format:
```
[openai] [OpenAI Request]
[openai] URL: https://api.openai.com/v1/chat/completions
[openai] Body: {"model":"gpt-4o",...}
```

### Goal

Establish `>>` prefix convention for outgoing messages (requests) to distinguish from incoming messages (`<<` for responses, added in subsequent fixes).

Target format:
```
[openai] >> REQUEST
[openai] >> URL: https://api.openai.com/v1/chat/completions
[openai] >> Body: {"model":"gpt-4o",...}
```

## High-Level Goal

**Update request debug output to use `>>` prefix convention.**

### Required Changes

In `src/openai/client_multi_request.c`, update the debug output block:

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

Update existing debug pipe tests to expect new `>>` prefix format.

## Success Criteria

- `make check` passes
- `make lint && make coverage` passes with 100% coverage
- Debug output shows `>> REQUEST`, `>> URL:`, `>> Body:` prefixes
