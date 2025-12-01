# Task: Build Glob Tool JSON Schema

## Target
User story: 01-simple-greeting-no-tools

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/coverage.md
- .agents/skills/quality.md
- .agents/skills/style.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/naming.md
- rel-04/user-stories/01-simple-greeting-no-tools.md (user story - see expected JSON format)

### Pre-read Source (patterns)
- src/openai/client.c (yyjson JSON building patterns)
- src/config.c (yyjson initialization and object/array creation)

### Pre-read Tests (patterns)
- tests/unit/openai/client_structures_test.c (talloc/yyjson test setup patterns)
- tests/unit/layer/basic_test.c (unit test structure with setup/teardown)

## Pre-conditions
- `make check` passes
- No existing tool module in src/

## Task
Create `src/tool.h` and `src/tool.c` with a function `ik_tool_build_glob_schema(yyjson_mut_doc *doc)` that builds the JSON schema for the glob tool. This is a focused first step - build one tool's schema correctly, test it thoroughly, then add the others.

## TDD Cycle

### Red
1. Create `tests/unit/tool/` directory if it doesn't exist
2. Create `tests/unit/tool/test_tool.c`
3. Write test that calls `ik_tool_build_glob_schema()` and verifies:
   - Returns non-NULL yyjson_mut_val*
   - Has "type": "function"
   - Has "function" object with "name": "glob"
   - Has "description" field
   - Has "parameters" with correct structure (type, properties, required)
4. Add test file to Makefile/build system
5. Create `src/tool.h` with function declaration
6. Create `src/tool.c` with stub: `return NULL;`
7. Add source files to build system
8. Run `make check` - expect assertion failure (returns NULL, test expects valid schema)

### Green
1. Replace stub in `src/tool.c` with implementation using yyjson to build:
   ```json
   {
     "type": "function",
     "function": {
       "name": "glob",
       "description": "Find files matching a glob pattern",
       "parameters": {
         "type": "object",
         "properties": {
           "pattern": {"type": "string", "description": "Glob pattern (e.g., 'src/**/*.c')"},
           "path": {"type": "string", "description": "Base directory (default: cwd)"}
         },
         "required": ["pattern"]
       }
     }
   }
   ```
2. Run `make check` - expect pass

### Refactor
1. Ensure function follows ik_ prefix convention
2. Verify memory ownership (yyjson_mut_val owned by doc)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `src/tool.h` and `src/tool.c` exist
- `ik_tool_build_glob_schema()` function works correctly
- 100% test coverage for new code
