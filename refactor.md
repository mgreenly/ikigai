# Wrapper Function Naming Refactor

## Status
**Planned** - Not yet implemented

## Motivation

The current wrapper functions use the `ik_` prefix (e.g., `ik_talloc_zero_wrapper`, `ik_yyjson_read_file_wrapper`, `ik_open_wrapper`), which creates confusion about the nature of these symbols.

### Problems with Current Naming

1. **Implies Native Ownership**: The `ik_` prefix suggests these are native ikigai APIs, part of the core codebase
2. **Obscures Purpose**: These are actually thin wrappers around external libraries and POSIX calls
3. **Inconsistent Signal**: The `ik_` namespace is meant for ikigai-authored functionality, not external library seams

### Purpose of Wrappers

These wrappers exist solely as **link seams for testing**:
- In release builds (`NDEBUG`): Compiled as `static inline` for zero overhead
- In debug/test builds: Weak symbols that tests can override to inject failures
- They are NOT ikigai APIs; they're testability shims around external code

## Proposed Solution

Remove the `ik_` prefix and adopt conventions that clearly signal these are external wrappers:

### Library Wrappers (talloc, yyjson)
Use **trailing underscore** (`_`) to indicate "this is the real function, with a testability seam":
- `talloc_zero_()` instead of `ik_talloc_zero_wrapper()`
- `talloc_strdup_()` instead of `ik_talloc_strdup_wrapper()`
- `yyjson_read_file_()` instead of `ik_yyjson_read_file_wrapper()`

**Rationale**: The trailing `_` is subtle but distinct, clearly saying "this is the library function, slightly modified". It doesn't claim ikigai ownership.

### POSIX System Call Wrappers
Use **`posix_` prefix + trailing underscore** to avoid namespace ambiguity:
- `posix_open_()` instead of `ik_open_wrapper()`
- `posix_write_()` instead of `ik_write_wrapper()`
- `posix_tcgetattr_()` instead of `ik_tcgetattr_wrapper()`

**Rationale**: POSIX call names like `open` and `write` are too generic without context. The `posix_` prefix makes it clear these are system call wrappers, while the trailing `_` maintains the "seam" signal.

## Scope

### Functions Affected (21 total)

**talloc wrappers (5)**:
- `ik_talloc_zero_wrapper` → `talloc_zero_`
- `ik_talloc_strdup_wrapper` → `talloc_strdup_`
- `ik_talloc_array_wrapper` → `talloc_array_`
- `ik_talloc_realloc_wrapper` → `talloc_realloc_`
- `ik_talloc_asprintf_wrapper` → `talloc_asprintf_`

**yyjson wrappers (6)**:
- `ik_yyjson_read_file_wrapper` → `yyjson_read_file_`
- `ik_yyjson_mut_write_file_wrapper` → `yyjson_mut_write_file_`
- `ik_yyjson_doc_get_root_wrapper` → `yyjson_doc_get_root_`
- `ik_yyjson_obj_get_wrapper` → `yyjson_obj_get_`
- `ik_yyjson_get_sint_wrapper` → `yyjson_get_sint_`
- `ik_yyjson_get_str_wrapper` → `yyjson_get_str_`

**POSIX wrappers (10)**:
- `ik_open_wrapper` → `posix_open_`
- `ik_close_wrapper` → `posix_close_`
- `ik_stat_wrapper` → `posix_stat_`
- `ik_mkdir_wrapper` → `posix_mkdir_`
- `ik_tcgetattr_wrapper` → `posix_tcgetattr_`
- `ik_tcsetattr_wrapper` → `posix_tcsetattr_`
- `ik_tcflush_wrapper` → `posix_tcflush_`
- `ik_ioctl_wrapper` → `posix_ioctl_`
- `ik_write_wrapper` → `posix_write_`
- `ik_read_wrapper` → `posix_read_`

### Impact
- **266 total usages** across **33 files**
- Affects production code and tests
- No functional changes, purely renaming

## Implementation Plan

### Phase 1: Update Wrapper Definitions
1. Update `src/wrapper.h`:
   - Rename all function declarations (both `#ifdef NDEBUG` and `#else` blocks)
   - Update inline function definitions in NDEBUG block
2. Update `src/wrapper.c`:
   - Rename all function implementations in debug/test block
3. Verify compilation: `make clean && make build/ikigai`
   - Expected: Build fails with undefined references (call sites not updated yet)

### Phase 2: Update Call Sites (33 files)

**Source files (13)**:
- `src/array.c` (2 usages)
- `src/config.c` (14 usages)
- `src/error.c` (1 usage)
- `src/format.c` (2 usages)
- `src/input.c` (1 usage)
- `src/input_buffer.c` (1 usage)
- `src/input_buffer_cursor.c` (1 usage)
- `src/panic.h` (1 usage)
- `src/render.c` (9 usages)
- `src/repl.c` (2 usages)
- `src/repl_actions.c` (1 usage)
- `src/scrollback.c` (9 usages)
- `src/terminal.c` (21 usages)

**Test utility files (2)**:
- `tests/test_utils.h` (5 usages)
- `tests/test_utils.c` (5 usages)

**Unit tests (11)**:
- `tests/unit/config/filesystem_test.c` (8 usages)
- `tests/unit/render/input_buffer_test.c` (2 usages)
- `tests/unit/render/render_cursor_visibility_test.c` (2 usages)
- `tests/unit/render/render_scrollback_test.c` (2 usages)
- `tests/unit/repl/repl_autoscroll_test.c` (16 usages)
- `tests/unit/repl/repl_combined_render_test.c` (2 usages)
- `tests/unit/repl/repl_init_test.c` (20 usages)
- `tests/unit/repl/repl_render_test.c` (2 usages)
- `tests/unit/repl/repl_resize_test.c` (16 usages)
- `tests/unit/repl/repl_run_test_common.c` (2 usages)
- `tests/unit/repl/repl_run_test_common.h` (2 usages)
- `tests/unit/repl/repl_scrollback_test.c` (16 usages)
- `tests/unit/repl/repl_slash_command_test.c` (7 usages)
- `tests/unit/terminal/terminal_test.c` (14 usages)

