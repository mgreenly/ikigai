# Fix: Tool Call Arrow Format

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/format.h` - Format buffer interface
- `src/format.c` - Format functions including `ik_format_tool_call()`
- `src/tool.h` - Tool call structure definition

### Test patterns:
- `tests/unit/format_test.c` - Existing format tests

## Situation

Current tool call format: `[tool] tool_name({"pattern": "*.c", "path": "src"})`

Desired format: `→ tool_name: pattern="*.c", path="src"`

The arrow prefix (→) provides visual distinction. Arguments should be formatted as key="value" pairs, not raw JSON.

## Task

Update `ik_format_tool_call()` to produce the new format:
1. Use `→` prefix instead of `[tool]`
2. Parse arguments JSON and format as `key="value"` pairs
3. Separate multiple arguments with `, `

### Output Format Examples

Single argument:
```
→ glob: pattern="*.c"
```

Multiple arguments:
```
→ file_read: path="/src/main.c", offset=0, limit=100
```

No arguments:
```
→ some_tool
```

## TDD Cycle

### Red Phase
Update tests in `tests/unit/format_test.c`:

```c
START_TEST(test_format_tool_call_single_arg)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_123",
        .name = "glob",
        .arguments = "{\"pattern\": \"*.c\"}"
    };
    const char *result = ik_format_tool_call(ctx, &call);
    ck_assert_str_eq(result, "→ glob: pattern=\"*.c\"");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_call_multiple_args)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_456",
        .name = "file_read",
        .arguments = "{\"path\": \"/src/main.c\", \"offset\": 0, \"limit\": 100}"
    };
    const char *result = ik_format_tool_call(ctx, &call);
    // Note: JSON object order may vary, check contains key parts
    ck_assert_ptr_nonnull(strstr(result, "→ file_read:"));
    ck_assert_ptr_nonnull(strstr(result, "path=\"/src/main.c\""));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_call_no_args)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_789",
        .name = "some_tool",
        .arguments = "{}"
    };
    const char *result = ik_format_tool_call(ctx, &call);
    ck_assert_str_eq(result, "→ some_tool");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_call_null_args)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_000",
        .name = "tool_x",
        .arguments = NULL
    };
    const char *result = ik_format_tool_call(ctx, &call);
    ck_assert_str_eq(result, "→ tool_x");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_call_invalid_json)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_bad",
        .name = "broken",
        .arguments = "not valid json"
    };
    const char *result = ik_format_tool_call(ctx, &call);
    // Fallback: show raw arguments
    ck_assert_str_eq(result, "→ broken: not valid json");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_call_string_value)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_str",
        .name = "bash",
        .arguments = "{\"command\": \"ls -la\"}"
    };
    const char *result = ik_format_tool_call(ctx, &call);
    ck_assert_str_eq(result, "→ bash: command=\"ls -la\"");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_call_bool_value)
{
    void *ctx = talloc_new(NULL);
    ik_tool_call_t call = {
        .id = "call_bool",
        .name = "file_write",
        .arguments = "{\"path\": \"test.txt\", \"create\": true}"
    };
    const char *result = ik_format_tool_call(ctx, &call);
    ck_assert_ptr_nonnull(strstr(result, "→ file_write:"));
    ck_assert_ptr_nonnull(strstr(result, "path=\"test.txt\""));
    ck_assert_ptr_nonnull(strstr(result, "create=true"));
    talloc_free(ctx);
}
END_TEST
```

### Green Phase
Update `ik_format_tool_call()` in `src/format.c`:

```c
const char *ik_format_tool_call(void *parent, const ik_tool_call_t *call)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE
    assert(call != NULL);    // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = ik_format_buffer_create(parent);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Start with arrow and tool name
    res_t res = ik_format_appendf(buf, "→ %s", call->name);
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    // Handle missing or empty arguments
    if (call->arguments == NULL || call->arguments[0] == '\0') {
        return ik_format_get_string(buf);
    }

    // Try to parse JSON arguments
    yyjson_doc *doc = yyjson_read_(call->arguments, strlen(call->arguments), 0);
    if (doc == NULL) {
        // Invalid JSON - show raw arguments as fallback
        res = ik_format_appendf(buf, ": %s", call->arguments);
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        // Not an object - show raw
        yyjson_doc_free(doc);
        res = ik_format_appendf(buf, ": %s", call->arguments);
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    // Check if object is empty
    size_t obj_size = yyjson_obj_size(root);
    if (obj_size == 0) {
        yyjson_doc_free(doc);
        return ik_format_get_string(buf);
    }

    // Format key=value pairs
    res = ik_format_append(buf, ": ");
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    bool first = true;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val *key;
    while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
        yyjson_val *val = yyjson_obj_iter_get_val(key);

        if (!first) {
            res = ik_format_append(buf, ", ");
            if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        }
        first = false;

        const char *key_str = yyjson_get_str(key);

        // Format value based on type
        if (yyjson_is_str(val)) {
            res = ik_format_appendf(buf, "%s=\"%s\"", key_str, yyjson_get_str(val));
        } else if (yyjson_is_int(val)) {
            res = ik_format_appendf(buf, "%s=%" PRId64, key_str, yyjson_get_sint(val));
        } else if (yyjson_is_real(val)) {
            res = ik_format_appendf(buf, "%s=%g", key_str, yyjson_get_real(val));
        } else if (yyjson_is_bool(val)) {
            res = ik_format_appendf(buf, "%s=%s", key_str, yyjson_get_bool(val) ? "true" : "false");
        } else if (yyjson_is_null(val)) {
            res = ik_format_appendf(buf, "%s=null", key_str);
        } else {
            // Arrays/objects - show as JSON
            const char *val_str = yyjson_val_write(val, 0, NULL);
            if (val_str != NULL) {
                res = ik_format_appendf(buf, "%s=%s", key_str, val_str);
                free((void *)val_str);
            }
        }
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
    }

    yyjson_doc_free(doc);
    return ik_format_get_string(buf);
}
```

### Verify Phase
```bash
make check  # All tests pass
make lint   # No complexity issues
```

## Notes

- The `→` character is UTF-8 (3 bytes: 0xE2 0x86 0x92)
- JSON object iteration order may vary; tests should check for key presence, not exact string match for multi-arg cases
- Fallback to raw arguments for invalid JSON maintains robustness

## Success Criteria

- All new tests pass
- Existing tests updated to expect new format
- `make check` passes
- `make lint` passes
- Output format: `→ tool_name: key="value", key2=value2`
