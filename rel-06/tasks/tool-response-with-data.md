# Task: Add ik_tool_response_success_with_data() Function

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

- src/tool_response.h (existing API: ik_tool_response_error, ik_tool_response_success, ik_tool_response_success_ex)
- src/tool_response.c (existing implementations)
- src/tool_grep.c lines 83-119 (build_grep_success - example of response with data object)
- src/tool_glob.c lines 54-128 (success response with data object)

### Test Patterns

- tests/unit/tool_response/tool_response_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes

## Task

Add `ik_tool_response_success_with_data()` function that builds responses with a nested `data` object.

### What

The existing `ik_tool_response_success()` builds: `{"success": true, "output": "..."}`

But tool implementations build: `{"success": true, "data": {"output": "...", "count": N}}`

Add a function that:
1. Creates the standard envelope with `success: true`
2. Creates a `data` object
3. Calls a user callback to populate the data object
4. Returns the JSON string

### How

Add to `src/tool_response.h`:

```c
// Callback type for adding fields to the data object
typedef void (*ik_tool_data_adder_t)(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx);

// Build success response with data object: {"success": true, "data": {...}}
// Caller provides a callback to populate the data object with tool-specific fields.
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
res_t ik_tool_response_success_with_data(TALLOC_CTX *ctx,
                                          ik_tool_data_adder_t add_data,
                                          void *user_ctx,
                                          char **out);
```

Add to `src/tool_response.c`:

```c
res_t ik_tool_response_success_with_data(TALLOC_CTX *ctx,
                                          ik_tool_data_adder_t add_data,
                                          void *user_ctx,
                                          char **out)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(add_data != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);       // LCOV_EXCL_BR_LINE

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);

    yyjson_mut_val *data = yyjson_mut_obj(doc);
    if (data == NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Call user callback to populate data object
    add_data(doc, data, user_ctx);

    yyjson_mut_obj_add_val(doc, root, "data", data);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc);  // LCOV_EXCL_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *out = result;
    return OK(result);
}
```

### Why

The existing `ik_tool_response_success_ex()` adds fields to the root object.
Tool implementations need to add fields to a `data` sub-object.

This function eliminates ~20 lines of boilerplate from each tool:
- Creating yyjson doc
- Creating root object
- Adding success field
- Creating data object
- Final JSON serialization and talloc copy

Tools only need to provide a 3-line callback to add their specific fields.

## TDD Cycle

### Red

Add test to `tests/unit/tool_response/tool_response_test.c`:

```c
typedef struct {
    const char *output;
    size_t count;
} test_data_t;

static void add_test_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *user_ctx) {
    test_data_t *d = user_ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint(doc, data, "count", d->count);
}

START_TEST(test_tool_response_success_with_data) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *result = NULL;

    test_data_t data = {.output = "test output", .count = 42};
    res_t res = ik_tool_response_success_with_data(ctx, add_test_data, &data, &result);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(result);

    // Parse and verify JSON structure
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success));

    yyjson_val *data_obj = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data_obj);

    yyjson_val *output = yyjson_obj_get(data_obj, "output");
    ck_assert_str_eq(yyjson_get_str(output), "test output");

    yyjson_val *count = yyjson_obj_get(data_obj, "count");
    ck_assert_int_eq(yyjson_get_uint(count), 42);

    yyjson_doc_free(doc);
    talloc_free(ctx);
}
END_TEST
```

### Green

1. Add callback typedef to `src/tool_response.h`
2. Add function declaration to `src/tool_response.h`
3. Add function implementation to `src/tool_response.c`
4. Add test to test file
5. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- New function produces expected JSON structure

## Post-conditions

- `ik_tool_data_adder_t` callback type defined in `src/tool_response.h`
- `ik_tool_response_success_with_data()` declared in `src/tool_response.h`
- Function implemented in `src/tool_response.c`
- Test verifies correct JSON structure with data object
- All existing tests still pass
