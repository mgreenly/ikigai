# Internal Tool System Removal Specification

Complete specification for removing the internal tool system (Phase 1 of external tool migration).

## Overview

This document specifies exactly which files to delete, which to modify, and what changes to make. It is designed for unattended execution by sub-agents.

## Files to Delete

### Source Files

Delete these 6 implementation files:

1. `src/tool.c` - Schema builders, ik_tool_build_all(), ik_tool_truncate_output(), ik_tool_result_add_limit_metadata()
2. `src/tool_dispatcher.c` - ik_tool_dispatch() function
3. `src/tool_bash.c` - Built-in bash tool execution
4. `src/tool_file_read.c` - Built-in file_read tool execution
5. `src/tool_file_write.c` - Built-in file_write tool execution
6. `src/tool_glob.c` - Built-in glob tool execution
7. `src/tool_grep.c` - Built-in grep tool execution

### Test Files

Delete these 12 unit test files that test deleted functionality:

1. `tests/unit/tool/tool_schema_test.c` - Tests ik_tool_build_glob_schema(), etc.
2. `tests/unit/tool/tool_definition_test.c` - Tests ik_tool_build_schema_from_def()
3. `tests/unit/tool/tool_test.c` - Tests ik_tool_build_all()
4. `tests/unit/tool/dispatcher_test.c` - Tests ik_tool_dispatch()
5. `tests/unit/tool/bash_execute_test.c` - Tests ik_tool_exec_bash()
6. `tests/unit/tool/file_read_execute_test.c` - Tests ik_tool_exec_file_read()
7. `tests/unit/tool/file_write_execute_test.c` - Tests ik_tool_exec_file_write()
8. `tests/unit/tool/glob_execute_test.c` - Tests ik_tool_exec_glob()
9. `tests/unit/tool/grep_execute_test.c` - Tests ik_tool_exec_grep()
10. `tests/unit/tool/grep_edge_cases_test.c` - Tests grep edge cases
11. `tests/unit/tool/tool_limit_test.c` - Tests ik_tool_result_add_limit_metadata()
12. `tests/unit/tool/tool_truncate_test.c` - Tests ik_tool_truncate_output()

Delete these 5 integration test files that test deleted functionality:

1. `tests/integration/tool_loop_limit_test.c` - Tests tool loop with limit metadata
2. `tests/integration/tool_choice_auto_test.c` - Tests auto tool choice with internal tools
3. `tests/integration/tool_choice_specific_test.c` - Tests specific tool choice
4. `tests/integration/tool_choice_required_test.c` - Tests required tool choice
5. `tests/integration/tool_choice_none_test.c` - Tests none tool choice

### Test Files to Keep

