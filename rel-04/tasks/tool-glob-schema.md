# Task: Build Glob Tool JSON Schema

## Target
User story: 01-simple-greeting-no-tools

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- No existing tool module in src/

## Context
Read before starting:
- docs/memory.md (talloc patterns)
- docs/return_values.md (Result types)
- docs/naming.md (ik_module_function conventions)
- src/openai/client.c (understand request serialization with yyjson)
- rel-04/user-stories/01-simple-greeting-no-tools.md (see expected JSON format)

## Task
Create `src/tool.h` and `src/tool.c` with a function `ik_tool_build_glob_schema(yyjson_mut_doc *doc)` that builds the JSON schema for the glob tool. This is a focused first step - build one tool's schema correctly, test it thoroughly, then add the others.

## TDD Cycle

### Red
1. Create `tests/unit/tool/test_tool.c` (new test directory)
2. Write test that calls `ik_tool_build_glob_schema()` and verifies:
   - Returns non-NULL yyjson_mut_val*
   - Has "type": "function"
   - Has "function" object with "name": "glob"
   - Has "description" field
   - Has "parameters" with correct structure (type, properties, required)
3. Add test file to Makefile/build system
4. Run `make check` - expect compile failure (function doesn't exist)

### Green
1. Create `src/tool.h` with function declaration
2. Create `src/tool.c` with implementation using yyjson to build:
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
3. Add source files to build system
4. Run `make check` - expect pass

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
