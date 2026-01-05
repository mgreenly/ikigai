# Task: Remove Internal Tool System

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/extended
**Depends on:** tool-grep.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task removes the internal tool system to create a clean slate for external tools. After completion, ikigai will build and run but tool calls return "not yet implemented" stubs.

## Pre-Read

**Skills:**
- `/load errors` - Result types for understanding code being removed
- `/load style` - Code style for modifications

**Plan:**
- `cdd/plan/removal-specification.md` - **CRITICAL: Follow this exactly** - complete list of files to delete, modify, and keep

**Source (to understand what's being removed):**
- `src/tool.h` - Current header (will be heavily modified)
- `src/repl_tool.c` - Will have dispatch replaced with stub

## Libraries

No new libraries. Only removing code.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] All 6 external tools build successfully

## Objective

Remove the internal tool system per `cdd/plan/removal-specification.md`:
1. Delete source files
2. Delete test files
3. Modify headers to remove deleted declarations
4. Replace tool dispatch with stubs
5. Update Makefile

After this task, `make check` passes and ikigai runs (but tool calls return stub error).

## Files to Delete (9 source files)

```
src/tool.c
src/tool_dispatcher.c
src/tool_bash.c
src/tool_file_read.c
src/tool_file_write.c
src/tool_glob.c
src/tool_grep.c
src/tool_response.c
src/tool_response.h
```

## Test Files to Delete (17 test files)

**Unit tests (12 files):**
```
tests/unit/tool/tool_schema_test.c
tests/unit/tool/tool_definition_test.c
tests/unit/tool/tool_test.c
tests/unit/tool/dispatcher_test.c
tests/unit/tool/bash_execute_test.c
tests/unit/tool/file_read_execute_test.c
tests/unit/tool/file_write_execute_test.c
tests/unit/tool/glob_execute_test.c
tests/unit/tool/grep_execute_test.c
tests/unit/tool/grep_edge_cases_test.c
tests/unit/tool/tool_limit_test.c
tests/unit/tool/tool_truncate_test.c
```

**Integration tests (5 files):**
```
tests/integration/tool_loop_limit_test.c
tests/integration/tool_choice_auto_test.c
tests/integration/tool_choice_specific_test.c
tests/integration/tool_choice_required_test.c
tests/integration/tool_choice_none_test.c
```

## Files to Keep (test files)

```
tests/unit/tool/tool_arg_parser_test.c
tests/unit/tool/tool_call_test.c
```

## Files to Modify

### 1. src/tool.h

Remove all function declarations and types EXCEPT:
- `ik_tool_call_t` typedef
- `ik_tool_call_create()` declaration
- `ik_tool_arg_get_string()` declaration
- `ik_tool_arg_get_int()` declaration

Remove these types:
- `ik_tool_param_def_t`
- `ik_tool_schema_def_t`

Remove these function declarations:
- `ik_tool_add_string_parameter()`
- `ik_tool_build_glob_schema()`
- `ik_tool_build_file_read_schema()`
- `ik_tool_build_grep_schema()`
- `ik_tool_build_file_write_schema()`
- `ik_tool_build_bash_schema()`
- `ik_tool_build_schema_from_def()`
- `ik_tool_build_all()`
- `ik_tool_truncate_output()`
- `ik_tool_exec_glob()`
- `ik_tool_exec_file_read()`
- `ik_tool_exec_grep()`
- `ik_tool_exec_file_write()`
- `ik_tool_exec_bash()`
- `ik_tool_dispatch()`
- `ik_tool_result_add_limit_metadata()`

### 2. src/providers/request_tools.c

Remove the hard-coded tool definitions and skip tool population:

**Remove:** Static tool definition structs (glob_params, file_read_params, etc.)
**Remove:** `build_tool_parameters_json()` helper function
**Replace:** Tool population loop with stub comment:

```c
// TODO(rel-08): Replace with external tool registry lookup
// Internal tools removed - no tools available until external tool system
(void)0; // No-op placeholder
```

### 3. src/repl_tool.c

Replace `ik_tool_dispatch()` calls with stub:

**In ik_repl_execute_pending_tool():**
```c
// 2. Execute tool - TODO(rel-08): Replace with registry lookup + ik_tool_external_exec()
char *result_json = talloc_asprintf(repl,
    "{\"success\": false, \"error\": \"Tool system not yet implemented. Tool '%s' unavailable.\"}",
    tc->name);
if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
```

**In tool_thread_worker():**
```c
// Execute tool - TODO(rel-08): Replace with registry lookup + ik_tool_external_exec()
char *result_json = talloc_asprintf(args->ctx,
    "{\"success\": false, \"error\": \"Tool system not yet implemented. Tool '%s' unavailable.\"}",
    args->tool_name);
if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
args->agent->tool_thread_result = result_json;
```

### 4. Makefile

Remove from CLIENT_SOURCES, MODULE_SOURCES, MODULE_SOURCES_NO_DB:
- `src/tool.c`
- `src/tool_dispatcher.c`
- `src/tool_bash.c`
- `src/tool_file_read.c`
- `src/tool_file_write.c`
- `src/tool_glob.c`
- `src/tool_grep.c`
- `src/tool_response.c`

Keep:
- `src/tool_arg_parser.c`

## Test Specification

**Reference:** `cdd/plan/test-specification.md` → "Phase 3: Remove Internal Tools"

**No new tests to create.** This is a removal task.

**Existing tests to KEEP (verify still pass):**
- `tests/unit/tool/tool_arg_parser_test.c`
- `tests/unit/tool/tool_call_test.c`

**Tests being DELETED (17 files):**
- All tests in `tests/unit/tool/` except the two above
- All tests in `tests/integration/` matching `tool_*`

**Verification Steps:**
1. `make clean && make` - Must compile without errors
2. `make check` - All remaining tests pass
3. `bin/ikigai` - Must start and show prompt
4. LLM requests work (with stub tools response)

**Coverage impact:** Coverage percentage may temporarily drop as internal tool code is removed but external tool tests not yet added. This is expected.

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(remove-internal-tools.md): [success|partial|failed] - removed internal tool system

Deleted 9 source files and 17 test files. Stubbed tool dispatch.
ikigai builds and runs but tools return "not yet implemented".
EOF
)"
```

Report status:
- Success: `/task-done remove-internal-tools.md`
- Partial/Failed: `/task-fail remove-internal-tools.md`

## Postconditions

- [ ] All 9 source files deleted
- [ ] All 17 test files deleted
- [ ] 2 test files kept (tool_arg_parser_test.c, tool_call_test.c)
- [ ] src/tool.h contains only 4 items
- [ ] `make clean && make` succeeds
- [ ] `make check` passes
- [ ] `bin/ikigai` starts
- [ ] All changes committed
- [ ] Working copy is clean
