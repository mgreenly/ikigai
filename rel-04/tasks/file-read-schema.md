# Task: Define File Read Tool Schema

## Target
User story: 03-single-file-read

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/coverage.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/naming.md
- docs/memory.md
- docs/return_values.md
- docs/architecture.md
- rel-04/user-stories/03-single-file-read.md (user story for file_read schema)

### Pre-read Source (patterns)
- src/openai/client.c (JSON schema building with yyjson)
- src/config.c (yyjson usage patterns)
- src/tool.h (existing schema functions)
- src/tool.c (how glob schema is defined)

### Pre-read Tests (patterns)
- tests/unit/openai/client_sse_test.c (yyjson test patterns)
- tests/unit/config/config_test.c (talloc context setup in tests)

## Pre-conditions
- `make check` passes
- Task `db-tool-persist.md` completed (Story 02 complete)
- `ik_tool_schema_create()` exists in `src/tool.h`

## Task
Add file_read tool schema to the tools array. The schema must match the format expected by OpenAI API.

Expected schema from user story:
```json
{
  "type": "function",
  "function": {
    "name": "file_read",
    "description": "Read contents of a file",
    "parameters": {
      "type": "object",
      "properties": {
        "path": {"type": "string"}
      },
      "required": ["path"]
    }
  }
}
```

## TDD Cycle

### Red
1. Add test in `tests/unit/tool/test_tool.c`:
   - Test `ik_tool_schema_file_read()` returns valid JSON object
   - Test contains "name": "file_read"
   - Test contains "description" field
   - Test parameters require "path" property
2. Run `make check` - expect compile failure (function doesn't exist)

### Green
1. Add `ik_tool_schema_file_read(void *parent)` declaration to `src/tool.h`
2. Implement in `src/tool.c`:
   - Build JSON schema with yyjson matching the expected format
   - Follow same pattern as `ik_tool_schema_glob()`
3. Update tool array building to include file_read schema
4. Run `make check` - expect pass

### Refactor
1. Check for duplication with glob schema creation
2. Consider extracting common schema building logic if warranted
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_schema_file_read()` exists and returns valid schema
- file_read schema included in tools array sent to API
- 100% test coverage for new code
