# Task: Convert tool_file_write.c to Use Response Builder

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
- src/tool_file_write.c (inline JSON construction to be replaced)

### Test Patterns

- tests/unit/repl/repl_tool_test.c (file_write tool tests)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_response_success_with_data()` function exists
- Pattern established in previous conversions

## Task

Convert `tool_file_write.c` to use the response builder instead of inline JSON construction.

### What

Replace the inline JSON construction in `ik_tool_exec_file_write()` (lines 68-104) with the response builder.

### How

Add callback struct and function:

```c
// File write result data for response callback
typedef struct {
    const char *output;
    size_t bytes;
} file_write_result_data_t;

// Callback to add file_write-specific fields to data object
static void add_file_write_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    file_write_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint(doc, data, "bytes", d->bytes);
}
```

Replace the success JSON construction with:
```c
file_write_result_data_t result_data = {
    .output = output_msg,
    .bytes = bytes_written
};
char *result;
ik_tool_response_success_with_data(parent, add_file_write_data, &result_data, &result);
return OK(result);
```

Note: File write has `output` and `bytes` fields.

### Why

Completing all 5 tool conversions demonstrates the full value of the pattern - consistent response building across all tools.

## TDD Cycle

### Red

No new tests needed - existing tests verify file_write tool behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add callback struct and function
2. Replace inline JSON construction with response builder call
3. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- All file_write-related tests pass
- JSON output format unchanged

## Post-conditions

- Inline JSON construction removed from `tool_file_write.c`
- `file_write_result_data_t` struct and `add_file_write_data()` callback defined
- `ik_tool_exec_file_write()` uses `ik_tool_response_success_with_data()`
- All existing tests pass unchanged
