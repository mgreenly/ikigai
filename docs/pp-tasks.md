# Pretty-Print (PP) Functionality - Implementation Tasks

[← Back to Main Documentation](README.md)

**Goal**: Implement debug pretty-printing functionality for internal C data structures and JSON/YAML content display.

**Status**: Task 1 COMPLETE - Format Buffer Module with 100% coverage (inserted before Phase 3 - Scrollback Buffer)

**Priority**: High - enables debugging and inspection before full scrollback implementation

## Overview

The application needs extensive ability to display:
1. **Internal C data structures** - for debugging (memory layout, pointers, sizes, etc.)
2. **JSON content** - from config files and API conversations, displayable as YAML

This work is split into two parallel concerns:

### PP for C Structures
- Functions named `ik_pp_<type>()` for each data structure
- Shows internal state: pointers, sizes, capacities, flags
- Used from REPL `/pp` commands for debugging
- Example: `ik_pp_block()`, `ik_pp_memory()`, `ik_pp_value()`

### PP for JSON Content
- Pretty-print JSON data (from `duckdb_json` or parsed values)
- Optional: convert JSON to YAML for readability
- Display config files and API message content

## Rationale

**Why before scrollback?**
- Enables immediate debugging capability (even with stdout)
- Smaller, testable increments
- Informs scrollback requirements (line-by-line vs bulk append)
- Can use PP functionality immediately for development
- Once scrollback exists, migration is trivial (one line change)

**Threading awareness:**
- v1.0 is single-threaded, but post-v1.0 will be heavily multi-threaded
- Design with thread-safety in mind:
  - Use `const` pointers in `ik_pp_*` signatures (read-only inspection)
  - Format buffers are thread-local (via talloc contexts)
  - Document threading assumptions for future work

## Implementation Tasks

### Task 1: Format Buffer Module ✅ COMPLETE

**Create**: `src/format.h` and `src/format.c` ✅

**Purpose**: Printf-style formatting into byte buffers (wrapper around `ik_byte_array_t`)

**Progress**:
- ✅ Created format.h with public API
- ✅ Created format.c with full implementation (146 lines)
- ✅ Added to Makefile (CLIENT_SOURCES and MODULE_SOURCES)
- ✅ Split tests into format_basic_test.c and format_oom_test.c (22 total tests)
- ✅ Implemented all 6 functions with full error handling
- ✅ Design fix: Replaced talloc() with ik_talloc_zero_wrapper() for OOM testing
- ✅ All 22 tests passing (15 functional + 7 OOM scenarios)
- ✅ All quality checks passing (make check && make lint && make coverage)
- ✅ Replaced defensive vsnprintf error checks with FATAL + LCOV_EXCL_LINE (invariant violations)
- ✅ Fixed error.h:117 to check vsnprintf return value with FATAL
- ✅ Removed all invalid LCOV_EXCL blocks - achieved 100% coverage WITHOUT exclusions (except assertions)

**Coverage**: ✅ **100% COMPLETE WITHOUT INVALID EXCLUSIONS**
- **Lines**: 100% (72/72 lines)
- **Functions**: 100% (6/6 functions)
- **Branches**: 100% (28/28 branches)

**LCOV Exclusions** (all valid - only invariants and assertions):
- Lines 48, 61-62: FATAL on vsnprintf invariant violations (programmer-controlled format strings)
- Lines 16-17, 37-38, 85-86, 104-105, 122, 144: Assertion branches (LCOV_EXCL_BR_LINE)
- Line 149: Short-circuit branch in compound condition (LCOV_EXCL_BR_LINE)

**Test Files**:
- `tests/unit/format/format_basic_test.c` (362 lines) - Basic functionality tests (15 tests)
- `tests/unit/format/format_oom_test.c` (219 lines) - OOM injection tests (7 tests)

