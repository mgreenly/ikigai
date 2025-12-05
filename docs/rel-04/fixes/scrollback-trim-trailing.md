# Fix: Scrollback Trim Trailing Whitespace

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `tdd.md` - Test-driven development approach
- `style.md` - Code style conventions

## Files to Explore

### Source files:
- `src/scrollback.h` - Scrollback buffer interface
- `src/scrollback.c` - Scrollback buffer implementation

### Test patterns:
- `tests/unit/scrollback_test.c` - Existing scrollback tests

## Situation

Content appended to the scrollback buffer may have inconsistent trailing whitespace (0, 1, 2, or more trailing newlines). To ensure consistent spacing, we need a utility function to normalize trailing whitespace before content is appended.

This is the foundation for consistent "single blank line after every event" spacing.

## Task

Add a utility function `ik_scrollback_trim_trailing()` that removes trailing whitespace (spaces, tabs, newlines) from a string.

### Function Signature

```c
// Trim trailing whitespace from string
//
// Returns a new string with trailing whitespace removed.
// Original string is not modified.
//
// @param parent Talloc parent context
// @param text Input string (NULL returns empty string)
// @param length Length of input string
// @return New string with trailing whitespace removed (owned by parent)
char *ik_scrollback_trim_trailing(void *parent, const char *text, size_t length);
```

### Implementation Details

1. If `text` is NULL or `length` is 0, return `talloc_strdup(parent, "")`
2. Scan backwards from end to find last non-whitespace character
3. Whitespace characters: space (0x20), tab (0x09), newline (0x0A), carriage return (0x0D)
4. Return `talloc_strndup(parent, text, trimmed_length)`

## TDD Cycle

### Red Phase
Write tests in `tests/unit/scrollback_test.c`:

```c
START_TEST(test_trim_trailing_null_returns_empty)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, NULL, 0);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_empty_returns_empty)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "", 0);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_no_whitespace)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello", 5);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_single_newline)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello\n", 6);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_multiple_newlines)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello\n\n\n", 8);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_mixed_whitespace)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello \t\n\r\n", 10);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_preserves_internal_whitespace)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello\nworld\n", 12);
    ck_assert_str_eq(result, "hello\nworld");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_trim_trailing_all_whitespace)
{
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "\n\n\n", 3);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
}
END_TEST
```

Add tests to suite and verify they fail (stub returns empty string).

### Green Phase
Implement `ik_scrollback_trim_trailing()` in `src/scrollback.c`:

```c
char *ik_scrollback_trim_trailing(void *parent, const char *text, size_t length)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    if (text == NULL || length == 0) {
        char *result = talloc_strdup_(parent, "");
        if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return result;
    }

    // Find last non-whitespace character
    size_t end = length;
    while (end > 0) {
        char c = text[end - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        end--;
    }

    char *result = talloc_strndup_(parent, text, end);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return result;
}
```

Add declaration to `src/scrollback.h`.

### Verify Phase
```bash
make check  # All tests pass
make lint   # No complexity issues
```

## Success Criteria

- All 8 new tests pass
- `make check` passes
- `make lint` passes
- Function handles edge cases: NULL, empty, no trailing whitespace, all whitespace
