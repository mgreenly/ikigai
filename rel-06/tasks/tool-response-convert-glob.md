# Task: Convert tool_glob.c to Use Response Builder

## Target

Refactoring #2: Eliminate Tool System Code Duplication - Response Building

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- patterns/callback-context

### Source Files

- src/tool_response.h (ik_tool_response_success_with_data, ik_tool_data_adder_t)
- src/tool_glob.c (inline JSON construction to be replaced)

### Test Patterns

- tests/unit/repl/repl_tool_test.c (glob tool tests)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_response_success_with_data()` function exists
- `tool_grep.c` already converted (pattern established)

## Task

Convert `tool_glob.c` to use the response builder instead of inline JSON construction.

### What

Replace the inline JSON construction in `ik_tool_exec_glob()` (lines 54-128) with the response builder.

### How

Add callback struct and function:

```c
// Glob result data for response callback
typedef struct {
    const char *output;
    size_t count;
} glob_result_data_t;

// Callback to add glob-specific fields to data object
static void add_glob_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    glob_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint(doc, data, "count", d->count);
}
```

Replace the success JSON construction (lines 101-128) with:
```c
glob_result_data_t result_data = {
    .output = output,
    .count = count
};
char *result;
ik_tool_response_success_with_data(parent, add_glob_data, &result_data, &result);
globfree(&glob_result);
return OK(result);
```

Note: The error handling path already uses `ik_tool_response_error()`.

### Why

Glob is similar to grep - has `output` and `count` fields. This continues the pattern established with grep conversion.

## TDD Cycle

### Red

No new tests needed - existing tests verify glob tool behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add callback struct and function
2. Replace inline JSON construction with response builder call
3. Ensure proper cleanup (globfree) still happens
4. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- All glob-related tests pass
- JSON output format unchanged

## Post-conditions

- Inline JSON construction removed from `tool_glob.c`
- `glob_result_data_t` struct and `add_glob_data()` callback defined
- `ik_tool_exec_glob()` uses `ik_tool_response_success_with_data()`
- All existing tests pass unchanged
