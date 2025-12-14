# Task: Create Tool Test JSON Parsing Helpers

## Target

Refactoring: Eliminate tool test JSON parsing duplication

## Pre-read Skills

- default
- database
- errors
- git
- log
- makefile
- naming
- quality
- scm
- source-code
- style
- tdd

## Pre-read Source Files

- `tests/test_utils.h` - Test utilities header
- `tests/test_utils.c` - Test utilities implementation
- `tests/unit/tool/bash_execute_test.c` - Example of duplication pattern
- `tests/unit/tool/file_read_execute_test.c` - Another example
- `src/vendor/yyjson/yyjson.h` - JSON library API

## Pre-read Documentation

- `docs/README.md` - Project documentation hub
- `project/error_handling.md` - Error handling patterns

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- All tests pass (`make check`)
- Current pattern: Every tool test manually parses JSON responses with 20-30 lines of boilerplate
- Pattern appears in 50+ tests across 8 files

## Task

Create reusable test helper functions in `tests/test_utils.h` and `tests/test_utils.c` to eliminate JSON parsing duplication in tool execution tests.

**Helper functions to create:**

1. **`ik_test_tool_parse_success`** - Parse JSON, verify success=true, return data object
   - Signature: `yyjson_val *ik_test_tool_parse_success(const char *json, yyjson_doc **out_doc)`
   - Returns: data object from response (caller must free *out_doc)
   - Asserts: JSON is valid, success=true, data object exists

2. **`ik_test_tool_parse_error`** - Parse JSON, verify success=false, return error string
   - Signature: `const char *ik_test_tool_parse_error(const char *json, yyjson_doc **out_doc)`
   - Returns: error message string (caller must free *out_doc)
   - Asserts: JSON is valid, success=false, error string exists

3. **`ik_test_tool_get_output`** - Extract output field from data object
   - Signature: `const char *ik_test_tool_get_output(yyjson_val *data)`
   - Returns: output string
   - Asserts: output field exists and is string

4. **`ik_test_tool_get_exit_code`** - Extract exit_code field from data object
   - Signature: `int64_t ik_test_tool_get_exit_code(yyjson_val *data)`
   - Returns: exit code integer
   - Asserts: exit_code field exists and is integer

**Example transformation:**

Before (20 lines):
```c
res_t res = ik_tool_exec_bash(ctx, "echo test");
ck_assert(!res.is_err);
char *json = res.ok;
ck_assert_ptr_nonnull(json);

yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
ck_assert_ptr_nonnull(doc);
yyjson_val *root = yyjson_doc_get_root(doc);
ck_assert(yyjson_is_obj(root));

yyjson_val *success = yyjson_obj_get(root, "success");
ck_assert_ptr_nonnull(success);
ck_assert(yyjson_get_bool(success) == true);

yyjson_val *data = yyjson_obj_get(root, "data");
ck_assert_ptr_nonnull(data);

yyjson_val *output = yyjson_obj_get(data, "output");
const char *output_str = yyjson_get_str(output);
ck_assert(strstr(output_str, "test") != NULL);

yyjson_doc_free(doc);
```

After (5 lines):
```c
res_t res = ik_tool_exec_bash(ctx, "echo test");
ck_assert(!res.is_err);

yyjson_doc *doc = NULL;
yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
const char *output = ik_test_tool_get_output(data);
ck_assert(strstr(output, "test") != NULL);

yyjson_doc_free(doc);
```

## TDD Cycle

### Red: Write Failing Tests

Create `tests/unit/test_utils/tool_json_helpers_test.c`:

1. **Test `ik_test_tool_parse_success` with valid success response**
   - Given: `{"success": true, "data": {"output": "test"}}`
   - Verify: Returns data object, doc is allocated
   - Verify: Can extract output field

2. **Test `ik_test_tool_parse_error` with valid error response**
   - Given: `{"success": false, "error": "File not found"}`
   - Verify: Returns "File not found", doc is allocated

3. **Test `ik_test_tool_get_output` extracts output field**
   - Given: data object with output field
   - Verify: Returns correct string

4. **Test `ik_test_tool_get_exit_code` extracts exit_code field**
   - Given: data object with exit_code field
   - Verify: Returns correct integer

Add test declarations to `tests/test_utils.h`.
Add stub implementations to `tests/test_utils.c` that compile but fail assertions.
Update `tests/unit/test_utils/Makefile.am` (if it exists) or create test build rules.

Run `make check` - tests should **FAIL** with clear assertion failures.

Commit: `git add -A && git commit -m "test: add tool JSON helper tests (RED)"`

### Green: Minimal Implementation

Implement the four helper functions in `tests/test_utils.c`:

1. **`ik_test_tool_parse_success`**:
   - Parse JSON with `yyjson_read()`
   - Assert doc not NULL
   - Get root object
   - Assert root is object
   - Get success field
   - Assert success = true
   - Get data field
   - Assert data exists and is object
   - Return data, set *out_doc

2. **`ik_test_tool_parse_error`**:
   - Parse JSON
   - Assert doc not NULL
   - Get root object
   - Get success field
   - Assert success = false
   - Get error field
   - Assert error exists and is string
   - Return error string, set *out_doc

3. **`ik_test_tool_get_output`**:
   - Assert data not NULL
   - Get output field
   - Assert output exists
   - Return output string

4. **`ik_test_tool_get_exit_code`**:
   - Assert data not NULL
   - Get exit_code field
   - Assert exit_code exists
   - Return integer value

Run `make check` - all tests should **PASS**.

Commit: `git add -A && git commit -m "feat: add tool JSON test helpers (GREEN)"`

### Refactor: Clean Up

Review implementation:
- Check for proper NULL handling
- Verify assertion messages are clear
- Ensure consistent style with project conventions
- Add brief function comments in header

Run `make check && make lint && make coverage` - all should pass.

Commit: `git add -A && git commit -m "refactor: polish tool JSON helpers"`

## Post-conditions

- Four new helper functions exist in `tests/test_utils.h` and `tests/test_utils.c`
- New test file `tests/unit/test_utils/tool_json_helpers_test.c` with 100% coverage
- All existing tests still pass (`make check`)
- Lint passes (`make lint`)
- Coverage maintained at 100% (`make coverage`)
- Working tree is clean
- All changes committed with descriptive messages
- Helper functions ready to use in subsequent refactoring tasks
