# Task: Convert tool_file_read.c to Use Response Builder

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
- src/tool_file_read.c (inline JSON construction to be replaced)

### Test Patterns

- tests/unit/repl/repl_tool_test.c (file_read tool tests)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_response_success_with_data()` function exists
- Pattern established in previous conversions

## Task

Convert `tool_file_read.c` to use the response builder instead of inline JSON construction.

### What

Replace the inline JSON construction in `ik_tool_exec_file_read()` (lines 65-102) with the response builder.

### How

Add callback struct and function:

```c
// File read result data for response callback
typedef struct {
    const char *output;
} file_read_result_data_t;

// Callback to add file_read-specific fields to data object
static void add_file_read_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    file_read_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
}
```

Replace the success JSON construction with:
```c
file_read_result_data_t result_data = {
    .output = buffer
};
char *result;
ik_tool_response_success_with_data(parent, add_file_read_data, &result_data, &result);
return OK(result);
```

Note: File read only has `output`, no count or other fields.

### Why

File read demonstrates the simplest case - just one field in the data object. The callback pattern handles this cleanly.

## TDD Cycle

### Red

No new tests needed - existing tests verify file_read tool behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add callback struct and function
2. Replace inline JSON construction with response builder call
3. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- All file_read-related tests pass
- JSON output format unchanged

## Post-conditions

- Inline JSON construction removed from `tool_file_read.c`
- `file_read_result_data_t` struct and `add_file_read_data()` callback defined
- `ik_tool_exec_file_read()` uses `ik_tool_response_success_with_data()`
- All existing tests pass unchanged
