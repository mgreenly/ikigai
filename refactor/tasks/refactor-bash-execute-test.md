# Task: Refactor bash_execute_test.c to Use JSON Helpers

## Target

Refactoring: Eliminate JSON parsing duplication in bash_execute_test.c

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

- `tests/test_utils.h` - Test helper functions (with new JSON helpers)
- `tests/test_utils.c` - Test helper implementations
- `tests/unit/tool/bash_execute_test.c` - File to refactor
- `src/tool.h` - Tool execution API
- `src/vendor/yyjson/yyjson.h` - JSON library API

## Pre-read Documentation

- `docs/README.md` - Project documentation hub

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- Helper functions exist: `ik_test_tool_parse_success`, `ik_test_tool_parse_error`, `ik_test_tool_get_output`, `ik_test_tool_get_exit_code`
- Helper tests pass and have 100% coverage
- All tests pass (`make check`)

## Task

Refactor `tests/unit/tool/bash_execute_test.c` to use the new JSON helper functions, eliminating ~150-200 lines of duplicated boilerplate.

**Files affected:**
- `tests/unit/tool/bash_execute_test.c` - Replace manual JSON parsing with helpers

**Pattern to replace:**

From:
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
ck_assert(yyjson_is_obj(data));

yyjson_val *output = yyjson_obj_get(data, "output");
ck_assert_ptr_nonnull(output);
const char *output_str = yyjson_get_str(output);
ck_assert(strstr(output_str, "test") != NULL);

yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
ck_assert_ptr_nonnull(exit_code);
ck_assert_int_eq(yyjson_get_int(exit_code), 0);

yyjson_doc_free(doc);
```

To:
```c
res_t res = ik_tool_exec_bash(ctx, "echo test");
ck_assert(!res.is_err);

yyjson_doc *doc = NULL;
yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
const char *output = ik_test_tool_get_output(data);
ck_assert(strstr(output, "test") != NULL);
ck_assert_int_eq(ik_test_tool_get_exit_code(data), 0);

yyjson_doc_free(doc);
```

## TDD Cycle

### Red: Verify Current Tests Pass

Run `make build/tests/unit/tool/bash_execute_test && ./build/tests/unit/tool/bash_execute_test` - verify ALL tests pass with current implementation.

This establishes baseline - we're not changing behavior, only implementation.

### Green: Refactor Using Helpers

Systematically refactor each test in `bash_execute_test.c`:

**Tests to refactor:**
1. `test_bash_exec_echo_command` - Success case with output
2. `test_bash_exec_nonzero_exit` - Success case with non-zero exit
3. `test_bash_exec_no_output` - Success case with empty output
4. `test_bash_exec_multiline_output` - Success case with multiline output
5. `test_bash_exec_stderr_output` - Success case with stderr
6. `test_bash_exec_special_characters` - Success case with special chars
7. Any error cases (if present) - Use `ik_test_tool_parse_error`

**Refactoring steps for each test:**
1. Locate the JSON parsing boilerplate (starts with `yyjson_doc *doc = yyjson_read`)
2. Replace with:
   - `yyjson_doc *doc = NULL;`
   - `yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);` (or `parse_error` for error cases)
3. Replace field extractions with helper calls:
   - `yyjson_obj_get(data, "output")` → `ik_test_tool_get_output(data)`
   - `yyjson_obj_get(data, "exit_code")` → `ik_test_tool_get_exit_code(data)`
4. Keep the `yyjson_doc_free(doc);` at the end
5. Verify test compiles
6. Run test to verify it still passes

After each test refactored, verify: `make build/tests/unit/tool/bash_execute_test && ./build/tests/unit/tool/bash_execute_test`

Commit after refactoring each test or small group:
```bash
git add tests/unit/tool/bash_execute_test.c
git commit -m "refactor: use JSON helpers in test_bash_exec_echo_command"
```

### Refactor: Clean Up and Verify

After all tests refactored:

1. Review the file - verify consistent style
2. Remove any unused includes (if JSON parsing headers no longer needed directly)
3. Check that all assertions are still meaningful
4. Verify proper cleanup (doc freed in all paths)

Run full test suite: `make check` - all tests should pass.
Run lint: `make lint` - should pass.
Run coverage: `make coverage` - should maintain 100%.

Final commit:
```bash
git add tests/unit/tool/bash_execute_test.c
git commit -m "refactor: complete bash_execute_test.c JSON helper migration"
```

## Post-conditions

- `tests/unit/tool/bash_execute_test.c` uses helper functions for all JSON parsing
- ~150-200 lines of boilerplate removed
- All tests in bash_execute_test.c still pass
- Test behavior unchanged (refactoring only)
- Full test suite passes (`make check`)
- Lint passes (`make lint`)
- Coverage maintained (`make coverage`)
- Working tree is clean
- All changes committed with descriptive messages