Keep these 2 unit test files (they test components we're keeping):

1. `tests/unit/tool/tool_arg_parser_test.c` - Tests ik_tool_arg_get_string(), ik_tool_arg_get_int() (keeping tool_arg_parser.c)
2. `tests/unit/tool/tool_call_test.c` - Tests ik_tool_call_create() (keeping this function)

## Files to Modify

### 1. src/tool.h

**Purpose:** Remove declarations for deleted functions and types, keep declarations for preserved functions.

**Functions to remove from tool.h:**

Lines 43-52: Remove `ik_tool_add_string_parameter()`
```c
void ik_tool_add_string_parameter(yyjson_mut_doc *doc,
                                  yyjson_mut_val *properties,
                                  const char *name,
                                  const char *description);
```

Lines 54-62: Remove `ik_tool_build_glob_schema()`
```c
yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc);
```

Lines 64-72: Remove `ik_tool_build_file_read_schema()`
```c
yyjson_mut_val *ik_tool_build_file_read_schema(yyjson_mut_doc *doc);
```

Lines 74-82: Remove `ik_tool_build_grep_schema()`
```c
yyjson_mut_val *ik_tool_build_grep_schema(yyjson_mut_doc *doc);
```

Lines 84-92: Remove `ik_tool_build_file_write_schema()`
```c
yyjson_mut_val *ik_tool_build_file_write_schema(yyjson_mut_doc *doc);
```

Lines 94-102: Remove `ik_tool_build_bash_schema()`
```c
yyjson_mut_val *ik_tool_build_bash_schema(yyjson_mut_doc *doc);
```

Lines 104-113: Remove `ik_tool_build_schema_from_def()`
```c
yyjson_mut_val *ik_tool_build_schema_from_def(yyjson_mut_doc *doc, const ik_tool_schema_def_t *def);
```

Lines 115-126: Remove `ik_tool_build_all()`
```c
yyjson_mut_val *ik_tool_build_all(yyjson_mut_doc *doc);
```

Lines 128-142: Remove `ik_tool_truncate_output()`
```c
char *ik_tool_truncate_output(void *parent, const char *output, size_t max_size);
```

Lines 169-180: Remove `ik_tool_exec_glob()`
```c
res_t ik_tool_exec_glob(void *parent, const char *pattern, const char *path);
```

Lines 182-192: Remove `ik_tool_exec_file_read()`
```c
res_t ik_tool_exec_file_read(void *parent, const char *path);
```

Lines 194-206: Remove `ik_tool_exec_grep()`
```c
res_t ik_tool_exec_grep(void *parent, const char *pattern, const char *glob, const char *path);
```

Lines 208-220: Remove `ik_tool_exec_file_write()`
```c
res_t ik_tool_exec_file_write(void *parent, const char *path, const char *content);
```

Lines 222-236: Remove `ik_tool_exec_bash()`
```c
res_t ik_tool_exec_bash(void *parent, const char *command);
```

Lines 238-262: Remove `ik_tool_dispatch()`
```c
res_t ik_tool_dispatch(void *parent, const char *tool_name, const char *arguments);
```

Lines 264-277: Remove `ik_tool_result_add_limit_metadata()`
```c
char *ik_tool_result_add_limit_metadata(void *parent, const char *result_json, int32_t max_tool_turns);
```

**Types to remove from tool.h:**

Lines 15-20: Remove `ik_tool_param_def_t`
```c
typedef struct {
    const char *name;
    const char *description;
    bool required;
} ik_tool_param_def_t;
```

Lines 22-28: Remove `ik_tool_schema_def_t`
```c
typedef struct {
    const char *name;
    const char *description;
    const ik_tool_param_def_t *params;
    size_t param_count;
} ik_tool_schema_def_t;
```

**Types and functions to KEEP in tool.h:**

Lines 8-13: Keep `ik_tool_call_t`
```c
typedef struct {
    char *id;
    char *name;
    char *arguments;
} ik_tool_call_t;
```

Lines 30-41: Keep `ik_tool_call_create()`
```c
ik_tool_call_t *ik_tool_call_create(TALLOC_CTX *ctx, const char *id, const char *name, const char *arguments);
```

Lines 144-154: Keep `ik_tool_arg_get_string()`
```c
char *ik_tool_arg_get_string(void *parent, const char *arguments_json, const char *key);
```

Lines 156-167: Keep `ik_tool_arg_get_int()`
```c
bool ik_tool_arg_get_int(const char *arguments_json, const char *key, int *out_value);
```

**After modifications, tool.h will contain:**
- Header guard and includes (lines 1-6)
- `ik_tool_call_t` typedef (currently lines 8-13)
- `ik_tool_call_create()` declaration (currently lines 30-41)
- `ik_tool_arg_get_string()` declaration (currently lines 144-154)
- `ik_tool_arg_get_int()` declaration (currently lines 156-167)
- Footer (line 279: `#endif // IK_TOOL_H`)

### 2. src/tool_arg_parser.c

**No changes required.** This file only contains `ik_tool_arg_get_string()` and `ik_tool_arg_get_int()` which we are keeping.

### 3. src/tool_response.c

**DELETE.** This file's functions are ONLY called by the internal tool implementations being deleted (tool_bash.c, tool_file_read.c, tool_file_write.c, tool_glob.c, tool_grep.c). No other code uses these functions. After deleting the internal tools, this file has no callers.

### 4. src/tool_response.h

**DELETE.** Header for tool_response.c - delete together with implementation.

### 5. src/providers/openai/request_chat.c

**Purpose:** Replace call to `ik_tool_build_all()` with stub that returns empty tools array.

**Current code (lines 190-195):**
```c
    /* Build and add tools array */
    yyjson_mut_val *tools_arr = ik_tool_build_all(doc);
    if (tools_arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add tools array to JSON"); // LCOV_EXCL_LINE
    }
```

**Replace with (empty tools array stub):**
```c
    /* Build and add tools array - TODO(rel-08): Replace with ik_tool_registry_build_all() */
    yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
    if (tools_arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    /* Empty tools array - no tools available until external tool system is implemented */
    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to add tools array to JSON"); // LCOV_EXCL_LINE
    }
```

**Rationale:** Phase 1 removes internal tools but doesn't yet implement external tools. This stub allows the system to compile and run without tools. Phase 2 will replace this stub with registry-based tool building.

### 5a. src/providers/anthropic/request.c

**Purpose:** Replace tool serialization with stub that returns empty tools array.

**Current code (lines 123-170):**
The current implementation iterates over `req->tools` and `req->tool_count` to build the tools array using `input_schema` format.

**Replace with (empty tools array stub):**

Replace the tools serialization block (lines 123-201) with:
```c
    // Add tools - TODO(rel-08): Replace with ik_tool_registry_build_anthropic()
    // Empty tools array - no tools available until external tool system is implemented
    if (req->tool_count > 0) {
        yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
        if (!tools_arr) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        /* Stub: no tools sent to Anthropic until external tool system implemented */
        if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }
    }
```

**Rationale:** Anthropic provider currently reads from `req->tools` which is populated by internal tool system. After removal, this code would reference undefined tool definitions. The stub ensures compilation succeeds and requests go through without tools.

### 5b. src/providers/google/request.c

**Purpose:** Replace tool serialization with stub that returns empty tools structure.

**Current code (lines 123-191):**
The `serialize_tools()` function iterates over `req->tools` and `req->tool_count` to build the `functionDeclarations` array, removing `additionalProperties` from each schema.

**Replace with (empty tools stub):**

Replace the `serialize_tools()` function body with:
```c
static bool serialize_tools(yyjson_mut_doc *doc, yyjson_mut_val *root,
                             const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    // TODO(rel-08): Replace with ik_tool_registry_build_google()
    // Stub: no tools sent to Google until external tool system implemented
    (void)req; // Suppress unused parameter warning
    return true;
}
```

**Rationale:** Google provider currently reads from `req->tools` which is populated by internal tool system. After removal, this code would reference undefined tool definitions. The stub ensures compilation succeeds and requests go through without tools.

### 6. src/repl_tool.c

**Purpose:** Replace calls to `ik_tool_dispatch()` with stub that returns "no tools available" error.

**Call site 1: ik_repl_execute_pending_tool() (line 88):**

Current code:
```c
    // 2. Execute tool
    res_t tool_res = ik_tool_dispatch(repl, tc->name, tc->arguments);
    if (is_err(&tool_res)) PANIC("tool dispatch failed"); // LCOV_EXCL_BR_LINE
    char *result_json = tool_res.ok;
```

Replace with:
```c
    // 2. Execute tool - TODO(rel-08): Replace with registry lookup + ik_tool_external_exec()
    char *result_json = talloc_asprintf(repl,
        "{\"success\": false, \"error\": \"Tool system not yet implemented. Tool '%s' unavailable.\"}",
        tc->name);
    if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
```

**Call site 2: tool_thread_worker() (line 43):**

Current code:
```c
    // Execute tool - this is the potentially slow operation.
    // All allocations go into args->ctx which main thread will free.
    res_t result = ik_tool_dispatch(args->ctx, args->tool_name, args->arguments);

    // Store result directly in agent context.
    // Safe without mutex: main thread won't read until complete=true,
    // and setting complete=true (below) provides the memory barrier.
    args->agent->tool_thread_result = result.ok;
```

Replace with:
```c
    // Execute tool - TODO(rel-08): Replace with registry lookup + ik_tool_external_exec()
    // Stub: return "not yet implemented" error
    char *result_json = talloc_asprintf(args->ctx,
        "{\"success\": false, \"error\": \"Tool system not yet implemented. Tool '%s' unavailable.\"}",
        args->tool_name);
    if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Store result directly in agent context.
    // Safe without mutex: main thread won't read until complete=true,
    // and setting complete=true (below) provides the memory barrier.
    args->agent->tool_thread_result = result_json;
```

**Rationale:** These stubs allow the REPL to continue functioning without tools until external tool system is implemented in Phase 2.

### 7. Makefile

**Purpose:** Remove deleted source files from MODULE_SOURCES, CLIENT_SOURCES, and MODULE_SOURCES_NO_DB.

**Line 106 - CLIENT_SOURCES:**

Remove these entries:
- `src/tool.c`
- `src/tool_dispatcher.c`
- `src/tool_bash.c`
- `src/tool_file_read.c`
- `src/tool_file_write.c`
- `src/tool_glob.c`
- `src/tool_grep.c`

Keep this entry:
- `src/tool_arg_parser.c`

Also remove (no callers after internal tools deleted):
- `src/tool_response.c`

**Line 121 - MODULE_SOURCES:**

Same removals as CLIENT_SOURCES.

**Line 125 - MODULE_SOURCES_NO_DB:**

Same removals as CLIENT_SOURCES.

**Exact change for line 106:**

Current (partial):
```makefile
CLIENT_SOURCES = ... src/event_render.c src/tool.c src/tool_arg_parser.c src/tool_response.c src/tool_glob.c src/tool_file_read.c src/tool_grep.c src/tool_file_write.c src/tool_bash.c src/tool_dispatcher.c src/msg.c ...
```

Replace with:
```makefile
CLIENT_SOURCES = ... src/event_render.c src/tool_arg_parser.c src/msg.c ...
```

**Exact change for line 121:**

Current (partial):
```makefile
MODULE_SOURCES = ... src/event_render.c src/tool.c src/tool_arg_parser.c src/tool_response.c src/tool_glob.c src/tool_file_read.c src/tool_grep.c src/tool_file_write.c src/tool_bash.c src/tool_dispatcher.c src/msg.c ...
```

Replace with:
```makefile
MODULE_SOURCES = ... src/event_render.c src/tool_arg_parser.c src/msg.c ...
```

**Exact change for line 125:**

Current (partial):
```makefile
MODULE_SOURCES_NO_DB = ... src/event_render.c src/tool.c src/tool_arg_parser.c src/tool_response.c src/tool_glob.c src/tool_file_read.c src/tool_grep.c src/tool_file_write.c src/tool_bash.c src/tool_dispatcher.c src/msg.c ...
```

Replace with:
```makefile
MODULE_SOURCES_NO_DB = ... src/event_render.c src/tool_arg_parser.c src/msg.c ...
```

## Verification Steps

After completing removals and modifications, verify:

1. **Compilation:** `make clean && make` should succeed
2. **Header integrity:** `src/tool.h` contains only 4 items:
   - `ik_tool_call_t` typedef
   - `ik_tool_call_create()` declaration
   - `ik_tool_arg_get_string()` declaration
   - `ik_tool_arg_get_int()` declaration
3. **Test compilation:** Unit tests for `tool_arg_parser_test` and `tool_call_test` should still compile
4. **Integration tests:** All tool_choice_* integration tests should be deleted (they test deleted functionality)
5. **Stub behavior:** Running ikigai and attempting tool use returns "Tool system not yet implemented" error

## Migration Context

This removal is Phase 1 of the external tool migration. After this phase:

- Internal tool system is completely removed
- System compiles and runs but has no tools
- LLM requests include empty tools array
- Tool calls return "not yet implemented" error
- Foundation is clean for Phase 2: implementing external tool infrastructure

Phase 2 will replace the stubs in `src/providers/openai/request_chat.c` and `src/repl_tool.c` with registry-based tool building and external tool execution.

## Dependencies

Files we're deleting have these include dependencies:

- `src/tool.c` includes: `tool.h`, `panic.h`, `wrapper.h`
- `src/tool_dispatcher.c` includes: `tool.h`, `panic.h`, `wrapper.h`
- `src/tool_bash.c` includes: `tool.h`, `tool_response.h`, `panic.h`, `wrapper.h`
- `src/tool_file_read.c` includes: `tool.h`, `tool_response.h`, `file_utils.h`, `panic.h`, `wrapper.h`
- `src/tool_file_write.c` includes: `tool.h`, `tool_response.h`, `panic.h`, `wrapper.h`
- `src/tool_glob.c` includes: `tool.h`, `tool_response.h`, `panic.h`, `wrapper.h`
- `src/tool_grep.c` includes: `tool.h`, `tool_response.h`, `line_array.h`, `file_utils.h`, `panic.h`, `wrapper.h`

After deletion, these headers are still used by:
- `tool.h` - Used by `src/tool_arg_parser.c`, `src/providers/openai/request_chat.c`, `src/repl_tool.c`
- `tool_response.h` - DELETE (no remaining callers)

## Summary

**Total files deleted:** 26 (9 source files + 17 test files)
  - Source: tool.c, tool_dispatcher.c, tool_bash.c, tool_file_read.c, tool_file_write.c, tool_glob.c, tool_grep.c, tool_response.c, tool_response.h
**Total files modified:** 6 (tool.h, openai/request_chat.c, anthropic/request.c, google/request.c, repl_tool.c, Makefile)
**Files kept unchanged:** 1 (tool_arg_parser.c)
**Lines removed from tool.h:** ~235 lines (functions and types)
**Lines kept in tool.h:** ~50 lines (ik_tool_call_t and arg parsing functions)

### Provider Stub Summary

| Provider | File | Stub Change |
|----------|------|-------------|
| OpenAI | `src/providers/openai/request_chat.c` | Replace `ik_tool_build_all()` with empty array |
| Anthropic | `src/providers/anthropic/request.c` | Replace tools serialization block with empty array |
| Google | `src/providers/google/request.c` | Replace `serialize_tools()` body with no-op |
