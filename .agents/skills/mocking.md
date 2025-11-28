# Mocking

## Description
MOCKABLE wrapper patterns for testing external dependencies.

## Philosophy

**Mock external dependencies, never our own code.** External means: libraries (talloc, yyjson, curl), system calls (open, read, write, stat), and vendor inline functions.

## The MOCKABLE Pattern

`wrapper.h` provides zero-overhead mocking via weak symbols:

```c
// wrapper.h
#ifdef NDEBUG
    #define MOCKABLE static inline  // Release: zero overhead
#else
    #define MOCKABLE __attribute__((weak))  // Debug: overridable
#endif

MOCKABLE int posix_write_(int fd, const void *buf, size_t count);
```

## Using Mocks in Tests

Override the weak symbol in your test file:

```c
// test_file.c
#include "wrapper.h"

// Override - this replaces the weak symbol
int posix_write_(int fd, const void *buf, size_t count) {
    return -1;  // Simulate write failure
}

void test_write_error_handling(void) {
    // Now any code calling posix_write_() gets our mock
    res_t r = save_file("test.txt", "content");
    assert(IS_ERR(r));
}
```

## Available Wrappers

Check `src/wrapper.h` for the full list. Common ones:

| Wrapper | Mocks |
|---------|-------|
| `posix_open_()` | open() |
| `posix_read_()` | read() |
| `posix_write_()` | write() |
| `posix_close_()` | close() |
| `posix_stat_()` | stat() |
| `talloc_size_()` | talloc_size() |
| `yyjson_read_()` | yyjson_read() |

## Adding New Wrappers

When you need to mock a new external dependency:

1. Add declaration to `wrapper.h`:
```c
MOCKABLE return_type function_name_(args);
```

2. Add implementation to `wrapper.c`:
```c
return_type function_name_(args) {
    return original_function(args);
}
```

3. Replace direct calls in production code with the wrapper

4. Override in tests to inject failures

## Wrapping Vendor Inline Functions

Problem: Inline functions like `yyjson_doc_get_root()` expand at every call site, creating untestable branches.

Solution: Wrap once, use wrapper everywhere:

```c
// sse_parser.h
yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc);

// sse_parser.c
yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc) {
    return yyjson_doc_get_root(doc);  // Inline expands here only
}

// Test both branches once
void test_wrapper_null(void) {
    assert(yyjson_doc_get_root_wrapper(NULL) == NULL);
}
void test_wrapper_valid(void) {
    yyjson_doc *doc = create_test_doc();
    assert(yyjson_doc_get_root_wrapper(doc) != NULL);
}
```

## What NOT to Mock

- Our own code (refactor instead)
- Pure functions (test directly)
- Simple data structures

## References

- `src/wrapper.h` - MOCKABLE declarations
- `src/wrapper.c` - Wrapper implementations
- `src/openai/sse_parser.h` - Inline wrapper example
