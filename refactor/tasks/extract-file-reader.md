# Task: Extract File Reading Utility

## Target

Refactor Issue #8: Extract shared file reading utility from duplicated code in 3 files (~120 lines total)

## Context

The following files contain nearly identical file-reading logic:

1. **`src/tool_file_read.c`** (lines 63-98)
2. **`src/db/migration.c`** (lines 69-109)
3. **`src/history.c`** (lines 315-349)

The duplicated pattern:
```c
// 1. Seek to end to get file size
if (fseek_(f, 0, SEEK_END) != 0) {
    fclose_(f);
    return ERR(...);
}

// 2. Get file size
long size = ftell_(f);
if (size < 0) {
    fclose_(f);
    return ERR(...);
}

// 3. Seek back to start
if (fseek_(f, 0, SEEK_SET) != 0) {
    fclose_(f);
    return ERR(...);
}

// 4. Allocate buffer
char *buffer = talloc_array(ctx, char, (size_t)(size + 1));
if (buffer == NULL) {
    fclose_(f);
    PANIC("Out of memory");
}

// 5. Read entire file
size_t bytes_read = fread_(buffer, 1, (size_t)size, f);
fclose_(f);

if (bytes_read != (size_t)size) {
    return ERR(...);
}

// 6. Null-terminate
buffer[size] = '\0';
```

This should be extracted into a reusable utility function.

## Pre-read

### Skills
- default
- database
- errors
- git
- log
- makefile
- naming
- quality
- scm
- source-code
- style
- tdd
- align

### Documentation
- docs/README.md
- project/error_handling.md
- project/memory.md

### Source Files (Duplication Locations)
- src/tool_file_read.c (lines 63-98) - tool implementation
- src/db/migration.c (lines 69-109) - migration file reading
- src/history.c (lines 315-349) - history JSON loading

### Related Source Files
- src/wrapper.h (fseek_, ftell_, fread_, fclose_ wrappers)
- src/wrapper.c (wrapper implementations)
- src/error.h (res_t, ERR macro)

### Existing Test Files
- tests/unit/test_tool_file_read.c
- tests/unit/test_migration.c
- tests/unit/test_history.c

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. All tests pass (`make check`)
3. The three source files contain the duplicated pattern

## Task

Create a new utility function `ik_file_read_all()` that:
1. Opens a file by path
2. Reads entire contents into a talloc-allocated buffer
3. Returns `res_t` with the buffer on success or error on failure
4. Handles all error cases (open, seek, tell, read, memory)

Then refactor the three files to use this utility.

## API Design

### Header: `src/file_utils.h`

```c
#ifndef IK_FILE_UTILS_H
#define IK_FILE_UTILS_H

#include "error.h"
#include <talloc.h>

// Read entire file into null-terminated buffer
// Returns: OK(buffer) where buffer is talloc-allocated on ctx
//          ERR on any failure (file not found, read error, etc.)
// Note: Caller owns returned buffer (freed with ctx)
res_t ik_file_read_all(TALLOC_CTX *ctx, const char *path, char **out_content, size_t *out_size);

#endif
```

### Implementation: `src/file_utils.c`

Follow patterns from existing wrapper usage. Use `fopen_`, `fseek_`, `ftell_`, `fread_`, `fclose_` wrappers for testability.

## TDD Cycle

### Red Phase

1. Create `tests/unit/test_file_utils.c` with test cases:
   - `test_file_read_all_success` - reads existing file correctly
   - `test_file_read_all_file_not_found` - returns error for missing file
   - `test_file_read_all_empty_file` - handles empty files
   - `test_file_read_all_null_ctx` - asserts on NULL context
   - `test_file_read_all_null_path` - asserts on NULL path
   - `test_file_read_all_null_out` - asserts on NULL output pointer

2. Create stub `src/file_utils.h` and `src/file_utils.c` with function that returns `ERR()`.

3. Update Makefile to compile new files.

4. Run `make check` - tests should fail with assertion failures.

### Green Phase

1. Implement `ik_file_read_all()`:
   - Open file with `fopen_(path, "rb")`
   - Seek to end, get size, seek to start
   - Allocate buffer with `talloc_array()`
   - Read contents with `fread_()`
   - Null-terminate buffer
   - Set output parameters
   - Return `OK(NULL)` on success

2. Run `make check` - all new tests should pass.

### Refactor Phase

1. Update `src/tool_file_read.c`:
   - Include `file_utils.h`
   - Replace inline file reading with `ik_file_read_all()` call
   - Remove duplicated code

2. Update `src/db/migration.c`:
   - Include `file_utils.h`
   - Replace inline file reading with `ik_file_read_all()` call
   - Remove duplicated code

3. Update `src/history.c`:
   - Include `file_utils.h`
   - Replace inline file reading with `ik_file_read_all()` call
   - Remove duplicated code

4. Run `make check` - all existing tests should still pass.

5. Run `make lint` - verify no new warnings.

6. Run `make coverage` - verify coverage maintained.

## Post-conditions

1. New `src/file_utils.h` and `src/file_utils.c` exist
2. New `tests/unit/test_file_utils.c` exists with full coverage
3. Three source files refactored to use new utility
4. All tests pass (`make check`)
5. Lint passes (`make lint`)
6. Coverage maintained at 100% (`make coverage`)
7. Working tree is clean (changes committed)

## Commit Strategy

### Commit 1: Add file_utils with tests
```
feat: add ik_file_read_all utility function

- New file_utils.h/c with ik_file_read_all()
- Reads entire file into talloc-allocated buffer
- Full test coverage in test_file_utils.c
```

### Commit 2: Refactor tool_file_read.c
```
refactor: use ik_file_read_all in tool_file_read.c

- Replace inline file reading with utility function
- Reduces code duplication
```

### Commit 3: Refactor migration.c
```
refactor: use ik_file_read_all in db/migration.c

- Replace inline file reading with utility function
- Reduces code duplication
```

### Commit 4: Refactor history.c
```
refactor: use ik_file_read_all in history.c

- Replace inline file reading with utility function
- Reduces code duplication
```

## Risk Assessment

**Risk: Low-Medium**
- New utility is straightforward
- Existing tests verify behavior preserved
- Incremental commits allow easy rollback

## Estimated Complexity

**Medium** - New file creation, test coverage, and 3 file refactors