**Integration tests (2)**:
- `tests/integration/config_integration_test.c` (3 usages)
- `tests/integration/repl_test.c` (14 usages)

**Strategy**: Use automated find-and-replace for each wrapper function name across all files.

### Phase 3: Quality Checks
1. Format code: `make fmt`
2. Build all targets: `make clean && make`
3. Run all tests: `make check`
4. Verify linting: `make lint`
5. Verify coverage: `make coverage` (must maintain 100.0%)
6. Run dynamic checks: `make check-dynamic`

### Phase 4: Documentation Updates

**Required documentation changes**:

1. **`docs/decisions/link-seams-mocking.md`** (ADR) - Update code examples:
   - Change `ik_talloc_zero_wrapper` to `talloc_zero_` in all examples
   - Update testing example showing override
   - Update build verification commands (nm output)
   - Update wrapper.c/wrapper.h references

2. **`docs/naming.md`** - Add new section documenting wrapper naming:
   - Add "External Library Wrappers" section
   - Document trailing `_` convention for library wrappers
   - Document `posix_*_` convention for POSIX wrappers
   - Explain rationale (no `ik_` prefix for external code)

3. **Search all documentation** for references to old wrapper names:
   - `docs/README.md`
   - `docs/error_handling.md`
   - `docs/error_testing.md`
   - `docs/memory.md`
   - Any other docs mentioning wrappers, MOCKABLE, or link seams

## Success Criteria

### Code Changes
- [ ] All 21 wrapper functions renamed in `src/wrapper.h` and `src/wrapper.c`
- [ ] All 266 call sites updated across 33 files

### Quality Checks
- [ ] Code compiles without errors or warnings
- [ ] `make fmt` applied
- [ ] `make check` passes (all tests pass)
- [ ] `make lint` passes (all complexity checks pass)
- [ ] `make coverage` shows 100.0% coverage (lines, functions, branches)
- [ ] `make check-dynamic` passes (ASan, UBSan, TSan clean)

### Documentation
- [ ] `docs/decisions/link-seams-mocking.md` updated with new naming
- [ ] `docs/naming.md` updated with wrapper naming section
- [ ] All other docs checked for references to old wrapper names

## Rollback Plan

If issues are discovered post-refactor:
1. Git revert the refactor commit(s)
2. Re-run quality checks to verify rollback is clean
3. Investigate issues before re-attempting

## Proposed Documentation Addition

The following section should be added to `docs/naming.md` after the "Special Conventions" section:

```markdown
## External Library Wrappers

**IMPORTANT**: External library wrappers do NOT use the `ik_` prefix.

These wrappers exist solely as link seams for testing (see `docs/decisions/link-seams-mocking.md`). They are not ikigai-authored APIs, but thin testability shims around external code.

### Library Wrappers (talloc, yyjson)

Use **trailing underscore** (`_`):
```c
talloc_zero_(ctx, size)           // wraps talloc_zero_size()
talloc_strdup_(ctx, str)          // wraps talloc_strdup()
yyjson_read_file_(path, flg, ...)  // wraps yyjson_read_file()
```

**Rationale**: The trailing `_` signals "this is the library function, with a testability seam". It clearly indicates these are not ikigai APIs while remaining close to the original function names.

### POSIX System Call Wrappers

Use **`posix_` prefix + trailing underscore**:
```c
posix_open_(pathname, flags)      // wraps open()
posix_write_(fd, buf, count)      // wraps write()
posix_stat_(pathname, statbuf)    // wraps stat()
```

**Rationale**: Generic POSIX names like `open` and `write` need context. The `posix_` prefix clarifies these are system call wrappers, while the trailing `_` maintains the "seam" signal.

### Build Behavior

- **Release builds** (`NDEBUG`): Wrappers are `static inline` - zero overhead, no symbols in binary
- **Debug/test builds**: Wrappers are `weak` symbols that tests can override to inject failures

All wrappers are marked with the `MOCKABLE` macro which expands to the appropriate linkage.
```

## Example Before/After

### Before
```c
// Library wrapper with ik_ prefix
char *result = ik_talloc_strdup_wrapper(ctx, path);

// POSIX wrapper with ik_ prefix
if (ik_stat_wrapper(dir, &st) != 0) {
    if (ik_mkdir_wrapper(dir, 0755) != 0) {
        return ERR(ctx, IO, "Failed to create directory");
    }
}

// yyjson wrapper with ik_ prefix
yyjson_doc *doc = ik_yyjson_read_file_wrapper(path, 0, &alc, &err);
```

### After
```c
// Library wrapper with trailing underscore
char *result = talloc_strdup_(ctx, path);

// POSIX wrapper with posix_ prefix and trailing underscore
if (posix_stat_(dir, &st) != 0) {
    if (posix_mkdir_(dir, 0755) != 0) {
        return ERR(ctx, IO, "Failed to create directory");
    }
}

// yyjson wrapper with trailing underscore
yyjson_doc *doc = yyjson_read_file_(path, 0, &alc, &err);
```

## Notes

- This is a pure refactoring - no functional changes
- The MOCKABLE macro behavior remains unchanged
- Zero-overhead in release builds is preserved
- Test mockability is preserved
- Consider doing this refactor in a single atomic commit for easy rollback if needed
