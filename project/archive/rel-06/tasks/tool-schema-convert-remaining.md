# Task: Convert Remaining Schema Builders to Use New Builder

## Target

Refactoring #2: Eliminate Tool System Code Duplication - Schema Building

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- patterns/factory

### Source Files

- src/tool.h (ik_tool_build_schema_from_def, ik_tool_schema_def_t)
- src/tool.c (remaining schema builders to convert):
  - lines 100-135: ik_tool_build_file_read_schema
  - lines 138-173: ik_tool_build_file_write_schema
  - lines 176-211: ik_tool_build_bash_schema
  - lines 214-249: ik_tool_build_grep_schema

### Test Patterns

- tests/unit/tool/tool_test.c:
  - test_tool_build_file_read_schema_structure
  - test_tool_build_file_write_schema_structure
  - test_tool_build_bash_schema_structure
  - test_tool_build_grep_schema_structure
  - test_tool_build_all

## Pre-conditions

- Working tree is clean
- `make check` passes
- `ik_tool_build_glob_schema()` already converted (pattern established)

## Task

Convert remaining 4 schema builders to use declarative definitions.

### What

Convert these functions to use `ik_tool_build_schema_from_def()`:
1. `ik_tool_build_file_read_schema()`
2. `ik_tool_build_file_write_schema()`
3. `ik_tool_build_bash_schema()`
4. `ik_tool_build_grep_schema()`

### How

Follow the same pattern established for glob. For each tool:

**file_read:**
```c
static const ik_tool_param_def_t file_read_params[] = {
    {"path", "Path to file", true}
};

static const ik_tool_schema_def_t file_read_schema_def = {
    .name = "file_read",
    .description = "Read contents of a file",
    .params = file_read_params,
    .param_count = 1
};

yyjson_mut_val *ik_tool_build_file_read_schema(yyjson_mut_doc *doc)
{
    return ik_tool_build_schema_from_def(doc, &file_read_schema_def);
}
```

**file_write:**
```c
static const ik_tool_param_def_t file_write_params[] = {
    {"path", "Path to file", true},
    {"content", "Content to write", true}
};

static const ik_tool_schema_def_t file_write_schema_def = {
    .name = "file_write",
    .description = "Write content to a file",
    .params = file_write_params,
    .param_count = 2
};
```

**bash:**
```c
static const ik_tool_param_def_t bash_params[] = {
    {"command", "Command to execute", true}
};

static const ik_tool_schema_def_t bash_schema_def = {
    .name = "bash",
    .description = "Execute a shell command",
    .params = bash_params,
    .param_count = 1
};
```

**grep:**
```c
static const ik_tool_param_def_t grep_params[] = {
    {"pattern", "Search pattern (regex)", true},
    {"path", "File or directory to search", false},
    {"glob", "File pattern filter (e.g., '*.c')", false}
};

static const ik_tool_schema_def_t grep_schema_def = {
    .name = "grep",
    .description = "Search file contents for a pattern",
    .params = grep_params,
    .param_count = 3
};
```

### Why

Completing the conversion demonstrates the full value of the refactoring:
- 140+ lines of duplicated code replaced by ~40 lines of declarations
- All 5 tools now use the same implementation
- Adding new tools becomes trivial

### Sub-agent opportunity

This task is parallelizable - each schema conversion is independent. A sub-agent could convert each tool in parallel, then merge results.

## TDD Cycle

### Red

No new tests needed - existing tests verify each schema builder.

Run `make check` before changes to confirm baseline passes.

### Green

1. Add static definitions for file_read above its function
2. Replace file_read function body
3. Run `make check` - verify file_read test passes
4. Repeat for file_write, bash, grep
5. Run `make check` after each conversion

### Verify

- `make check` passes
- `make lint` passes
- All 5 schema structure tests pass
- `test_tool_build_all` passes

## Post-conditions

- All 5 schema builders use declarative definitions
- All function signatures unchanged
- Static definition arrays for all 5 tools
- All existing tests pass unchanged
- ~140 lines of duplicate code replaced
