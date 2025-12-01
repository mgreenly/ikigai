# Task: Add Tools to Request Serialization

## Target
User story: 01-simple-greeting-no-tools

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/naming.md
- docs/architecture.md
- rel-04/user-stories/01-simple-greeting-no-tools.md (user story - see Request A for expected format)

### Pre-read Source (patterns)
- src/openai/client.c (serialize_request implementation with yyjson usage)
- src/openai/client.h (request structure)
- src/error.h (error handling patterns)
- src/tool.h (tools array function)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (JSON serialization test patterns using yyjson parsing)

## Pre-conditions
- `make check` passes
- `ik_tool_build_all()` exists and works in `src/tool.c`
- Task `tool-build-array.md` completed successfully

## Task
Modify `ik_openai_serialize_request()` to include:
1. `"tools"` array (from `ik_tool_build_all()`)
2. `"tool_choice": "auto"` field

This completes story-01 - requests will now include tools, allowing the model to use them (even if it chooses not to for a simple greeting).

## TDD Cycle

### Red
1. Add/modify test in tests/unit/openai/client_structures_test.c for serialization:
   - Serialize a request
   - Parse the resulting JSON
   - Verify `tools` key exists and is an array
   - Verify `tools` array has 5 elements
   - Verify `tool_choice` key exists with value "auto"
2. Run `make check` - expect test failure (fields not present)

### Green
1. In `ik_openai_serialize_request()`:
   - Add `#include "tool.h"` at top
   - After adding other fields, call `ik_tool_build_all(doc)`
   - Add tools array to root object with key "tools"
   - Add `tool_choice` string field with value "auto"
2. Update Makefile to link tool.o with openai client if needed
3. Run `make check` - expect pass

### Refactor
1. Verify field order matches user story (model, messages, tools, tool_choice, stream)
2. Consider if tool_choice should be configurable (NO for story-01 - hardcode "auto")
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_openai_serialize_request()` includes tools array and tool_choice
- Serialized JSON matches format in user story 01 Request A
- 100% test coverage maintained
- Story 01 is complete: requests include tools, model can respond without using them
