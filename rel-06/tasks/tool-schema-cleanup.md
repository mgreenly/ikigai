# Task: Document Schema Builder Pattern and Verify

## Target

Refactoring #2: Eliminate Tool System Code Duplication - Schema Building (Completion)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- source-code

### Source Files

- src/tool.h (final API with all structs and functions)
- src/tool.c (converted schema builders)

### Test Patterns

- tests/unit/tool/tool_test.c (all schema tests)

## Pre-conditions

- Working tree is clean
- `make check` passes
- All 5 schema builders converted to use `ik_tool_build_schema_from_def()`

## Task

Update documentation and verify the schema building refactoring is complete.

### What

1. Add documentation comments to static schema definitions
2. Verify all schema builders produce identical output to original
3. Update any related documentation

### How

Add comment block above the static definitions in `src/tool.c`:

```c
// =============================================================================
// Tool Schema Definitions (Data-Driven)
//
// Each tool is defined declaratively using ik_tool_schema_def_t:
// - Static parameter arrays specify name, description, and required flag
// - Static schema definitions tie together name, description, and parameters
// - Public ik_tool_build_*_schema() functions delegate to ik_tool_build_schema_from_def()
//
// To add a new tool:
// 1. Define static const ik_tool_param_def_t params[] = {...};
// 2. Define static const ik_tool_schema_def_t schema_def = {...};
// 3. Add thin wrapper: yyjson_mut_val *ik_tool_build_X_schema(doc) {
//        return ik_tool_build_schema_from_def(doc, &schema_def);
//    }
// =============================================================================
```

### Why

Clear documentation ensures:
- Future developers understand the pattern
- Adding new tools follows consistent process
- Intent of refactoring is preserved

## TDD Cycle

### Red

Write a comparison test that verifies the new implementation produces the same JSON as what the old implementation would have produced. This is a sanity check.

```c
START_TEST(test_schema_definitions_complete) {
    // Verify all 5 tool schemas can be built and have expected tool names
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    const char *expected_names[] = {"glob", "file_read", "grep", "file_write", "bash"};
    yyjson_mut_val *arr = ik_tool_build_all(doc);
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 5);

    for (size_t i = 0; i < 5; i++) {
        yyjson_mut_val *schema = yyjson_mut_arr_get(arr, i);
        yyjson_mut_val *function = yyjson_mut_obj_get(schema, "function");
        yyjson_mut_val *name = yyjson_mut_obj_get(function, "name");
        ck_assert_str_eq(yyjson_mut_get_str(name), expected_names[i]);
    }

    yyjson_mut_doc_free(doc);
}
END_TEST
```

### Green

1. Add documentation comment block
2. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- Documentation is clear and accurate

## Post-conditions

- Schema builder refactoring is complete
- Documentation explains the pattern
- All tests pass
- Pattern is ready for use when adding new tools
