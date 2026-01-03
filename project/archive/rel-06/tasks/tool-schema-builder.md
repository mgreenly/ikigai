# Task: Implement ik_tool_build_schema_from_def()

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

- src/tool.h (ik_tool_param_def_t, ik_tool_schema_def_t structs)
- src/tool.c (existing schema builders - use as reference for JSON structure)
- src/tool.c lines 62-97 (ik_tool_build_glob_schema - reference implementation)

### Test Patterns

- tests/unit/tool/tool_test.c (verify_schema_basics, verify_string_param, verify_required helpers)

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_param_def_t` struct exists
- `ik_tool_schema_def_t` struct exists

## Task

Implement `ik_tool_build_schema_from_def()` function that builds JSON schema from definition struct.

### What

Create function that:
1. Takes a `yyjson_mut_doc *` and `const ik_tool_schema_def_t *`
2. Builds complete OpenAI tool schema JSON
3. Returns `yyjson_mut_val *` (same as existing builders)

### How

Add declaration to `src/tool.h`:

```c
// Build JSON schema from tool definition (data-driven schema builder)
//
// Creates a tool schema object following OpenAI's function calling format
// from a declarative definition. The schema includes the tool name,
// description, and parameter specifications derived from the definition.
//
// @param doc The yyjson mutable document to build the schema in
// @param def Tool schema definition containing name, description, and parameters
// @return Pointer to the schema object (owned by doc), or NULL on error
yyjson_mut_val *ik_tool_build_schema_from_def(yyjson_mut_doc *doc,
                                               const ik_tool_schema_def_t *def);
```

Add implementation to `src/tool.c`:

```c
yyjson_mut_val *ik_tool_build_schema_from_def(yyjson_mut_doc *doc,
                                               const ik_tool_schema_def_t *def)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(def != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", def->name)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, function, "description", def->description)) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Add all parameters from definition
    for (size_t i = 0; i < def->param_count; i++) {
        ik_tool_add_string_param(doc, properties, def->params[i].name, def->params[i].description);
    }

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    // Build required array from params marked as required
    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < def->param_count; i++) {
        if (def->params[i].required) {
            if (!yyjson_mut_arr_add_str(doc, required, def->params[i].name)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed");  // LCOV_EXCL_BR_LINE

    return schema;
}
```

### Why

This function is the core of the refactoring. It replaces 5 nearly-identical schema builder functions with one data-driven implementation. Each tool definition becomes ~5 lines of declarative data instead of ~35 lines of imperative code.

## TDD Cycle

### Red

Write test that:
1. Creates a test schema definition
2. Calls `ik_tool_build_schema_from_def()`
3. Verifies output matches expected structure using existing helpers

```c
START_TEST(test_tool_build_schema_from_def_basic) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    static const ik_tool_param_def_t params[] = {
        {"pattern", "Glob pattern", true},
        {"path", "Base directory", false}
    };

    ik_tool_schema_def_t def = {
        .name = "test_glob",
        .description = "Test glob tool",
        .params = params,
        .param_count = 2
    };

    yyjson_mut_val *schema = ik_tool_build_schema_from_def(doc, &def);
    verify_schema_basics(schema, "test_glob");

    yyjson_mut_val *parameters = get_parameters(schema);
    yyjson_mut_val *properties = yyjson_mut_obj_get(parameters, "properties");
    ck_assert_ptr_nonnull(properties);

    verify_string_param(properties, "pattern");
    verify_string_param(properties, "path");

    const char *required_params[] = {"pattern"};
    verify_required(parameters, required_params, 1);

    yyjson_mut_doc_free(doc);
}
END_TEST
```

### Green

1. Add function declaration to `src/tool.h`
2. Add function implementation to `src/tool.c`
3. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- Generated schema matches existing schema builders

## Post-conditions

- `ik_tool_build_schema_from_def()` function declared in `src/tool.h`
- Function implemented in `src/tool.c`
- Test verifies correct JSON structure
- All existing tests still pass
