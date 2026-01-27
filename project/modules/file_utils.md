# file_utils

Reads entire files into memory.

## Files

- [src/file_utils.h](../../src/file_utils.h)
- [src/file_utils.c](../../src/file_utils.c)

## Usage

Read a file into a talloc-allocated buffer:

```c
char *content = NULL;
size_t size = 0;
res_t r = ik_file_read_all(ctx, "/path/to/file.txt", &content, &size);
if (is_err(&r)) {
    // file not found or read error
}
// content is null-terminated, size is length without null
// caller owns content (freed with ctx)
```

The size parameter is optional:

```c
char *content = NULL;
ik_file_read_all(ctx, path, &content, NULL);
```

## API

```c
res_t ik_file_read_all(TALLOC_CTX *ctx, const char *path, char **out_content, size_t *out_size);
```

## Dependencies

- [error](error.md) - result types
