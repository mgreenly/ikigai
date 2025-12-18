# Task: Define ik_tool_schema_def_t Struct

## Target

Refactoring #2: Eliminate Tool System Code Duplication - Schema Building

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- patterns/factory

### Source Files

- src/tool.h (existing tool API, ik_tool_param_def_t from previous task)
- src/tool.c (schema builder functions showing name, description, params patterns)

### Test Patterns

- tests/unit/tool/tool_test.c (verify_schema_basics checks name, description)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_param_def_t` struct exists in src/tool.h

## Task

Define `ik_tool_schema_def_t` struct for complete tool schema specification.

### What

Create a struct that captures complete tool metadata:
- Tool name
- Tool description
- Array of parameter definitions
- Parameter count

This enables a single function to build schemas for all tools.

### How

Add to `src/tool.h` (after `ik_tool_param_def_t`):

```c
// Tool schema definition for data-driven schema building
typedef struct {
    const char *name;                     // Tool name (e.g., "glob")
    const char *description;              // Tool description
    const ik_tool_param_def_t *params;    // Array of parameter definitions
    size_t param_count;                   // Number of parameters
} ik_tool_schema_def_t;
```

### Why

Currently each `ik_tool_build_*_schema()` function duplicates:
- Schema object creation
- Function object creation
- Name/description setting
- Parameters object creation
- Required array building

With `ik_tool_schema_def_t`, complete tool definitions become declarative:
```c
static const ik_tool_schema_def_t glob_schema = {
    .name = "glob",
    .description = "Find files matching a glob pattern",
    .params = glob_params,
    .param_count = 2
};
```

## TDD Cycle

### Red

Write test that:
1. Declares parameter definitions array
2. Declares `ik_tool_schema_def_t` variable referencing the params
3. Verifies all fields accessible
4. Test fails because struct doesn't exist

```c
START_TEST(test_tool_schema_def_struct_exists) {
    static const ik_tool_param_def_t params[] = {
        {"pattern", "Pattern to match", true},
        {"path", "Base path", false}
    };

    ik_tool_schema_def_t schema = {
        .name = "test_tool",
        .description = "Test tool description",
        .params = params,
        .param_count = 2
    };

    ck_assert_str_eq(schema.name, "test_tool");
    ck_assert_str_eq(schema.description, "Test tool description");
    ck_assert_ptr_eq(schema.params, params);
    ck_assert_uint_eq(schema.param_count, 2);
}
END_TEST
```

### Green

1. Add struct definition to `src/tool.h`
2. Run `make check` - test passes

### Verify

- `make check` passes
- `make lint` passes
- Struct is usable with parameter arrays

## Post-conditions

- `ik_tool_schema_def_t` struct defined in `src/tool.h`
- Struct has `name`, `description`, `params`, `param_count` fields
- Test verifies struct is usable with parameter arrays
- All existing tests still pass
