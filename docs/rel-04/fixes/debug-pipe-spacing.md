# Fix: Debug Pipe Output Spacing

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/debug_pipe.h` - Debug pipe interface
- `src/debug_pipe.c` - Debug pipe implementation, especially `ik_debug_mgr_handle_ready()`
- `src/scrollback.h` - Scrollback interface

### Test patterns:
- `tests/unit/debug_pipe_test.c` - Existing debug pipe tests

## Situation

Debug pipe output (from `/debug on`) is appended to scrollback without consistent spacing. Each line should have a blank line after it to match the spacing convention used for all other events.

Current behavior:
```
[openai] >> Authorization: [REDACTED]
[openai] >> Content-Type: application/json
[openai] << {"choices":[...]}
```

Desired behavior:
```
[openai] >> Authorization: [REDACTED]

[openai] >> Content-Type: application/json

[openai] << {"choices":[...]}

```

## Task

Update `ik_debug_mgr_handle_ready()` in `src/debug_pipe.c` to append a blank line after each debug line.

## TDD Cycle

### Red Phase
Update tests in `tests/unit/debug_pipe_test.c`:

```c
START_TEST(test_debug_mgr_handle_ready_adds_blank_lines)
{
    void *ctx = talloc_new(NULL);

    // Create manager
    res_t res = ik_debug_mgr_create(ctx);
    ck_assert(is_ok(&res));
    ik_debug_pipe_manager_t *mgr = res.ok;

    // Add pipe with prefix
    res = ik_debug_mgr_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    // Create scrollback
    ik_scrollback_t *sb = NULL;
    res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Write test lines to pipe
    fprintf(pipe->write_end, "line1\n");
    fprintf(pipe->write_end, "line2\n");
    fflush(pipe->write_end);

    // Set up fd_set
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    // Handle ready pipes with debug enabled
    res = ik_debug_mgr_handle_ready(mgr, &read_fds, sb, true);
    ck_assert(is_ok(&res));

    // Should have 4 lines: line1, blank, line2, blank
    ck_assert_uint_eq(ik_scrollback_line_count(sb), 4);
    ck_assert_str_eq(ik_scrollback_get_line(sb, 0), "[test] line1");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 1), "");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 2), "[test] line2");
    ck_assert_str_eq(ik_scrollback_get_line(sb, 3), "");

    talloc_free(ctx);
}
END_TEST

START_TEST(test_debug_mgr_handle_ready_disabled_no_output)
{
    void *ctx = talloc_new(NULL);

    // Create manager
    res_t res = ik_debug_mgr_create(ctx);
    ck_assert(is_ok(&res));
    ik_debug_pipe_manager_t *mgr = res.ok;

    // Add pipe
    res = ik_debug_mgr_add_pipe(mgr, "[test]");
    ck_assert(is_ok(&res));
    ik_debug_pipe_t *pipe = res.ok;

    // Create scrollback
    ik_scrollback_t *sb = NULL;
    res = ik_scrollback_create(ctx, 80, &sb);
    ck_assert(is_ok(&res));

    // Write test line
    fprintf(pipe->write_end, "should not appear\n");
    fflush(pipe->write_end);

    // Set up fd_set
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(pipe->read_fd, &read_fds);

    // Handle ready pipes with debug DISABLED
    res = ik_debug_mgr_handle_ready(mgr, &read_fds, sb, false);
    ck_assert(is_ok(&res));

    // Should have 0 lines (debug disabled)
    ck_assert_uint_eq(ik_scrollback_line_count(sb), 0);

    talloc_free(ctx);
}
END_TEST
```

### Green Phase
Update `ik_debug_mgr_handle_ready()` in `src/debug_pipe.c`:

```c
res_t ik_debug_mgr_handle_ready(ik_debug_pipe_manager_t *mgr, fd_set *read_fds,
                                ik_scrollback_t *scrollback, bool debug_enabled)
{
    assert(mgr != NULL);       // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    // scrollback can be NULL when debug_enabled is false

    // Iterate over all pipes
    for (size_t i = 0; i < mgr->count; i++) {
        ik_debug_pipe_t *pipe = mgr->pipes[i];

        // Check if this pipe is ready
        if (!FD_ISSET(pipe->read_fd, read_fds)) {
            continue;
        }

        // Read from pipe
        char **lines = NULL;
        size_t count = 0;
        res_t res = ik_debug_pipe_read(pipe, &lines, &count);
        if (is_err(&res)) {
            return res;
        }

        // If debug enabled, append lines to scrollback
        if (debug_enabled && count > 0) {
            assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
            for (size_t j = 0; j < count; j++) {
                // Append debug line
                res_t append_res = ik_scrollback_append_line(scrollback, lines[j], strlen(lines[j]));
                if (is_err(&append_res)) {  // LCOV_EXCL_BR_LINE
                    talloc_free(lines);  // LCOV_EXCL_LINE
                    return append_res;  // LCOV_EXCL_LINE
                }

                // Append blank line for consistent spacing
                append_res = ik_scrollback_append_line(scrollback, "", 0);
                if (is_err(&append_res)) {  // LCOV_EXCL_BR_LINE
                    talloc_free(lines);  // LCOV_EXCL_LINE
                    return append_res;  // LCOV_EXCL_LINE
                }
            }
        }

        // Free lines array (whether or not we used it)
        if (lines != NULL) {
            talloc_free(lines);
        }
    }

    return OK(NULL);
}
```

### Verify Phase
```bash
make check  # All tests pass
make lint   # No complexity issues
```

## Dependencies

None - this fix is independent of the other spacing fixes.

## Notes

- Debug pipe lines already have their prefix applied in `ik_debug_pipe_read()`
- The blank line is only added when `debug_enabled` is true
- When debug is disabled, lines are still read (to prevent blocking) but discarded

## Success Criteria

- Debug output has blank line after each line
- No output when debug is disabled
- `make check` passes
- `make lint` passes
