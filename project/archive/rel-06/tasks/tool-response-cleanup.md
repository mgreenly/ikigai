# Task: Document Response Builder Pattern and Verify

## Target

Refactoring #2: Eliminate Tool System Code Duplication - Response Building (Completion)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm
- source-code

### Source Files

- src/tool_response.h (final API)
- src/tool_response.c (all implementations)
- src/tool_grep.c, src/tool_glob.c, src/tool_bash.c, src/tool_file_read.c, src/tool_file_write.c (converted implementations)

### Test Patterns

- tests/unit/tool_response/tool_response_test.c
- tests/unit/repl/repl_tool_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- All 5 tools converted to use `ik_tool_response_success_with_data()`

## Task

Document the response builder pattern and verify the refactoring is complete.

### What

1. Add documentation to tool_response.h explaining the pattern
2. Verify all tools produce correct JSON output
3. Count lines saved vs. original implementation

### How

Update `src/tool_response.h` header comment:

```c
// Tool Response Builders
//
// Centralized response building for tool results. All tools return JSON in
// one of two envelope formats:
//
// Error:   {"success": false, "error": "message"}
// Success: {"success": true, "data": {...}}
//
// Usage:
//   // For errors:
//   char *result;
//   ik_tool_response_error(ctx, "Error message", &result);
//
//   // For success with data:
//   typedef struct { ... } my_data_t;
//   static void add_my_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *ctx) {
//       my_data_t *d = ctx;
//       yyjson_mut_obj_add_str(doc, data, "output", d->output);
//   }
//
//   my_data_t data = { .output = "result" };
//   char *result;
//   ik_tool_response_success_with_data(ctx, add_my_data, &data, &result);
//
// Adding a new tool:
// 1. Define result data struct
// 2. Define callback to populate data fields
// 3. Call ik_tool_response_success_with_data() or ik_tool_response_error()
```

### Why

Clear documentation ensures:
- Future developers understand the pattern
- Adding new tools uses consistent response format
- Intent of refactoring is preserved

## TDD Cycle

### Red

No new tests - verify existing tests cover all scenarios.

### Green

1. Add documentation comment to header
2. Run `make check`

### Verify

- `make check` passes
- `make lint` passes
- Documentation is clear and accurate
- All 5 tools use the response builder pattern

## Post-conditions

- Response builder refactoring is complete
- Documentation explains the pattern with usage examples
- All tests pass
- Pattern is ready for use when adding new tools

## Summary

After completing all tasks in this refactoring:

**Schema Building (Pattern 1):**
- `ik_tool_param_def_t` and `ik_tool_schema_def_t` structs enable declarative definitions
- `ik_tool_build_schema_from_def()` replaces 5 nearly-identical functions
- ~140 lines of duplicated code eliminated

**Response Building (Pattern 2):**
- `ik_tool_response_success_with_data()` centralizes JSON envelope construction
- Each tool provides a small callback for tool-specific data fields
- ~100 lines of duplicated code eliminated

**Total: ~240 lines eliminated, pattern established for future tools**
