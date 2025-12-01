# Task: Add tool_choice Parameter to Request Builder

## Target
User story: 13-tool-choice-auto, 13-tool-choice-none, 13-tool-choice-required, 13-tool-choice-specific

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/testability.md
- .agents/skills/quality.md

### Pre-read Docs
- docs/architecture.md
- rel-04/tasks/request-with-tools.md
- rel-04/tasks/tool-choice-none-on-limit.md

### Pre-read Source (patterns)
- src/openai/client.c (ik_openai_serialize_request function with yyjson patterns)
- src/openai/request.c (alternative request builder location)
- src/openai/client.h (ik_openai_request_t structure definition)
- src/openai/tool_choice.h (tool_choice type and serialize function)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (JSON serialization tests with yyjson parsing and verification patterns)

## Pre-conditions
- `make check` passes
- Task `tool-choice-serialize.md` completed
- `ik_tool_choice_serialize()` works for all 4 modes
- Request builder exists from Story 01 (task: request-with-tools.md)
- Request builder currently has conditional logic for tool_choice (Story 11: tool-choice-none-on-limit.md)

## Refactoring Context

**This task refactors Story 11 code**: Task `tool-choice-none-on-limit.md` implemented tool_choice handling using hardcoded string literals (`"auto"`, `"none"`) and conditional logic based on `limit_reached` state. This was intentional to complete Story 11 with minimal dependencies.

**Now we clean it up**: With the proper `ik_tool_choice_t` type available, this task replaces that conditional logic with a cleaner parameter-based approach. The caller decides which tool_choice to use and passes it explicitly.

This follows the TDD pattern: "Make it work, then make it right."

## Task
Modify the request builder to accept an `ik_tool_choice_t` parameter instead of hardcoding "auto" or conditionally using "none".

This unifies all tool_choice handling in one place and makes it configurable for all use cases.

## TDD Cycle

### Red
1. Add/modify tests in tests/unit/openai/test_client.c:
   - Build request with tool_choice auto → verify "tool_choice": "auto"
   - Build request with tool_choice none → verify "tool_choice": "none"
   - Build request with tool_choice required → verify "tool_choice": "required"
   - Build request with tool_choice specific "glob" → verify object format
2. Run `make check` - expect test failure (parameter doesn't exist)

### Green
1. Locate request builder function signature
2. Add parameter: `ik_tool_choice_t tool_choice`
3. Remove hardcoded `"tool_choice": "auto"` line
4. Remove conditional logic for limit_reached (if present)
5. Add call to `ik_tool_choice_serialize(doc, root, "tool_choice", tool_choice)`
6. Update all call sites to pass tool_choice parameter:
   - Normal requests: pass `ik_tool_choice_auto()`
   - Limit reached: pass `ik_tool_choice_none()`
7. Run `make check` - expect pass

### Refactor
1. Verify all call sites are updated (grep for function name)
2. Consider if default value helper is needed (most requests use auto)
3. Ensure parameter ordering makes sense
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- Request builder accepts tool_choice parameter
- No hardcoded tool_choice values remain
- All existing stories still work (01-12)
- 100% test coverage maintained
