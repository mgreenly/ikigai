# Test Specification

This document specifies the tests required for TDD during task execution. Each task should create tests BEFORE implementing functionality.

## Testing Philosophy

**Red-Green-Verify cycle applies to all tasks:**
1. Create test file with failing tests (Red)
2. Implement minimal code to pass (Green)
3. Run `make check` to verify (Verify)

**Coverage requirement:** 100% line and branch coverage. Use `LCOV_EXCL_BR_LINE` for OOM/PANIC branches only.

## Test Patterns Reference

Before creating tests, read these existing patterns:
- `tests/unit/commands/dispatch_test.c` - Command testing with mock REPL
- `tests/unit/error/error_test.c` - Result type testing
- `tests/test_utils.h` - Helper functions and mocking infrastructure

**Test file structure:**
```c
#include <check.h>
#include <talloc.h>

static void *ctx;
static void setup(void) { ctx = talloc_new(NULL); }
static void teardown(void) { talloc_free(ctx); }

START_TEST(test_name) {
    // Arrange, Act, Assert
}
END_TEST

static Suite *suite_name(void) {
    Suite *s = suite_create("Module/Feature");
    TCase *tc = tcase_create("Core");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_name);
    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    Suite *s = suite_name();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed ? 1 : 0;
}
```

---

## Phase 1: External Tools

External tools are standalone executables. Test via shell commands during manual verification, but also create unit tests for JSON parsing/building logic if complex helper functions are extracted.

### tool-bash.md

**Test file:** None (external executable)

**Manual verification (in order.json stop):**
```bash
libexec/ikigai/bash --schema              # Returns valid JSON schema
echo '{"command":"echo hello"}' | libexec/ikigai/bash  # Returns {"output":"hello","exit_code":0}
echo '{"command":"false"}' | libexec/ikigai/bash       # Returns exit_code != 0
echo '{}' | libexec/ikigai/bash            # Returns error JSON (missing command)
```

**If helper functions extracted** (e.g., `parse_bash_args()`, `build_bash_response()`):
- Create `tests/unit/tools/bash_helpers_test.c`
- Test JSON parsing edge cases
- Test output buffer growth
- Test trailing newline stripping

### tool-file-read.md

**Manual verification:**
```bash
libexec/ikigai/file-read --schema
echo '{"file_path":"/etc/hostname"}' | libexec/ikigai/file-read
echo '{"file_path":"/nonexistent"}' | libexec/ikigai/file-read  # FILE_NOT_FOUND
echo '{"file_path":"Makefile","offset":1,"limit":5}' | libexec/ikigai/file-read
echo '{"file_path":"Makefile","offset":99999}' | libexec/ikigai/file-read  # Empty output
```

**Behaviors to verify:**
- Entire file read when no offset/limit
- Line-based offset (1-based)
- Offset beyond EOF returns empty (not error)
- Permission denied returns PERMISSION_DENIED error code

### tool-file-write.md

**Manual verification:**
```bash
libexec/ikigai/file-write --schema
echo '{"file_path":"/tmp/test.txt","content":"hello"}' | libexec/ikigai/file-write
cat /tmp/test.txt  # Verify content
echo '{"file_path":"/etc/passwd","content":"x"}' | libexec/ikigai/file-write  # PERMISSION_DENIED
echo '{"file_path":"/nonexistent/x","content":"x"}' | libexec/ikigai/file-write  # OPEN_FAILED
```

**Behaviors to verify:**
- Creates file if not exists
- Overwrites file if exists
- Empty content creates 0-byte file
- Byte count in response matches strlen(content)

### tool-file-edit.md

**Manual verification:**
```bash
libexec/ikigai/file-edit --schema
# Create test file
echo "hello world" > /tmp/edit_test.txt
echo '{"file_path":"/tmp/edit_test.txt","old_string":"world","new_string":"universe"}' | libexec/ikigai/file-edit
cat /tmp/edit_test.txt  # "hello universe"
```

**Behaviors to verify:**
- Single unique match: replaces, returns replacements:1
- Multiple matches + replace_all:false: returns NOT_UNIQUE error
- Multiple matches + replace_all:true: replaces all, returns count
- old_string not found: returns NOT_FOUND error
- old_string == new_string: returns INVALID_ARG error
- Empty old_string: returns INVALID_ARG error

### tool-glob.md

**Manual verification:**
```bash
libexec/ikigai/glob --schema
echo '{"pattern":"*.c","path":"src"}' | libexec/ikigai/glob
echo '{"pattern":"nonexistent*"}' | libexec/ikigai/glob  # count:0, not error
```

**Behaviors to verify:**
- Pattern matching returns sorted list
- No matches returns count:0 (success, not error)
- Output format: one path per line, no trailing newline
- Path parameter prepended to pattern

### tool-grep.md

