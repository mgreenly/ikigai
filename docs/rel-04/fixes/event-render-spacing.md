# Fix: Event Render Consistent Spacing

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/event_render.h` - Event render interface
- `src/event_render.c` - Event render implementation
- `src/scrollback.h` - Scrollback trim function (from prior fix)
- `src/scrollback.c` - Scrollback implementation
- `src/repl_callbacks.c` - Streaming callbacks
- `src/repl_tool.c` - Tool call/result rendering

### Test patterns:
- `tests/unit/event_render_test.c` - Existing event render tests
- `tests/unit/scrollback_test.c` - Scrollback tests

## Situation

Currently, messages are appended to scrollback without consistent spacing. The goal is:

**Every event rendered to scrollback should have exactly one blank line after it.**

This includes:
- User messages
- Assistant responses (after streaming completes)
- System messages
- Mark events
- Tool calls
- Tool results

Content should be trimmed of trailing whitespace before appending, then a blank line added.

## Task

### Part 1: Modify `ik_event_render()` to add spacing

Update `ik_event_render()` to:
1. Trim trailing whitespace from content using `ik_scrollback_trim_trailing()`
2. Append trimmed content
3. Append a blank line after content

### Part 2: Handle streaming completion

The assistant response is rendered line-by-line during streaming. The blank line should only appear AFTER streaming completes.

In `ik_repl_http_completion_callback()`:
- After flushing any remaining line buffer
- Add a blank line to scrollback

### Part 3: Tool call display formatting

In `repl_tool.c`, the tool call display currently uses:
```c
ik_event_render(repl->scrollback, "tool_call", summary, "{}");
```

Change to use the arrow format:
```c
const char *formatted_call = ik_format_tool_call(repl, tc);
ik_event_render(repl->scrollback, "tool_call", formatted_call, "{}");
```

## TDD Cycle

### Red Phase
Add/update tests in `tests/unit/event_render_test.c`:

```c
START_TEST(test_event_render_user_adds_blank_line)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    res = ik_event_render(sb, "user", "hello", "{}");
    ck_assert(is_ok(&res));

    // Should have 2 lines: "hello" and ""
    ck_assert_uint_eq(ik_scrollback_line_count(sb), 2);
    ck_assert_str_eq(ik_scrollback_get_line(sb, 0), "hello");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 1), "");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_event_render_trims_trailing_newlines)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    res = ik_event_render(sb, "user", "hello\n\n\n", "{}");
    ck_assert(is_ok(&res));

    // Should have 2 lines: "hello" (trimmed) and ""
    ck_assert_uint_eq(ik_scrollback_line_count(sb), 2);
    ck_assert_str_eq(ik_scrollback_get_line(sb, 0), "hello");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 1), "");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_event_render_mark_adds_blank_line)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    res = ik_event_render(sb, "mark", NULL, "{\"label\": \"checkpoint\"}");
    ck_assert(is_ok(&res));

    // Should have 2 lines: "/mark checkpoint" and ""
    ck_assert_uint_eq(ik_scrollback_line_count(sb), 2);
    ck_assert_str_eq(ik_scrollback_get_line(sb, 0), "/mark checkpoint");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 1), "");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_event_render_tool_call_adds_blank_line)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    res = ik_event_render(sb, "tool_call", "→ glob: pattern=\"*.c\"", "{}");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(ik_scrollback_line_count(sb), 2);
    ck_assert_str_eq(ik_scrollback_get_line(sb, 0), "→ glob: pattern=\"*.c\"");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 1), "");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_event_render_empty_content_no_double_blank)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Empty content should not add anything
    res = ik_event_render(sb, "system", "", "{}");
    ck_assert(is_ok(&res));

    ck_assert_uint_eq(ik_scrollback_line_count(sb), 0);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_event_render_multiline_content)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *sb = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    res = ik_event_render(sb, "user", "line1\nline2\nline3", "{}");
    ck_assert(is_ok(&res));

    // Content is one logical line (with embedded newlines) + blank line
    ck_assert_uint_eq(ik_scrollback_line_count(sb), 2);
    ck_assert_str_eq(ik_scrollback_get_line(sb, 0), "line1\nline2\nline3");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 1), "");

    talloc_free(ctx);
}
END_TEST
```

### Green Phase

**Update `src/event_render.c`:**

```c
// Helper: render content event (user, assistant, system, tool_call, tool_result)
static res_t render_content_event(ik_scrollback_t *scrollback, const char *content)
{
    // Content can be NULL (e.g., empty system message)
    if (content == NULL || content[0] == '\0') {
        return OK(NULL);
    }

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Trim trailing whitespace
    char *trimmed = ik_scrollback_trim_trailing(tmp, content, strlen(content));

    // Skip if empty after trimming
    if (trimmed[0] == '\0') {
        talloc_free(tmp);
        return OK(NULL);
    }

    // Append content
    res_t result = ik_scrollback_append_line_(scrollback, trimmed, strlen(trimmed));
    if (is_err(&result)) {
        talloc_free(tmp);
        return result;
    }

    // Append blank line for spacing
    result = ik_scrollback_append_line_(scrollback, "", 0);

    talloc_free(tmp);
    return result;
}

