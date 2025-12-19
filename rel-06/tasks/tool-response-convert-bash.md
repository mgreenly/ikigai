# Task: Convert tool_bash.c to Use Response Builder

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
- src/tool_bash.c (inline JSON construction to be replaced)

### Test Patterns

- tests/unit/repl/repl_tool_test.c (bash tool tests)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_response_success_with_data()` function exists
- Pattern established in previous conversions

## Task

Convert `tool_bash.c` to use the response builder instead of inline JSON construction.

### What

Replace the inline JSON construction in `ik_tool_exec_bash()` (lines 74-108) with the response builder.

### How

Add callback struct and function:

```c
// Bash result data for response callback
typedef struct {
    const char *output;
    int exit_code;
} bash_result_data_t;

// Callback to add bash-specific fields to data object
static void add_bash_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    bash_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_int(doc, data, "exit_code", d->exit_code);
}
```

Replace the success JSON construction with:
```c
bash_result_data_t result_data = {
    .output = output,
    .exit_code = exit_code
};
char *result;
ik_tool_response_success_with_data(parent, add_bash_data, &result_data, &result);
return OK(result);
```

Note: Bash has `exit_code` (int) instead of `count` (size_t).

### Why

Bash demonstrates the flexibility of the callback pattern - different field types are easily handled by tool-specific callbacks.

## TDD Cycle

### Red

No new tests needed - existing tests verify bash tool behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add callback struct and function
2. Replace inline JSON construction with response builder call
3. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- All bash-related tests pass
- JSON output format unchanged

## Post-conditions

- Inline JSON construction removed from `tool_bash.c`
- `bash_result_data_t` struct and `add_bash_data()` callback defined
- `ik_tool_exec_bash()` uses `ik_tool_response_success_with_data()`
- All existing tests pass unchanged