**Manual verification:**
```bash
libexec/ikigai/grep --schema
echo '{"pattern":"main","glob":"*.c","path":"src"}' | libexec/ikigai/grep
echo '{"pattern":"[invalid"}' | libexec/ikigai/grep  # INVALID_PATTERN
echo '{"pattern":"nonexistent_string"}' | libexec/ikigai/grep  # count:0
```

**Behaviors to verify:**
- POSIX extended regex matching
- Output format: `filename:line_number: line_content`
- Invalid regex returns INVALID_PATTERN error
- No matches returns count:0 (success, not error)
- Glob filter limits searched files

---

## Phase 2: Discovery Infrastructure

### discovery-infrastructure.md

**Test files to create:**

#### tests/unit/tool_registry/registry_test.c

**Goals:**
- Test registry creation and lifecycle
- Test entry lookup by name
- Test schema building for providers

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_registry_create` | `ik_tool_registry_create()` returns non-NULL, count=0 |
| `test_registry_lookup_empty` | Lookup on empty registry returns NULL |
| `test_registry_lookup_not_found` | Lookup for non-existent name returns NULL |
| `test_registry_lookup_found` | After adding entry, lookup returns correct entry |
| `test_registry_build_all_empty` | `ik_tool_registry_build_all()` with empty registry returns empty array |
| `test_registry_build_all_entries` | With entries, returns JSON array with all schemas |

**Mocking:** None needed - registry is pure data structure

#### tests/unit/tool_discovery/discovery_test.c

**Goals:**
- Test tool scanning from directories
- Test schema parsing
- Test error handling for invalid tools

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_discovery_empty_dirs` | Empty directories return success with 0 tools |
| `test_discovery_nonexistent_dir` | Non-existent directory handled gracefully |
| `test_discovery_invalid_schema` | Tool returning invalid JSON logged, skipped |
| `test_discovery_timeout` | Tool not responding within 1s is skipped |
| `test_discovery_user_overrides_system` | Same name in user dir overrides system dir |

**Mocking:**
- Create temporary directories with mock executables
- Mock executables: shell scripts that echo JSON schemas
- Use `mkdtemp()` for isolation

#### tests/unit/tool_external/external_exec_test.c

**Goals:**
- Test tool execution with JSON input/output
- Test timeout handling
- Test error cases

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_exec_success` | Valid tool returns JSON output |
| `test_exec_invalid_json_output` | Tool returning non-JSON returns error |
| `test_exec_timeout` | Tool exceeding 30s timeout returns error |
| `test_exec_nonexistent_path` | Non-existent tool path returns error |
| `test_exec_permission_denied` | Non-executable file returns error |

**Mocking:**
- Create temporary shell scripts as mock tools
- One that echoes valid JSON
- One that sleeps forever (timeout test)
- One that exits non-zero

#### tests/unit/tool_wrapper/wrapper_test.c

**Goals:**
- Test success/failure envelope building
- Test JSON structure correctness

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_wrap_success_simple` | Wraps result in success envelope |
| `test_wrap_success_nested_json` | Nested JSON preserved correctly |
| `test_wrap_failure_all_fields` | All failure fields populated |
| `test_wrap_failure_null_fields` | NULL optional fields handled |

**Mocking:** None - pure JSON building

---

## Phase 3: Remove Internal Tools

### remove-internal-tools.md

**No new tests to create.**

**Existing tests to verify still pass:**
- `tests/unit/tool/tool_arg_parser_test.c` - KEEP
- `tests/unit/tool/tool_call_test.c` - KEEP

**Verification:**
- `make check` passes after removal
- Removed test files no longer referenced in Makefile

---

## Phase 4: Provider Integration

### provider-anthropic.md

**Test file:** `tests/unit/providers/anthropic/tools_serialize_test.c`

**Goals:**
- Verify tools from registry serialized to Anthropic format
- Verify `input_schema` key used (not `parameters`)

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_serialize_tools_empty` | No tools produces no tools array |
| `test_serialize_tools_single` | Single tool has correct Anthropic format |
| `test_serialize_tools_input_schema` | Parameters wrapped in `input_schema` |
| `test_serialize_tools_all_fields` | name, description, input_schema present |

**Mocking:**
- Create mock registry with known entries
- Verify JSON output structure

**Pattern to follow:** `tests/unit/providers/anthropic/request_serialize_test.c`

### provider-google.md

**Test file:** `tests/unit/providers/google/tools_serialize_test.c`

**Goals:**
- Verify tools serialized to Gemini `functionDeclarations` format
- Verify `additionalProperties` field removed

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_serialize_function_declarations` | Tools wrapped in functionDeclarations array |
| `test_serialize_removes_additional_properties` | additionalProperties stripped from schema |
| `test_serialize_preserves_required` | required array preserved |

**Pattern to follow:** `tests/unit/providers/google/request_test.c`

