# Task: Define ik_tool_param_def_t Struct

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

- src/tool.h (existing tool API declarations)
- src/tool.c (schema builder functions, lines 62-249)

### Test Patterns

- tests/unit/tool/tool_test.c (test helpers verify_schema_basics, verify_string_param)

## Pre-conditions

- Working tree is clean
- `make check` passes

## Task

Define `ik_tool_param_def_t` struct for declarative parameter specification.

### What

Create a struct that captures tool parameter metadata:
- Parameter name
- Parameter description
- Whether parameter is required

This enables data-driven schema building instead of repetitive code.

### How

Add to `src/tool.h`:

```c
// Parameter definition for data-driven schema building
typedef struct {
    const char *name;        // Parameter name (e.g., "pattern")
    const char *description; // Parameter description
    bool required;           // true if parameter is required
} ik_tool_param_def_t;
```

### Why

Currently 5 schema builder functions (`ik_tool_build_glob_schema`, etc.) repeat:
- `ik_tool_add_string_param()` calls
- Required array construction

With `ik_tool_param_def_t`, parameters become declarative data:
```c
static const ik_tool_param_def_t glob_params[] = {
    {"pattern", "Glob pattern (e.g., 'src/**/*.c')", true},
    {"path", "Base directory (default: cwd)", false}
};
```

This is the foundation for `ik_tool_schema_def_t` and `ik_tool_build_schema_from_def()`.

## TDD Cycle

### Red

Write test that:
1. Declares a `ik_tool_param_def_t` variable
2. Verifies fields can be accessed
3. Test fails because struct doesn't exist

```c
START_TEST(test_tool_param_def_struct_exists) {
    ik_tool_param_def_t param = {
        .name = "test_param",
        .description = "Test description",
        .required = true
    };
    ck_assert_str_eq(param.name, "test_param");
    ck_assert_str_eq(param.description, "Test description");
    ck_assert(param.required);
}
END_TEST
```

### Green

1. Add struct definition to `src/tool.h`
2. Run `make check` - test passes

### Verify

- `make check` passes
- `make lint` passes
- Struct is usable in test code

## Post-conditions

- `ik_tool_param_def_t` struct defined in `src/tool.h`
- Struct has `name`, `description`, `required` fields
- Test verifies struct is usable
- All existing tests still pass
