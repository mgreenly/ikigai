# Task: Convert tool_grep.c to Use Response Builder

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
- src/tool_grep.c (build_grep_success function to be replaced)

### Test Patterns

- tests/unit/repl/repl_tool_test.c (grep tool tests)
- tests/integration/* (any grep-related tests)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_response_success_with_data()` function exists

## Task

Convert `tool_grep.c` to use the response builder instead of inline JSON construction.

### What

1. Remove `build_grep_success()` static function
2. Create callback struct and function for grep-specific data
3. Use `ik_tool_response_success_with_data()` in `ik_tool_exec_grep()`

### How

Replace the static `build_grep_success()` function (lines 83-119) with a callback:

```c
// Grep result data for response callback
typedef struct {
    const char *output;
    size_t count;
} grep_result_data_t;

// Callback to add grep-specific fields to data object
static void add_grep_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx)
{
    grep_result_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint(doc, data, "count", d->count);
}
```

At the end of `ik_tool_exec_grep()`, replace:
```c
char *result = build_grep_success(parent, output_buffer, match_count);
return OK(result);
```

With:
```c
grep_result_data_t result_data = {
    .output = output_buffer,
    .count = match_count
};
char *result;
ik_tool_response_success_with_data(parent, add_grep_data, &result_data, &result);
return OK(result);
```

### Why

This is a proof of concept for the response builder pattern:
- Demonstrates that existing tests pass with new implementation
- Shows reduction from 35 lines of JSON building to ~10 lines
- Other tool implementations follow the same pattern
- Grep is a good test case because it has both output and count fields

## TDD Cycle

### Red

No new tests needed - existing tests verify grep tool behavior.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add callback struct typedef above `ik_tool_exec_grep()`
2. Add callback function
3. Replace call to `build_grep_success()` with response builder
4. Remove `build_grep_success()` function
5. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- All grep-related tests pass
- JSON output format unchanged

## Post-conditions

- `build_grep_success()` removed from `tool_grep.c`
- `grep_result_data_t` struct and `add_grep_data()` callback defined
- `ik_tool_exec_grep()` uses `ik_tool_response_success_with_data()`
- All existing tests pass unchanged
- ~35 lines of boilerplate replaced with ~10 lines
