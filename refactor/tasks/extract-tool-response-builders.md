# Task: Extract Tool Response Builders

## Target

Refactor Issue #2: Extract JSON response builders from duplicated code in 5 tool files (~200 lines total)

## Context

All tool implementations follow an identical pattern for building JSON responses:

**Affected Files:**
1. `src/tool_bash.c` (lines 21-45, 96-130)
2. `src/tool_glob.c` (lines 29-74, 114-142)
3. `src/tool_grep.c` (lines 16-42, 104-139)
4. `src/tool_file_read.c` (lines 21-59, 110-142)
5. `src/tool_file_write.c` (lines 23-61, 92-129)

**Duplicated Error Response Pattern (~25 lines each, 5+ occurrences):**
```c
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
if (doc == NULL) PANIC("Out of memory");

yyjson_mut_val *root = yyjson_mut_obj(doc);
if (root == NULL) {
    yyjson_mut_doc_free(doc);
    PANIC("Out of memory");
}
yyjson_mut_doc_set_root(doc, root);

yyjson_mut_obj_add_bool(doc, root, "success", false);
yyjson_mut_obj_add_str(doc, root, "error", error_msg);

char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
if (json == NULL) {
    yyjson_mut_doc_free(doc);
    PANIC("Out of memory");
}

char *result = talloc_strdup(parent, json);
free(json);
yyjson_mut_doc_free(doc);

if (result == NULL) PANIC("Out of memory");
return OK(result);
```

**Duplicated Success Response Pattern (similar, with varying fields):**
```c
// Same boilerplate, but with:
yyjson_mut_obj_add_bool(doc, root, "success", true);
yyjson_mut_obj_add_str(doc, root, "output", output);
// ... additional tool-specific fields
```

This should be extracted into reusable builder functions.

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
- src/tool_bash.c - bash tool implementation
- src/tool_glob.c - glob tool implementation
- src/tool_grep.c - grep tool implementation
- src/tool_file_read.c - file read tool implementation
- src/tool_file_write.c - file write tool implementation

### Related Source Files
- src/tool.h - tool definitions and schemas
- src/tool_dispatcher.c - tool dispatch logic
- src/json_allocator.h - talloc-based JSON allocator
- src/vendor/yyjson/yyjson.h - JSON library

### Existing Test Files
- tests/unit/test_tool_bash.c
- tests/unit/test_tool_glob.c
- tests/unit/test_tool_grep.c
- tests/unit/test_tool_file_read.c
- tests/unit/test_tool_file_write.c

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. All tests pass (`make check`)
3. The five tool files contain the duplicated JSON building patterns

## Task

Create utility functions for building tool responses:
1. `ik_tool_response_error()` - builds error response JSON
2. `ik_tool_response_success()` - builds success response JSON with output
3. `ik_tool_response_success_with_fields()` - builds success response with custom fields

Then refactor all five tool files to use these utilities.

## API Design

### Header: `src/tool_response.h`

```c
#ifndef IK_TOOL_RESPONSE_H
#define IK_TOOL_RESPONSE_H

#include "error.h"
#include <talloc.h>
#include <yyjson.h>

// Build error response: {"success": false, "error": "message"}
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
res_t ik_tool_response_error(TALLOC_CTX *ctx, const char *error_msg, char **out);

// Build success response: {"success": true, "output": "content"}
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
res_t ik_tool_response_success(TALLOC_CTX *ctx, const char *output, char **out);

// Build success response with additional fields
// Caller provides a callback to add custom fields to the root object
// Returns: OK(json_string) where json_string is talloc-allocated on ctx
typedef void (*ik_tool_field_adder_t)(yyjson_mut_doc *doc, yyjson_mut_val *root, void *user_ctx);
res_t ik_tool_response_success_ex(TALLOC_CTX *ctx,
                                   const char *output,
                                   ik_tool_field_adder_t add_fields,
                                   void *user_ctx,
                                   char **out);

#endif
```

### Implementation: `src/tool_response.c`

Consolidates all JSON building boilerplate. Uses yyjson for JSON construction and talloc for memory management.

## TDD Cycle

### Red Phase

1. Create `tests/unit/test_tool_response.c` with test cases:
   - `test_tool_response_error_basic` - builds error response
   - `test_tool_response_error_special_chars` - handles quotes, newlines
   - `test_tool_response_success_basic` - builds success response
   - `test_tool_response_success_empty_output` - handles empty output
   - `test_tool_response_success_ex_with_fields` - adds custom fields
   - `test_tool_response_null_ctx` - asserts on NULL context
   - `test_tool_response_null_message` - asserts on NULL message

2. Create stub `src/tool_response.h` and `src/tool_response.c`.

3. Update Makefile to compile new files.

4. Run `make check` - tests should fail.

### Green Phase