// Update render_mark_event to add blank line:
static res_t render_mark_event(ik_scrollback_t *scrollback, const char *data_json)
{
    // ... existing extract_label_from_json code ...

    // Append mark text
    res_t result = ik_scrollback_append_line_(scrollback, text, strlen(text));
    if (is_err(&result)) {
        talloc_free(tmp);
        return result;
    }

    // Append blank line for spacing
    result = ik_scrollback_append_line_(scrollback, "", 0);

    talloc_free(tmp);
    return result;
}
```

**Update `src/event_render.c` to handle tool_call and tool_result kinds:**

```c
res_t ik_event_render(ik_scrollback_t *scrollback,
                      const char *kind,
                      const char *content,
                      const char *data_json)
{
    // ... existing validation ...

    // Handle each event kind
    if (strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "system") == 0 ||
        strcmp(kind, "tool_call") == 0 ||
        strcmp(kind, "tool_result") == 0) {
        return render_content_event(scrollback, content);
    }

    // ... rest of function ...
}
```

**Update `src/repl_callbacks.c` - add blank line after streaming:**

```c
res_t ik_repl_http_completion_callback(const ik_http_completion_t *completion, void *ctx)
{
    // ... existing code ...

    // Flush any remaining buffered line content
    if (repl->streaming_line_buffer != NULL) {
        size_t buffer_len = strlen(repl->streaming_line_buffer);
        ik_scrollback_append_line(repl->scrollback, repl->streaming_line_buffer, buffer_len);
        talloc_free(repl->streaming_line_buffer);
        repl->streaming_line_buffer = NULL;
    }

    // Add blank line after assistant response (spacing)
    if (completion->type == IK_HTTP_SUCCESS) {
        ik_scrollback_append_line(repl->scrollback, "", 0);
    }

    // ... rest of function ...
}
```

**Update `src/repl_tool.c` - use formatted tool call:**

```c
// In ik_repl_execute_pending_tool and ik_repl_complete_tool_execution:

// Display tool call with arrow format
const char *formatted_call = ik_format_tool_call(repl, tc);
ik_event_render(repl->scrollback, "tool_call", formatted_call, "{}");

// Tool result already uses ik_format_tool_result, keep that
const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
ik_event_render(repl->scrollback, "tool_result", formatted_result, "{}");
```

### Verify Phase
```bash
make check  # All tests pass
make lint   # No complexity issues
```

## Dependencies

- `scrollback-trim-trailing.md` - Must be completed first (provides `ik_scrollback_trim_trailing()`)
- `tool-call-arrow-format.md` - Must be completed first (provides new `ik_format_tool_call()`)
- `tool-result-truncate.md` - Must be completed first (provides new `ik_format_tool_result()`)

## Success Criteria

- All events render with exactly one blank line after
- Trailing whitespace is trimmed from content
- Empty content does not produce output
- Streaming responses get blank line after completion
- `make check` passes
- `make lint` passes
