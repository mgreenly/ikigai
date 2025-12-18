# Task: Convert glob Schema to Use New Builder

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

- src/tool.h (ik_tool_build_schema_from_def, ik_tool_schema_def_t)
- src/tool.c lines 62-97 (ik_tool_build_glob_schema - to be converted)

### Test Patterns

- tests/unit/tool/tool_test.c (test_tool_build_glob_schema_structure - must still pass)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_build_schema_from_def()` function exists and works

## Task

Convert `ik_tool_build_glob_schema()` to use declarative definition and new builder.

### What

1. Create static `glob_params[]` array with parameter definitions
2. Create static `glob_schema_def` definition struct
3. Change `ik_tool_build_glob_schema()` to call `ik_tool_build_schema_from_def()`
4. Keep function signature unchanged (backwards compatible)

### How

In `src/tool.c`, replace the glob schema builder:

```c
// Static definitions for glob tool schema
static const ik_tool_param_def_t glob_params[] = {
    {"pattern", "Glob pattern (e.g., 'src/**/*.c')", true},
    {"path", "Base directory (default: cwd)", false}
};

static const ik_tool_schema_def_t glob_schema_def = {
    .name = "glob",
    .description = "Find files matching a glob pattern",
    .params = glob_params,
    .param_count = 2
};

yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &glob_schema_def);
}
```

### Why

This is a proof of concept for the refactoring pattern:
- Demonstrates that existing tests pass with new implementation
- Shows the dramatic reduction in code (35 lines -> 10 lines)
- Other schema builders follow the same pattern
- Keeps old function as thin wrapper for backwards compatibility

## TDD Cycle

### Red

No new tests needed - existing test `test_tool_build_glob_schema_structure` verifies the function.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add static definition arrays above `ik_tool_build_glob_schema()`
2. Replace function body with single call to `ik_tool_build_schema_from_def()`
3. Run `make check` - all tests pass (including glob schema structure test)

### Verify

- `make check` passes
- `make lint` passes
- `test_tool_build_glob_schema_structure` passes
- `test_tool_build_all` passes (depends on glob schema)

## Post-conditions

- `ik_tool_build_glob_schema()` uses declarative definition
- Function signature unchanged (public API stable)
- Static `glob_params[]` and `glob_schema_def` defined
- All existing tests pass unchanged