### provider-openai.md

**Test file:** `tests/unit/providers/openai/tools_serialize_test.c`

**Goals:**
- Verify tools wrapped in `{type: "function", function: {...}}`
- Verify strict mode: `additionalProperties: false`, all props in `required[]`

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_serialize_function_wrapper` | Each tool has type:function wrapper |
| `test_serialize_strict_true` | strict:true added to function |
| `test_serialize_additional_properties_false` | additionalProperties:false in parameters |
| `test_serialize_all_properties_required` | All property names in required array |
| `test_serialize_optional_becomes_required` | Optional props added to required for strict mode |

**Pattern to follow:** `tests/unit/providers/openai/request_chat_test.c`

---

## Phase 5: Commands

### commands.md

**Test file:** `tests/unit/commands/cmd_tool_test.c`

**Goals:**
- Test `/tool` command output
- Test `/tool NAME` command output
- Test `/refresh` command behavior

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_tool_list_all` | `/tool` lists all tools with descriptions |
| `test_tool_inspect_valid` | `/tool bash` shows full schema |
| `test_tool_inspect_unknown` | `/tool unknown` returns ERR_INVALID_ARG |
| `test_refresh_reloads_registry` | `/refresh` clears and repopulates registry |
| `test_refresh_reports_count` | `/refresh` output includes tool count |

**Mocking:**
- Create mock registry with known entries
- Override discovery for `/refresh` test

**Pattern to follow:** `tests/unit/commands/dispatch_test.c`

**Fixture setup:**
```c
static ik_repl_ctx_t *create_test_repl_with_registry(void *parent) {
    // Create REPL context
    // Create shared context with tool_registry
    // Populate registry with mock entries
}
```

---

## Phase 6: Async Optimization

### async-optimization.md

**Test file:** `tests/unit/tool_discovery/async_test.c`

**Goals:**
- Test async discovery primitives
- Test fd integration with select()
- Test completion detection

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_async_start_returns_state` | `ik_tool_discovery_start()` returns non-NULL state |
| `test_async_add_fds_populates_fdset` | `ik_tool_discovery_add_fds()` adds fds to set |
| `test_async_process_fds_handles_data` | `ik_tool_discovery_process_fds()` parses schemas |
| `test_async_is_complete_initially_false` | `ik_tool_discovery_is_complete()` returns false initially |
| `test_async_is_complete_after_finalize` | After all tools respond, returns true |
| `test_async_finalize_cleans_up` | `ik_tool_discovery_finalize()` frees resources |

**Mocking:**
- Create mock tools as shell scripts in temp directory
- Use pipe fds for testing fd operations

**Integration test:** `tests/integration/tool_discovery_async_test.c`

**Goals:**
- Test startup timing (terminal appears before discovery complete)
- Test submit-during-discovery behavior

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_startup_immediate` | REPL loop enters before discovery complete |
| `test_submit_waits_for_discovery` | LLM submit blocks until discovery done |

---

## Integration Tests

Beyond unit tests, create integration tests for end-to-end flows:

### tests/integration/tool_external_e2e_test.c

**Goals:**
- Test real tool execution through full stack
- Test LLM → tool dispatch → execution → response

**Test scenarios:**
| Test Name | Goal |
|-----------|------|
| `test_bash_tool_e2e` | Complete bash tool cycle with real execution |
| `test_file_read_e2e` | File read with temp file |
| `test_tool_not_found_e2e` | Unknown tool returns proper error envelope |

---

## Makefile Additions

For each new test file, add to appropriate pattern:

```makefile
# Unit tests auto-discovered via:
UNIT_TEST_SOURCES = $(wildcard tests/unit/*/*_test.c) $(wildcard tests/unit/*/*/*_test.c)

# Create directories as needed:
$(BUILDDIR)/tests/unit/tool_registry:
	mkdir -p $@

$(BUILDDIR)/tests/unit/tool_discovery:
	mkdir -p $@

$(BUILDDIR)/tests/unit/tool_external:
	mkdir -p $@

$(BUILDDIR)/tests/unit/tool_wrapper:
	mkdir -p $@
```

---

## Coverage Exclusions

New code should only use these exclusion markers:

```c
// LCOV_EXCL_BR_LINE  - OOM checks (if ptr == NULL) PANIC(...)
// LCOV_EXCL_LINE     - Impossible state PANIC (switch default)
```

Do NOT exclude:
- Normal error paths (return ERR(...))
- Validation errors
- I/O errors that can be triggered

---

## Test Execution Order

Tests should be written in this order within each task:

1. **Happy path** - Basic success case
2. **Error cases** - Each error code that can be returned
3. **Edge cases** - Empty input, boundary values
4. **Resource cleanup** - Memory leaks, fd leaks

Use `valgrind` via `make valgrind-check` to verify no leaks.
