# Task: Display Tool Calls and Results in Scrollback

## Target
User story: 02-single-glob-call

## Agent
model: haiku

### Pre-read Docs
- rel-04/user-stories/02-single-glob-call.md (walkthrough steps 5 and 8)

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/mocking.md
- .agents/skills/style.md

### Pre-read Source (patterns)
- src/scrollback.c (scrollback buffer append patterns)
- src/scrollback.h (scrollback API and line structure)
- src/format.c (formatting buffer implementation patterns)
- src/format.h (format buffer API)
- src/repl.c (where tool display integration occurs)
- src/repl_actions.c (display logic)

### Pre-read Tests (patterns)
- tests/unit/format/format_basic_test.c (buffer creation, append, formatting patterns)
- tests/unit/scrollback/scrollback_append_test.c (line append and layout patterns)
- tests/unit/commands/mark_scrollback_format_test.c (scrollback formatting with multi-line output patterns)

## Pre-conditions
- `make check` passes
- Conversation tool loop integration works
- Task `tool-loop-continuation.md` completed

## Task
Display tool calls and results in the scrollback buffer. The user story walkthrough says:
- Step 5: "Tool call displayed in scrollback"
- Step 8: "Tool result displayed in scrollback"

Design a clear, readable format for tool activity. Example:

```
[tool] glob(pattern="*.c", path="src/")
[result] 3 files found:
  src/main.c
  src/config.c
  src/repl.c
```

## TDD Cycle

### Red
1. Add tests for tool display formatting:
   - Format tool call produces expected string
   - Format tool result produces expected string
   - Handles empty results gracefully
   - Handles long file lists (truncation or scrolling)
2. Add formatting function declarations to appropriate header
3. Add stubs that return empty strings or NULL
4. Run `make check` - expect assertion failure (tests expect formatted output)

### Green
1. Replace stubs with implementations:
   - `ik_format_tool_call(void *parent, const ik_tool_call_t *call)` - returns display string
   - `ik_format_tool_result(void *parent, const char *tool_name, const char *result_json)` - returns display string
2. Integrate into scrollback:
   - Add tool call to scrollback after parsing
   - Add tool result to scrollback after execution
3. Run `make check` - expect pass

### Refactor
1. Consider color/styling for tool output (if terminal supports)
2. Ensure format is consistent with existing scrollback style
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Tool calls and results display clearly in scrollback
- 100% test coverage for new code
