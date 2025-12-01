# Task: Tool Argument Parser Utilities

## Target
User story: 02-single-glob-call

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/coverage.md
- .agents/skills/testability.md
- .agents/skills/mocking.md

### Pre-read Docs
- docs/naming.md
- docs/memory.md
- docs/return_values.md
- docs/error_handling.md

### Pre-read Source (patterns)
- src/config.c (yyjson_obj_get_ and yyjson_get_str_ usage for parsing JSON)
- src/wrapper.h (yyjson wrapper functions for mocking)
- src/error.h (res_t type definition)
- src/tool.h (existing tool module)

### Pre-read Tests (patterns)
- tests/unit/config/config_test.c (JSON parsing test patterns)
- tests/unit/openai/client_structures_test.c (talloc test patterns)

## Pre-conditions
- `make check` passes
- `ik_tool_call_t` struct exists in `src/tool.h` with `arguments` field (JSON string)
- yyjson library is vendored and available
- Task `tool-call-struct.md` completed

## Task
Create helper functions to extract and parse arguments from tool call JSON. These utilities will be used by all tool execution functions to safely extract typed parameters from the `ik_tool_call_t.arguments` JSON string.

Functions to implement:
```c
// Extract string argument, returns NULL if key not found or wrong type
char *ik_tool_arg_get_string(void *parent, const char *arguments_json, const char *key);

// Extract integer argument, returns true on success, false if key not found or wrong type
bool ik_tool_arg_get_int(void *parent, const char *arguments_json, const char *key, int *out_value);
```

## TDD Cycle

### Red
1. Add tests to `tests/unit/tool/test_tool.c` or create `tests/unit/tool/test_tool_arg_parser.c`:
   - `ik_tool_arg_get_string()` with valid JSON returns extracted string
   - `ik_tool_arg_get_string()` with missing key returns NULL
   - `ik_tool_arg_get_string()` with wrong type (number instead of string) returns NULL
   - `ik_tool_arg_get_string()` with malformed JSON returns NULL
   - `ik_tool_arg_get_int()` with valid JSON returns true and sets out_value
   - `ik_tool_arg_get_int()` with missing key returns false
   - `ik_tool_arg_get_int()` with wrong type (string instead of number) returns false
   - `ik_tool_arg_get_int()` with malformed JSON returns false
   - `ik_tool_arg_get_int()` with NULL out_value returns false (defensive)
2. Test with realistic tool argument JSON:
   - `{"pattern": "*.c", "path": "src/"}`
   - `{"file_path": "/etc/hosts"}`
3. Add to `src/tool.h`:
   - Declare `ik_tool_arg_get_string(void *parent, const char *arguments_json, const char *key)`
   - Declare `ik_tool_arg_get_int(void *parent, const char *arguments_json, const char *key, int *out_value)`
4. Add stubs in `src/tool.c`:
   - `ik_tool_arg_get_string()`: `return NULL;`
   - `ik_tool_arg_get_int()`: `return false;`
5. Run `make check` - expect assertion failure (tests expect valid extractions)

### Green
1. Replace stubs in `src/tool.c` with implementations:
   - Use yyjson_read() to parse arguments_json
   - Use yyjson_obj_get_() to find key
   - Use yyjson_is_str() / yyjson_is_int() for type checking
   - Use yyjson_get_str_() / yyjson_get_int() to extract values
   - For string results: use talloc_strdup() to copy to parent context
   - Free yyjson_doc after use
   - Handle all error cases (NULL input, parse failure, key not found, type mismatch)
3. Run `make check` - expect pass

### Refactor
1. Ensure yyjson_doc is freed in all code paths
2. Verify NULL handling is defensive and safe
3. Check for code duplication in parsing logic
4. Ensure naming matches docs/naming.md
5. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint && make coverage` passes
- `ik_tool_arg_get_string()` safely extracts string arguments
- `ik_tool_arg_get_int()` safely extracts integer arguments
- Both functions handle malformed JSON gracefully
- 100% test coverage for new code
