# Task: Add Remaining Tool Schemas

## Target
User story: 01-simple-greeting-no-tools

## Agent
model: sonnet

## Pre-conditions
- `make check` passes
- `ik_tool_build_glob_schema()` exists and works in `src/tool.c`
- Task `tool-glob-schema.md` completed successfully

## Context
Read before starting:
- src/tool.h (existing glob function)
- src/tool.c (existing implementation pattern)
- tests/unit/tool/test_tool.c (existing test pattern)
- rel-04/user-stories/01-simple-greeting-no-tools.md (see all 5 tool schemas in Request A)

## Task
Add 4 more schema builder functions following the same pattern as glob:
- `ik_tool_build_file_read_schema()`
- `ik_tool_build_grep_schema()`
- `ik_tool_build_file_write_schema()`
- `ik_tool_build_bash_schema()`

## TDD Cycle

### Red
1. Add tests for `ik_tool_build_file_read_schema()`:
   - Has "name": "file_read"
   - Has "description" about reading files
   - Has "path" parameter (required)
2. Add tests for `ik_tool_build_grep_schema()`:
   - Has "name": "grep"
   - Has "pattern" (required), "path", "glob" parameters
3. Add tests for `ik_tool_build_file_write_schema()`:
   - Has "name": "file_write"
   - Has "path" and "content" parameters (both required)
4. Add tests for `ik_tool_build_bash_schema()`:
   - Has "name": "bash"
   - Has "command" parameter (required)
5. Run `make check` - expect compile failures

### Green
1. Implement `ik_tool_build_file_read_schema()` in src/tool.c
2. Implement `ik_tool_build_grep_schema()` in src/tool.c
3. Implement `ik_tool_build_file_write_schema()` in src/tool.c
4. Implement `ik_tool_build_bash_schema()` in src/tool.c
5. Add declarations to src/tool.h
6. Run `make check` - expect pass

### Refactor
1. Extract common patterns if duplication is excessive
2. Consider helper for building parameter objects
3. Ensure consistent description style across all tools
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- All 5 tool schema functions exist and work
- 100% test coverage for all tool functions
