# Task: Serialize tool_choice to JSON

## Target
User story: 13-tool-choice-auto, 13-tool-choice-none, 13-tool-choice-required, 13-tool-choice-specific

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- Task `tool-choice-config.md` completed
- `ik_tool_choice_t` type exists with all 4 modes

## Context
Read before starting:
- src/openai/tool_choice.h (tool_choice configuration type)
- src/openai/client.c (existing JSON serialization using yyjson)
- rel-04/user-stories/13-tool-choice-auto.md (expect `"tool_choice": "auto"`)
- rel-04/user-stories/13-tool-choice-none.md (expect `"tool_choice": "none"`)
- rel-04/user-stories/13-tool-choice-required.md (expect `"tool_choice": "required"`)
- rel-04/user-stories/13-tool-choice-specific.md (expect `"tool_choice": {"type": "function", "function": {"name": "glob"}}`)

## Task
Implement JSON serialization for `ik_tool_choice_t` that outputs:
- String value for auto/none/required modes
- Object value for specific tool mode

## TDD Cycle

### Red
1. Add tests to tests/unit/test_tool_choice_config.c:
   - Serialize auto mode → verify JSON is string "auto"
   - Serialize none mode → verify JSON is string "none"
   - Serialize required mode → verify JSON is string "required"
   - Serialize specific tool "glob" → verify JSON is object with type="function" and function.name="glob"
   - Parse each result with yyjson to verify structure
2. Run `make check` - expect test failure (function doesn't exist)

### Green
1. Add function to src/openai/tool_choice.c:
   - `void ik_tool_choice_serialize(yyjson_mut_doc* doc, yyjson_mut_val* obj, const char* key, ik_tool_choice_t choice)`
   - For AUTO/NONE/REQUIRED: add string value to obj
   - For SPECIFIC: create object with type and function.name, add to obj
2. Add function declaration to src/openai/tool_choice.h
3. Run `make check` - expect pass

### Refactor
1. Consider extracting string literals ("auto", "none", "required", "function") to constants
2. Verify error handling for invalid mode values
3. Check if yyjson calls need null checks
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_choice_serialize()` correctly outputs all 4 formats
- Serialized JSON matches user story examples exactly
- 100% test coverage for new code
