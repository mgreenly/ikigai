# Fix: Tool Result Truncate and Arrow Format

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
- `src/format.c` - Format functions including `ik_format_tool_result()`
- `src/tool.h` - Tool structures

### Test patterns:
- `tests/unit/format_test.c` - Existing format tests

## Situation

Current tool result format: `[result] tool_name: <full JSON result>`

Desired format: `← tool_name: <truncated summary>` with truncation at:
- 3 lines maximum, OR
- 400 characters maximum
- Whichever limit is reached first
- Add `...` when truncated

The arrow prefix (←) provides visual distinction and pairs with `→` for tool calls.

## Task

Update `ik_format_tool_result()` to:
1. Use `←` prefix instead of `[result]`
2. Extract meaningful content from JSON result (not raw JSON)
3. Truncate to 3 lines or 400 characters (whichever is less)
4. Add `...` when content is truncated

### Output Format Examples

Short result (no truncation):
```
← glob: src/main.c, src/config.c
```

Long result (character truncation):
```
← file_read: #include <stdio.h>
#include <stdlib.h>
int main(void) {
    printf("Hello...
```

Multi-line result (line truncation):
```
← grep: src/main.c:10: match one
src/main.c:25: match two
src/util.c:5: match three...
```

Error result:
```
← bash: [error] Command failed with exit code 1
```

Null result:
```
← tool_name: (no output)
```

## TDD Cycle

### Red Phase
Update tests in `tests/unit/format_test.c`:

```c
START_TEST(test_format_tool_result_short)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "glob", "[\"a.c\", \"b.c\"]");
    ck_assert_str_eq(result, "← glob: a.c, b.c");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_null)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "tool_x", NULL);
    ck_assert_str_eq(result, "← tool_x: (no output)");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_empty_string)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "bash", "\"\"");
    ck_assert_str_eq(result, "← bash: (no output)");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_truncate_chars)
{
    void *ctx = talloc_new(NULL);
    // Create a string > 400 chars
    char long_content[500];
    memset(long_content, 'x', 450);
    long_content[450] = '\0';
    char json[600];
    snprintf(json, sizeof(json), "\"%s\"", long_content);

    const char *result = ik_format_tool_result(ctx, "file_read", json);
    // Should be truncated with ...
    ck_assert(strlen(result) <= 420);  // ← file_read: + 400 + ...
    ck_assert_ptr_nonnull(strstr(result, "..."));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_truncate_lines)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "grep",
        "\"line1\\nline2\\nline3\\nline4\\nline5\"");
    // Should show only 3 lines + ...
    ck_assert_ptr_nonnull(strstr(result, "← grep:"));
    ck_assert_ptr_nonnull(strstr(result, "line1"));
    ck_assert_ptr_nonnull(strstr(result, "line2"));
    ck_assert_ptr_nonnull(strstr(result, "line3"));
    ck_assert_ptr_null(strstr(result, "line4"));
    ck_assert_ptr_nonnull(strstr(result, "..."));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_error_object)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "bash",
        "{\"error\": \"Command failed\", \"exit_code\": 1}");
    ck_assert_ptr_nonnull(strstr(result, "← bash:"));
    ck_assert_ptr_nonnull(strstr(result, "error"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_array_of_strings)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "glob",
        "[\"file1.c\", \"file2.c\", \"file3.c\"]");
    ck_assert_str_eq(result, "← glob: file1.c, file2.c, file3.c");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_exactly_three_lines)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "grep",
        "\"line1\\nline2\\nline3\"");
    // Exactly 3 lines - no truncation needed
    ck_assert_ptr_nonnull(strstr(result, "line1"));
    ck_assert_ptr_nonnull(strstr(result, "line2"));
    ck_assert_ptr_nonnull(strstr(result, "line3"));
    ck_assert_ptr_null(strstr(result, "..."));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_format_tool_result_invalid_json)
{
    void *ctx = talloc_new(NULL);
    const char *result = ik_format_tool_result(ctx, "broken", "not json");
    // Fallback to raw content
    ck_assert_str_eq(result, "← broken: not json");
    talloc_free(ctx);
}
END_TEST
```

### Green Phase
Update `ik_format_tool_result()` in `src/format.c`:

```c
// Constants for truncation
#define TOOL_RESULT_MAX_CHARS 400
#define TOOL_RESULT_MAX_LINES 3

// Helper to truncate content
static const char *truncate_content(void *parent, const char *content, size_t len)
{
    if (content == NULL || len == 0) {
        return talloc_strdup_(parent, "(no output)");
    }

    // Count lines and find truncation point
    size_t line_count = 1;
    size_t char_count = 0;
    size_t truncate_at = len;
    bool needs_truncation = false;

    for (size_t i = 0; i < len; i++) {
        char_count++;

        if (content[i] == '\n') {
            line_count++;
            if (line_count > TOOL_RESULT_MAX_LINES) {
                truncate_at = i;
                needs_truncation = true;
                break;
            }
        }

        if (char_count >= TOOL_RESULT_MAX_CHARS) {
            truncate_at = i + 1;
            needs_truncation = true;
            break;
        }
    }

    if (!needs_truncation) {
        return talloc_strndup_(parent, content, len);
    }

    // Truncate and add ...
    char *result = talloc_asprintf_(parent, "%.*s...", (int)truncate_at, content);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return result;
}

const char *ik_format_tool_result(void *parent, const char *tool_name, const char *result_json)
{
    assert(parent != NULL);    // LCOV_EXCL_BR_LINE
    assert(tool_name != NULL); // LCOV_EXCL_BR_LINE

    ik_format_buffer_t *buf = ik_format_buffer_create(parent);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Start with arrow and tool name
    res_t res = ik_format_appendf(buf, "← %s: ", tool_name);
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    // Handle null result
    if (result_json == NULL) {
        res = ik_format_append(buf, "(no output)");
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    // Try to parse JSON
    yyjson_doc *doc = yyjson_read_(result_json, strlen(result_json), 0);
    if (doc == NULL) {
        // Invalid JSON - show raw, truncated
        const char *truncated = truncate_content(parent, result_json, strlen(result_json));
        res = ik_format_append(buf, truncated);
        if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
        return ik_format_get_string(buf);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    const char *content = NULL;
    size_t content_len = 0;

    if (yyjson_is_str(root)) {
        // String result - use directly
        content = yyjson_get_str(root);
        content_len = yyjson_get_len(root);

        // Check for empty string
        if (content_len == 0) {
            yyjson_doc_free(doc);
            res = ik_format_append(buf, "(no output)");
            if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE
            return ik_format_get_string(buf);
        }
    } else if (yyjson_is_arr(root)) {
        // Array - join elements with ", "
        ik_format_buffer_t *arr_buf = ik_format_buffer_create(parent);
        size_t arr_size = yyjson_arr_size(root);

        for (size_t i = 0; i < arr_size; i++) {
            yyjson_val *elem = yyjson_arr_get(root, i);
            if (i > 0) {
                ik_format_append(arr_buf, ", ");
            }
            if (yyjson_is_str(elem)) {
                ik_format_append(arr_buf, yyjson_get_str(elem));
            } else {
                const char *elem_str = yyjson_val_write(elem, 0, NULL);
                if (elem_str != NULL) {
                    ik_format_append(arr_buf, elem_str);
                    free((void *)elem_str);
                }
            }
        }

        content = ik_format_get_string(arr_buf);
        content_len = ik_format_get_length(arr_buf);
    } else {
        // Object or other - serialize to JSON
        const char *json_str = yyjson_val_write(root, 0, NULL);
        if (json_str != NULL) {
            content = talloc_strdup_(parent, json_str);
            content_len = strlen(content);
            free((void *)json_str);
        }
    }

    yyjson_doc_free(doc);

    // Truncate and append
    const char *truncated = truncate_content(parent, content, content_len);
    res = ik_format_append(buf, truncated);
    if (is_err(&res)) PANIC("formatting failed"); // LCOV_EXCL_BR_LINE

    return ik_format_get_string(buf);
}
```

### Verify Phase
```bash
make check  # All tests pass
make lint   # No complexity issues
```

## Notes

- The `←` character is UTF-8 (3 bytes: 0xE2 0x86 0x90)
- Truncation happens BEFORE appending to format buffer
- Array results are formatted as comma-separated values
- String results are used directly (common case for file_read, bash output)
- Object results fall back to JSON serialization

## Success Criteria

- All new tests pass
- Existing tests updated to expect new format
- `make check` passes
- `make lint` passes
- Output format: `← tool_name: <truncated content>...`
- Truncation at 3 lines or 400 chars, whichever first