**Public API**:
```c
/**
 * Format buffer for building output strings.
 *
 * Thread-safety: Each thread should create its own buffer.
 * Buffers are NOT thread-safe for concurrent access.
 */
typedef struct ik_format_buffer_t {
    ik_byte_array_t *array;    // Underlying byte array
    void *parent;              // Talloc parent
} ik_format_buffer_t;

// Create format buffer
res_t ik_format_buffer_create(void *parent, ik_format_buffer_t **buf_out);

// Append formatted string (like sprintf)
res_t ik_format_appendf(ik_format_buffer_t *buf, const char *fmt, ...);

// Append raw string
res_t ik_format_append(ik_format_buffer_t *buf, const char *str);

// Append indent spaces
res_t ik_format_indent(ik_format_buffer_t *buf, int indent);

// Get final string (null-terminated)
const char *ik_format_get_string(ik_format_buffer_t *buf);

// Get length in bytes (excluding null terminator)
size_t ik_format_get_length(ik_format_buffer_t *buf);
```

**Implementation Notes**:
- Wraps `ik_byte_array_t` for text accumulation
- `ik_format_appendf()` uses `vsnprintf()` internally
- Null-terminates the buffer automatically
- All memory managed via talloc hierarchy
- No locks needed (thread-local usage pattern)

**Estimated size**: ~150-200 lines

### Task 2: Comprehensive Unit Tests for Format Module

**Test Coverage** (`tests/unit/format/format_test.c`):
- Buffer creation and cleanup
- `ik_format_append()`:
  - Empty string
  - Short string
  - Long string requiring multiple reallocations
- `ik_format_appendf()`:
  - Simple format strings
  - Multiple format specifiers
  - Large formatted output
  - Edge cases (empty format, no args)
- `ik_format_indent()`:
  - Zero indent
  - Small indent (< 10)
  - Large indent (> 100)
- `ik_format_get_string()`:
  - Empty buffer
  - Populated buffer
  - Null termination verification
- `ik_format_get_length()`:
  - Empty buffer (length 0)
  - Populated buffer
- OOM injection tests (via MOCKABLE wrappers)

**Coverage requirement**: 100% (lines, functions, branches)

**Estimated size**: ~300-400 lines

### Task 3: PP Functions for Core Data Structures

**Implement `ik_pp_*` functions** for each major data structure:

```c
// In block.h/block.c (when it exists)
void ik_pp_block(const Block* block, ik_format_buffer_t *buf, int indent);

// In memory.h/memory.c (when it exists)
void ik_pp_memory(const Memory* mem, ik_format_buffer_t *buf, int indent);

// In value.h/value.c (when it exists)
void ik_pp_value(const Value* val, ik_format_buffer_t *buf, int indent);

// Example for existing workspace structure
void ik_pp_workspace(const ik_workspace_t *ws, ik_format_buffer_t *buf, int indent);
```

**Function signature pattern**:
- First parameter: `const Type*` (read-only, thread-safe for inspection)
- Second parameter: `ik_format_buffer_t*` (output buffer)
- Third parameter: `int indent` (indentation level for nested structures)

**Implementation pattern**:
```c
void ik_pp_block(const Block* block, ik_format_buffer_t *buf, int indent) {
    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "Block @ %p:\n", (void*)block);

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  length: %zu\n", block->length);

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  capacity: %zu\n", block->capacity);

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  arena: %p\n", (void*)block->arena);

    // Recurse for nested structures
    if (block->value_count > 0) {
        ik_format_indent(buf, indent);
        ik_format_appendf(buf, "  values: %zu\n", block->value_count);
        for (size_t i = 0; i < block->value_count; i++) {
            ik_pp_value(&block->values[i], buf, indent + 4);
        }
    }
}
```

**Start with**: `ik_pp_workspace()` since workspace already exists

**Estimated size**: ~50-100 lines per data structure

### Task 4: Unit Tests for PP Functions

**Test Coverage** for each `ik_pp_*` function:
- Null/empty structures
- Populated structures with typical data
- Deeply nested structures (recursion depth)
- Large structures (many elements)
- Output format verification (string matching)
- Indent level correctness

**Estimated size**: ~200-300 lines per data structure

### Task 5: Temporary REPL Integration (stdout)

**Goal**: Add `/pp` command to REPL that dumps to stdout

**Modifications**:
- Add new input action: `IK_INPUT_SLASH_COMMAND` (or similar)
- Parse slash commands in workspace (detect `/pp` prefix)
- Implement command handler:
  ```c
  // In repl.c (or new commands.c module)
  res_t ik_repl_handle_pp_command(ik_repl_ctx_t *repl, const char *args) {
      ik_format_buffer_t *buf;
      ik_format_buffer_create(repl, &buf);

      // For now, just pretty-print the workspace itself
      ik_pp_workspace(repl->workspace, buf, 0);

      // Temporary: dump to stdout
      printf("%s", ik_format_get_string(buf));
      fflush(stdout);

      talloc_free(buf);
      return OK(repl);
  }
  ```

