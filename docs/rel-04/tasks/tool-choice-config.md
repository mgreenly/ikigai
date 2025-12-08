# Task: Add tool_choice Configuration Type

## Target
User story: 13-tool-choice-auto, 13-tool-choice-none, 13-tool-choice-required, 13-tool-choice-specific

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/quality.md
- .agents/skills/coverage.md

### Pre-read Docs
- docs/naming.md
- docs/architecture.md
- rel-04/user-stories/13-tool-choice-auto.md
- rel-04/user-stories/13-tool-choice-none.md
- rel-04/user-stories/13-tool-choice-required.md
- rel-04/user-stories/13-tool-choice-specific.md

### Pre-read Source (patterns)
- src/openai/client.h (request structure reference)
- src/openai/client.c (JSON serialization pattern using yyjson)
- src/error.h (enum + struct pattern reference)
- src/config.h (simple struct with helper functions)

### Pre-read Tests (patterns)
- tests/unit/config/basic_test.c (struct creation and field testing pattern)
- tests/unit/error/error_test.c (enum + error handling pattern)

## Pre-conditions
- `make check` passes
- Task `replay-tool-e2e.md` completed (Story 12)
- Request serialization currently hardcodes `tool_choice: "auto"` (Story 01)
- Loop limit logic can set `tool_choice: "none"` (Story 11)

## Task
Create a configuration type that can represent all valid `tool_choice` values:
1. String modes: "auto", "none", "required"
2. Specific tool object: `{"type": "function", "function": {"name": "glob"}}`

This will be used by the request builder to serialize the correct tool_choice value.

## TDD Cycle

### Red
1. Create tests/unit/openai/test_tool_choice.c:
   - Test creating tool_choice for "auto" mode
   - Test creating tool_choice for "none" mode
   - Test creating tool_choice for "required" mode
   - Test creating tool_choice for specific tool (e.g., "glob")
   - Test serializing each to JSON string
2. Create src/openai/tool_choice.h with:
   - Enum for mode: `IK_TOOL_CHOICE_AUTO`, `IK_TOOL_CHOICE_NONE`, `IK_TOOL_CHOICE_REQUIRED`, `IK_TOOL_CHOICE_SPECIFIC`
   - Struct `ik_tool_choice_t` with mode and optional tool name
   - Function declarations
3. Create src/openai/tool_choice.c with stubs:
   - All functions return struct with mode set to `IK_TOOL_CHOICE_AUTO` (incorrect for none/required/specific tests)
4. Run `make check` - expect assertion failure (tests for none/required/specific expect different modes)

### Green
1. Replace stubs in src/openai/tool_choice.c with correct implementations:
   - `ik_tool_choice_auto()` returns struct with `IK_TOOL_CHOICE_AUTO`
   - `ik_tool_choice_none()` returns struct with `IK_TOOL_CHOICE_NONE`
   - `ik_tool_choice_required()` returns struct with `IK_TOOL_CHOICE_REQUIRED`
   - `ik_tool_choice_specific()` returns struct with `IK_TOOL_CHOICE_SPECIFIC` and tool name
2. Run `make check` - expect pass

### Refactor
1. Consider if tool_choice should have a destroy function (depends on ownership of tool_name)
2. Ensure naming matches docs/naming.md conventions
3. Verify struct is small enough to pass by value or needs pointer passing
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_choice_t` type exists with mode enum and tool name
- Helper functions create tool_choice values for all 4 modes
- 100% test coverage for new code
