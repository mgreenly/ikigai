# Task: Build Complete Tools Array

## Target
User story: 01-simple-greeting-no-tools

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/memory.md
- docs/return_values.md
- docs/naming.md
- docs/architecture.md
- rel-04/user-stories/01-simple-greeting-no-tools.md (see complete tools array in Request A)

### Pre-read Source (patterns)
- src/config.c (yyjson_mut_doc/arr/obj patterns for building JSON structures)
- src/tool.h (existing functions)
- src/tool.c (existing implementation)

### Pre-read Tests (patterns)
- tests/unit/config/basic_test.c (testing patterns with yyjson and talloc)
- tests/unit/tool/test_tool.c (existing test patterns)

## Pre-conditions
- `make check` passes
- All 5 tool schema functions exist in `src/tool.c`:
  - `ik_tool_build_glob_schema()`
  - `ik_tool_build_file_read_schema()`
  - `ik_tool_build_grep_schema()`
  - `ik_tool_build_file_write_schema()`
  - `ik_tool_build_bash_schema()`
- Task `tool-all-schemas.md` completed successfully

## Task
Create `ik_tool_build_all(yyjson_mut_doc *doc)` that returns a yyjson_mut_val* array containing all 5 tool schemas. This is the function that will be called from request serialization.

## TDD Cycle

### Red
1. Add test for `ik_tool_build_all()`:
   - Returns non-NULL yyjson_mut_val*
   - Is a JSON array (yyjson_mut_is_arr)
   - Has exactly 5 elements
   - First element has "function.name": "glob"
   - Second element has "function.name": "file_read"
   - Third element has "function.name": "grep"
   - Fourth element has "function.name": "file_write"
   - Fifth element has "function.name": "bash"
2. Add `ik_tool_build_all()` declaration to `src/tool.h`
3. Add stub in `src/tool.c`: `return NULL;`
4. Run `make check` - expect assertion failure (returns NULL, test expects valid array)

### Green
1. Replace stub in `src/tool.c` with implementation:
   - Create yyjson array
   - Call each schema builder function
   - Add each result to array
   - Return array
3. Run `make check` - expect pass

### Refactor
1. Verify order matches user story Request A
2. Consider if array size constant would be useful (probably not for story-01)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_build_all()` returns complete tools array
- Array has all 5 tools in correct order
- 100% test coverage
