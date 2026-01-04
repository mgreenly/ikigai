# Plan vs Codebase Gap Analysis

The plan files describe an external tool architecture that has not been implemented. The codebase uses an internal tool system with tools compiled into the main binary.

## Summary

| Category | Gap Count |
|----------|-----------|
| Missing files | 13 |
| Missing types/enums | 4 |
| Missing struct fields | 3 |
| Missing functions | 14 |
| Incorrect file paths | 3 |
| Unimplemented features | 2 |

---

## 1. File Path Errors

Plan files reference paths that don't exist or have moved.

### 1.1 OpenAI Client Path

**Plan references:** `src/openai/client.c`

**Actual:** File does not exist. OpenAI provider code is at:
- `src/providers/openai/openai.c`
- `src/providers/openai/request_chat.c`
- `src/providers/openai/request_responses.c`

**Affected files:**
- `cdd/plan/removal-specification.md`
- `cdd/plan/integration-specification.md`
- `cdd/plan/architecture.md`

**Fix:** Update all references to use `src/providers/openai/request_chat.c`

### 1.2 Tool Source Directory

**Plan references:** `src/tools/bash/main.c`, `src/tools/file_read/main.c`, etc.

**Actual:** Directory `src/tools/` does not exist. Current tool implementations are:
- `src/tool_bash.c`
- `src/tool_file_read.c`
- `src/tool_file_write.c`
- `src/tool_glob.c`
- `src/tool_grep.c`

**Affected files:**
- `cdd/plan/tool-specifications.md`
- `cdd/plan/tool-discovery-execution.md`

**Fix:** Plan describes future external tools, not current internal tools. Clarify this is future state.

### 1.3 OpenAI Multi Context

**Plan references:** `src/openai/client_multi.h`

**Actual:** File does not exist. Only a forward declaration exists in `src/agent.h`:
```c
struct ik_openai_multi;
```

**Affected files:**
- `cdd/plan/integration-specification.md`

**Fix:** Remove or update references to reflect actual architecture.

---

## 2. Missing Types and Enums

These types are referenced in the plan but don't exist in the codebase.

### 2.1 tool_scan_state_t

**Plan defines:**
```c
typedef enum {
    TOOL_SCAN_NOT_STARTED,
    TOOL_SCAN_IN_PROGRESS,
    TOOL_SCAN_COMPLETE,
    TOOL_SCAN_FAILED
} tool_scan_state_t;
```

**Actual:** Does not exist.

**Affected files:**
- `cdd/plan/architecture.md`
- `cdd/plan/tool-discovery-execution.md`

### 2.2 ik_tool_registry_t

**Plan defines:**
```c
typedef struct {
    ik_tool_registry_entry_t *entries;
    size_t count;
    size_t capacity;
} ik_tool_registry_t;
```

**Actual:** Does not exist.

**Affected files:**
- `cdd/plan/architecture.md`
- `cdd/plan/integration-specification.md`

### 2.3 ik_tool_registry_entry_t

**Plan defines:**
```c
typedef struct {
    char *name;
    char *path;
    yyjson_doc *schema_doc;
    yyjson_val *schema_root;
} ik_tool_registry_entry_t;
```

**Actual:** Does not exist.

**Affected files:**
- `cdd/plan/architecture.md`

### 2.4 ik_tool_discovery_state_t

**Plan references:** Opaque state for async discovery operations.

**Actual:** Does not exist.

**Affected files:**
- `cdd/plan/architecture.md`

---

## 3. Missing Struct Fields

### 3.1 ik_shared_ctx_t.tool_registry

**Plan says:** Add `ik_tool_registry_t *tool_registry` field.

**Actual struct** (src/shared.h):
```c
typedef struct ik_shared_ctx {
    ik_config_t *cfg;
    ik_logger_t *logger;
    ik_term_ctx_t *term;
    ik_render_ctx_t *render;
    ik_db_ctx_t *db_ctx;
    int64_t session_id;
    ik_history_t *history;
    ik_debug_pipe_manager_t *debug_mgr;
    ik_debug_pipe_t *openai_debug_pipe;
    ik_debug_pipe_t *db_debug_pipe;
    bool debug_enabled;
    atomic_bool fork_pending;
} ik_shared_ctx_t;
```

**Missing:** `tool_registry` field

**Affected files:**
- `cdd/plan/architecture.md`
- `cdd/plan/tool-discovery-execution.md`
- `cdd/plan/integration-specification.md`

### 3.2 ik_shared_ctx_t.tool_scan_state

**Plan says:** Add `tool_scan_state_t tool_scan_state` field.

**Actual:** Field does not exist.

**Affected files:**
- `cdd/plan/architecture.md`
- `cdd/plan/tool-discovery-execution.md`

### 3.3 ik_repl_ctx_t.tool_discovery

**Plan says:** Add `ik_tool_discovery_state_t *tool_discovery` field for Phase 6.

**Actual:** Field does not exist.

**Affected files:**
- `cdd/plan/architecture.md`

---

## 4. Missing Functions

### 4.1 Tool Registry Functions

| Function | Purpose | Exists |
|----------|---------|--------|
| `ik_tool_registry_create()` | Create registry | No |
| `ik_tool_registry_scan()` | Populate from directories | No |
| `ik_tool_registry_lookup()` | Find tool by name | No |
| `ik_tool_registry_build_all()` | Build schema array for LLM | No |

**Note:** `ik_tool_build_all()` exists but is different - it builds static schemas, not from registry.

### 4.2 Tool Discovery Functions

| Function | Purpose | Exists |
|----------|---------|--------|
| `ik_tool_discovery_run()` | Blocking discovery | No |
| `ik_tool_discovery_start()` | Start async discovery | No |
| `ik_tool_discovery_add_fds()` | Add to select() | No |
| `ik_tool_discovery_process_fds()` | Process ready fds | No |
| `ik_tool_discovery_is_complete()` | Check completion | No |
| `ik_tool_discovery_finalize()` | Cleanup | No |