1. Implement `ik_tool_response_error()`:
   - Create yyjson_mut_doc
   - Build root object with "success": false, "error": message
   - Serialize to JSON string
   - Copy to talloc-allocated buffer
   - Cleanup doc and return

2. Implement `ik_tool_response_success()`:
   - Similar pattern with "success": true, "output": output

3. Implement `ik_tool_response_success_ex()`:
   - Call add_fields callback before serialization

4. Run `make check` - all new tests should pass.

### Refactor Phase

1. Update `src/tool_bash.c`:
   - Include `tool_response.h`
   - Replace error response building with `ik_tool_response_error()`
   - Replace success response building with `ik_tool_response_success()`

2. Update `src/tool_glob.c`:
   - Include `tool_response.h`
   - Replace response building (may need `_ex` for matches array)

3. Update `src/tool_grep.c`:
   - Include `tool_response.h`
   - Replace response building (may need `_ex` for matches)

4. Update `src/tool_file_read.c`:
   - Include `tool_response.h`
   - Replace response building

5. Update `src/tool_file_write.c`:
   - Include `tool_response.h`
   - Replace response building

6. Run `make check` - all existing tests should still pass.

7. Run `make lint` - verify no new warnings.

8. Run `make coverage` - verify coverage maintained.

## Post-conditions

1. New `src/tool_response.h` and `src/tool_response.c` exist
2. New `tests/unit/test_tool_response.c` exists with full coverage
3. Five tool files refactored to use new utilities
4. All tests pass (`make check`)
5. Lint passes (`make lint`)
6. Coverage maintained at 100% (`make coverage`)
7. Working tree is clean (changes committed)

## Commit Strategy

### Commit 1: Add tool_response utilities with tests
```
feat: add tool response builder utilities

- New tool_response.h/c with ik_tool_response_error/success/success_ex
- Consolidates JSON response building boilerplate
- Full test coverage in test_tool_response.c
```

### Commit 2: Refactor tool_bash.c
```
refactor: use tool response builders in tool_bash.c

- Replace inline JSON building with utility functions
- Reduces code duplication by ~40 lines
```

### Commit 3: Refactor tool_glob.c
```
refactor: use tool response builders in tool_glob.c

- Replace inline JSON building with utility functions
- Reduces code duplication by ~40 lines
```

### Commit 4: Refactor tool_grep.c
```
refactor: use tool response builders in tool_grep.c

- Replace inline JSON building with utility functions
- Reduces code duplication by ~40 lines
```

### Commit 5: Refactor tool_file_read.c
```
refactor: use tool response builders in tool_file_read.c

- Replace inline JSON building with utility functions
- Reduces code duplication by ~40 lines
```

### Commit 6: Refactor tool_file_write.c
```
refactor: use tool response builders in tool_file_write.c

- Replace inline JSON building with utility functions
- Reduces code duplication by ~40 lines
```

## Risk Assessment

**Risk: Medium**
- API design needs to handle varying tool response fields
- The `_ex` callback pattern adds complexity
- Many files touched, but existing tests verify behavior

## Estimated Complexity

**Medium-High** - New file creation, flexible API design, and 5 file refactors

## Sub-agent Strategy

**Files affected in Refactor Phase:** 5 tool files with identical refactoring pattern

After the utility is created and tested (Green Phase complete), use sub-agents to parallelize the 5 tool file refactors:

```
Sub-agent 1: Refactor tool_bash.c
Sub-agent 2: Refactor tool_glob.c
Sub-agent 3: Refactor tool_grep.c
Sub-agent 4: Refactor tool_file_read.c
Sub-agent 5: Refactor tool_file_write.c
```

**Each sub-agent task:**
1. Read the target tool file
2. Read tool_response.h for the new API
3. Replace inline JSON building with `ik_tool_response_error()` / `ik_tool_response_success()` / `ik_tool_response_success_ex()`
4. Run `make check` to verify tests still pass
5. Commit the single file change

**Coordination:**
- Launch all 5 sub-agents in parallel after Green Phase completes
- Each sub-agent works on exactly one file
- No dependencies between the 5 refactors
- Main agent collects results and runs final `make lint && make coverage`

**Sub-agent prompt template:**
```
Refactor src/tool_XXX.c to use the new tool_response.h utilities.

Pre-read:
- src/tool_response.h (the new API)
- src/tool_XXX.c (the file to refactor)
- tests/unit/test_tool_XXX.c (existing tests)

Task:
1. Replace all inline JSON error response building with ik_tool_response_error()
2. Replace all inline JSON success response building with ik_tool_response_success() or ik_tool_response_success_ex()
3. Remove the duplicated yyjson boilerplate code
4. Run: make check
5. Commit with message: "refactor: use tool response builders in tool_XXX.c"

Return: {"ok": true} or {"ok": false, "reason": "..."}
```