**Manual testing**:
- Type `/pp workspace` in REPL
- Verify output appears on stdout
- Test with various workspace states (empty, single line, multi-line, UTF-8)

**Note**: This is temporary scaffolding. Once scrollback exists (Phase 3), output will go to scrollback buffer instead.

**Estimated size**: ~100-150 lines

### Task 6: JSON Pretty-Print Utilities (Optional - defer if needed)

**Goal**: Pretty-print JSON content from `duckdb_json` or parsed values

**Two sub-tasks**:

**6a. JSON pretty-print**:
```c
// In json_display.h/json_display.c (new module)
void ik_pp_json(duckdb_json_value* root, ik_format_buffer_t *buf, int indent);
```

**6b. JSON to YAML conversion**:
```c
void ik_json_to_yaml(duckdb_json_value* root, ik_format_buffer_t *buf, int indent);
```

**Decision**: Defer this task if it's not immediately needed. Focus on C structure PP first.

**Estimated size**: ~200-300 lines (if implemented)

## Manual Verification

**Demo via client.c or test harness**:
- Create various data structures (workspace, future: blocks, memory, etc.)
- Call `ik_pp_*` functions with format buffer
- Verify output formatting:
  - Correct indentation
  - All fields displayed
  - Nested structures formatted properly
  - No memory leaks (talloc hierarchy)

**Verification Checklist**:
- [ ] Format buffer correctly accumulates text
- [ ] `ik_format_appendf()` handles various format strings
- [ ] Indentation works correctly for nested structures
- [ ] `ik_pp_*` functions show all relevant fields
- [ ] Null termination correct (no buffer overruns)
- [ ] Memory management correct (talloc hierarchy)
- [ ] `/pp` command works in REPL (dumps to stdout)
- [ ] No memory leaks (valgrind clean)

## What We Validate

- Format buffer module with printf-like functionality
- Pretty-print functions for C data structures
- Correct indentation and formatting for nested structures
- Memory management via talloc (no leaks)
- Basic REPL integration (temporary stdout output)
- Foundation for future scrollback integration

## What We Defer

- Scrollback buffer integration (Phase 3)
- JSON/YAML pretty-printing (unless needed immediately)
- Full slash command parsing (just `/pp` for now)
- Multi-threaded synchronization (post-v1.0)

## Future: Scrollback Integration (Phase 3)

Once scrollback is implemented, migration is trivial:

```c
// Before (Task 5 - stdout):
printf("%s", ik_format_get_string(buf));

// After (Phase 3 - scrollback):
ik_scrollback_append_line(repl->scrollback,
                          ik_format_get_string(buf),
                          ik_format_get_length(buf));
```

**Threading considerations (post-v1.0)**:
- Each thread creates its own format buffer (thread-local talloc context)
- `ik_pp_*` functions remain const/read-only (thread-safe for reading)
- Only `ik_scrollback_append_line()` needs synchronization (mutex)
- Output ordering handled by scrollback module (timestamps, channels, etc.)

## PP Tasks Complete When

- [ ] Format module implemented with 100% test coverage
- [ ] `ik_pp_workspace()` implemented and tested
- [ ] (Optional) Additional `ik_pp_*` functions for other types
- [ ] Temporary `/pp` command works in REPL (stdout)
- [ ] Manual verification passes all checks
- [ ] 100% test coverage maintained
- [ ] `make check && make lint && make coverage` all pass
- [ ] Documentation updated (this file + README.md)

## Development Approach

Strict TDD with 100% coverage requirement. Each task builds incrementally with full verification before moving to the next.

## Estimated Total Effort

- Task 1 (format module): ~150-200 lines
- Task 2 (format tests): ~300-400 lines
- Task 3 (pp functions): ~50-100 lines per structure (start with 1-2)
- Task 4 (pp tests): ~200-300 lines per structure
- Task 5 (REPL integration): ~100-150 lines
- Task 6 (JSON - optional): ~200-300 lines (defer if needed)

**Total**: ~1000-1500 lines (without JSON utilities)

**Time estimate**: 4-8 hours (depending on number of structures implemented)