### 4.3 Tool Execution Functions

| Function | Purpose | Exists |
|----------|---------|--------|
| `ik_tool_external_exec()` | Execute external tool | No |
| `ik_tool_wrap_success()` | Wrap successful result | No |
| `ik_tool_wrap_failure()` | Wrap failed result | No |

### 4.4 Command Functions

| Function | Purpose | Exists |
|----------|---------|--------|
| `ik_cmd_tool()` | /tool command | No |
| `ik_cmd_refresh()` | /refresh command | No |

---

## 5. Missing Files

### 5.1 New Source Files (to be created)

| File | Purpose |
|------|---------|
| `src/tool_registry.c` | Dynamic tool registry |
| `src/tool_registry.h` | Registry header |
| `src/tool_discovery.c` | External tool discovery |
| `src/tool_discovery.h` | Discovery header |
| `src/tool_external.c` | External tool execution |
| `src/tool_external.h` | External execution header |
| `src/tool_wrapper.c` | Response envelope wrapping |
| `src/tool_wrapper.h` | Wrapper header |
| `src/commands_tool.c` | /tool and /refresh commands |
| `src/commands_tool.h` | Command header |

### 5.2 External Tool Binaries (to be created)

| Directory | Binary |
|-----------|--------|
| `src/tools/bash/main.c` | `libexec/ikigai/bash` |
| `src/tools/file_read/main.c` | `libexec/ikigai/file-read` |
| `src/tools/file_write/main.c` | `libexec/ikigai/file-write` |
| `src/tools/file_edit/main.c` | `libexec/ikigai/file-edit` |
| `src/tools/glob/main.c` | `libexec/ikigai/glob` |
| `src/tools/grep/main.c` | `libexec/ikigai/grep` |

### 5.3 Build Output Directory

`libexec/ikigai/` does not exist.

---

## 6. Removal Specification Status

Files marked for deletion in `removal-specification.md` still exist:

| File | Status | Should Be |
|------|--------|-----------|
| `src/tool_dispatcher.c` | EXISTS | Delete in Phase 3 |
| `src/tool_bash.c` | EXISTS | Delete in Phase 3 |
| `src/tool_file_read.c` | EXISTS | Delete in Phase 3 |
| `src/tool_file_write.c` | EXISTS | Delete in Phase 3 |
| `src/tool_glob.c` | EXISTS | Delete in Phase 3 |
| `src/tool_grep.c` | EXISTS | Delete in Phase 3 |
| `src/tool.c` | EXISTS | Delete in Phase 3 |
| `src/tool_response.c` | EXISTS | Delete in Phase 3 |

**Note:** This is expected - Phase 3 (removal) hasn't been executed yet.

---

## 7. Unimplemented Features

### 7.1 file_edit Tool

**Plan:** Complete specification in `tool-specifications.md` with schema, behavior, and error cases.

**Actual:** No `tool_file_edit.c` exists. The file_edit tool was never implemented, even as an internal tool.

**Impact:**
- `removal-specification.md` doesn't list it for removal (nothing to remove)
- Must be implemented as external tool directly

### 7.2 Makefile Tool Targets

**Plan:** Makefile should have:
- `TOOLS` variable
- `TOOL_TARGETS` variable
- Individual targets: `tool-bash`, `tool-file-read`, etc.
- Build rules for `libexec/ikigai/*`

**Actual:** None of these exist. Tools are compiled into main binary.

---

## 8. Architecture Mismatch

### Current Architecture (Internal Tools)

```
src/tool.c           → Static schema definitions
src/tool_dispatcher.c → Route calls to internal handlers
src/tool_bash.c      → Direct C implementation
src/tool_file_read.c → Direct C implementation
...
```

- Tools compiled into `bin/ikigai`
- Schemas hardcoded in `src/providers/request_tools.c`
- Execution via `ik_tool_dispatch()` (direct C call)

### Planned Architecture (External Tools)

```
src/tool_registry.c   → Dynamic registry from discovery
src/tool_discovery.c  → Scan directories, call --schema
src/tool_external.c   → Fork/exec with JSON I/O
libexec/ikigai/bash   → Standalone binary
libexec/ikigai/...    → Standalone binaries
```

- Tools as separate executables
- Schemas from `--schema` flag
- Execution via fork/exec with pipes

---

## 9. Recommended Plan Updates

### 9.1 Fix File Paths

Update all references to `src/openai/client.c` → `src/providers/openai/request_chat.c`

### 9.2 Update architecture-current.md

The current tool system reference may be outdated. Verify it matches actual code.

### 9.3 Clarify Phase Status

Add a note that:
- Phase 1-2 (external tool binaries): NOT STARTED
- Phase 3 (removal): NOT STARTED
- Phase 4 (sync infrastructure): NOT STARTED
- Phase 5 (commands): NOT STARTED
- Phase 6 (async): NOT STARTED

### 9.4 Add file_edit to Phase 1-2

Since file_edit was never implemented internally, it should be created directly as an external tool in Phase 2.

### 9.5 Update Integration Points

The integration-specification.md references `ik_openai_serialize_request()` which doesn't exist. Update to reference actual functions:
- `ik_openai_serialize_chat_request()`
- `ik_request_add_tool()`

---

## 10. Resolution Priority

| Priority | Gap | Impact |
|----------|-----|--------|
| High | File path errors | Tasks will look for wrong files |
| High | Integration point references | Tasks will modify wrong functions |
| Medium | Missing types documentation | Tasks need accurate type specs |
| Low | Phase status clarity | Informational only |
