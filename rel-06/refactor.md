# Refactoring Recommendations for ikigai

**Date:** 2025-12-18
**Analysis Depth:** Ultrathink with 5 parallel Opus agents
**Scope:** Comprehensive codebase analysis for consistency, flexibility, and ease of development

## Executive Summary

After deep analysis of initialization patterns, error handling, module boundaries, naming conventions, and code duplication across the entire `src/` directory, we identified 5 high-impact refactoring opportunities. These recommendations balance architectural improvements with practical implementation concerns.

**Key Findings:**
- Overall code quality is **high** - error handling is exemplary, DI principles mostly followed
- Main issue: **`ik_agent_ctx_t` god object** (35+ fields mixing 6 concerns)
- **350+ lines of tool system duplication** present high-value refactoring opportunity
- **21 public functions** lack `ik_` namespace prefix
- Initialization patterns are inconsistent across modules
- Static function ban may be counterproductive

---

## Top 5 Refactoring Recommendations

### 1. Decompose `ik_agent_ctx_t` God Object ⚠️ CRITICAL

**Priority:** Highest architectural impact
**Effort:** High
**Risk:** High
**Value:** ⭐⭐⭐ Consistency, ⭐⭐⭐ Flexibility, ⭐⭐⭐ Ease of Development

#### The Problem

`ik_agent_ctx_t` (src/agent.h:54-120) violates Single Responsibility Principle with **35+ fields** mixing **6 distinct concerns**:

```c
typedef struct ik_agent_ctx {
    // 1. Identity (5 fields)
    char *uuid;
    char *name;
    char *parent_uuid;
    int64_t created_at;
    int64_t fork_message_id;

    // 2. Display State (11 fields)
    ik_scrollback_t *scrollback;
    ik_layer_cake_t *layer_cake;
    ik_layer_t *scrollback_layer;
    ik_layer_t *spinner_layer;
    ik_layer_t *separator_layer;
    ik_layer_t *input_layer;
    ik_layer_t *completion_layer;
    size_t viewport_offset;
    ik_spinner_state_t spinner_state;
    bool separator_visible;
    bool input_buffer_visible;

    // 3. Input State (4 fields)
    ik_input_buffer_t *input_buffer;
    ik_completion_t *completion;
    const char *input_text;
    size_t input_text_len;

    // 4. Conversation State (3 fields)
    ik_openai_conversation_t *conversation;
    ik_mark_t **marks;
    size_t mark_count;

    // 5. LLM Interaction (8 fields)
    struct ik_openai_multi *multi;
    int curl_still_running;
    ik_agent_state_t state;
    char *assistant_response;
    char *streaming_line_buffer;
    char *http_error_message;
    char *response_model;
    char *response_finish_reason;
    int32_t response_completion_tokens;

    // 6. Tool Execution (7 fields)
    ik_tool_call_t *pending_tool_call;
    pthread_t tool_thread;
    pthread_mutex_t tool_thread_mutex;
    bool tool_thread_running;
    bool tool_thread_complete;
    TALLOC_CTX *tool_thread_ctx;
    char *tool_thread_result;
    int32_t tool_iteration_count;

    // References
    ik_shared_ctx_t *shared;
    struct ik_repl_ctx_t *repl;
} ik_agent_ctx_t;
```

**Violations:**
- **Bounded Context Violation (DDD):** Mixes Terminal UI Context and LLM Integration Context
- **Single Responsibility:** Agent struct has 6 distinct reasons to change
- **Testing Complexity:** Mocking 35 fields for unit tests is painful
- **Cognitive Load:** Must understand all 6 concerns to modify any one

#### Recommended Refactoring

Split into focused, cohesive structs:

```c
// src/agent_identity.h
typedef struct {
    char *uuid;
    char *name;
    char *parent_uuid;
    int64_t created_at;
    int64_t fork_message_id;
} ik_agent_identity_t;

// src/agent_display.h
typedef struct {
    ik_scrollback_t *scrollback;
    ik_layer_cake_t *layer_cake;
    ik_layer_t *layers[5];  // Array instead of individual pointers
    size_t viewport_offset;
    ik_spinner_state_t spinner;
    bool separator_visible;
    bool input_buffer_visible;
} ik_agent_display_t;

// src/agent_llm.h
typedef struct {
    struct ik_openai_multi *multi;
    int curl_still_running;
    ik_agent_state_t state;
    char *response_buffer;
    char *streaming_line_buffer;
    char *http_error_message;
    char *response_model;
    char *response_finish_reason;
    int32_t response_completion_tokens;
} ik_agent_llm_t;

// src/agent_tool.h
typedef struct {
    ik_tool_call_t *pending;
    pthread_t thread;
    pthread_mutex_t mutex;
    bool running;
    bool complete;
    TALLOC_CTX *ctx;
    char *result;
    int32_t iteration_count;
} ik_agent_tool_executor_t;

// src/agent.h - Slim composition
typedef struct ik_agent_ctx {
    ik_shared_ctx_t *shared;
    struct ik_repl_ctx_t *repl;

    // Focused sub-contexts (owned)
    ik_agent_identity_t *identity;
    ik_agent_display_t *display;
    ik_agent_llm_t *llm;
    ik_agent_tool_executor_t *tool_executor;

    // Remaining state
    ik_input_buffer_t *input_buffer;
    ik_completion_t *completion;
    ik_openai_conversation_t *conversation;
    ik_mark_t **marks;
    size_t mark_count;
} ik_agent_ctx_t;
```

#### Benefits

- **Testability:** Mock only the concern being tested
- **Clarity:** Each struct has single, clear purpose
- **Flexibility:** Swap display implementation without touching LLM state
- **Bounded Contexts:** Clean separation per DDD principles
- **Future-proofing:** Enables multi-provider support (Claude, Gemini)

#### Implementation Strategy

1. Create new header files for sub-contexts
2. Add factory functions for each sub-context
3. Update `ik_agent_create()` to compose sub-contexts
4. Migrate callers incrementally (search for `agent->` field access)
5. Run `make check` after each concern migration

#### Why This is #1

This is the **core architectural issue** blocking:
- Clean bounded context separation
- Independent testing of concerns
- Future multi-provider support
- Tool execution refactoring

---

### 2. Eliminate Tool System Code Duplication 🎯 HIGH VALUE

**Priority:** High value, low risk
**Effort:** Medium
**Risk:** Low
**Value:** ⭐⭐⭐ Consistency, ⭐⭐⭐ Flexibility, ⭐⭐⭐ Ease of Development

#### The Problem

**Two major duplication patterns** across the tool system:

##### Pattern 1: Schema Building Duplication (200+ lines)

Five nearly identical functions in `src/tool.c`:
- `ik_tool_build_glob_schema()` (lines 62-97)
- `ik_tool_build_file_read_schema()` (lines 100-135)
- `ik_tool_build_file_write_schema()` (lines 138-173)
- `ik_tool_build_bash_schema()` (lines 176-211)
- `ik_tool_build_grep_schema()` (lines 214-249)

Each follows identical structure:

```c
yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc)
{
    assert(doc != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *schema = yyjson_mut_obj(doc);
    if (schema == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, schema, "type", "function")) PANIC("Failed");

    yyjson_mut_val *function = yyjson_mut_obj(doc);
    if (function == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, function, "name", "glob")) PANIC("Failed");
    if (!yyjson_mut_obj_add_str(doc, function, "description", "Find files matching a glob pattern")) PANIC("Failed");

    yyjson_mut_val *properties = yyjson_mut_obj(doc);
    if (properties == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_tool_add_string_param(doc, properties, "pattern", "Glob pattern (e.g., 'src/**/*.c')");
    ik_tool_add_string_param(doc, properties, "path", "Base directory (default: cwd)");

    yyjson_mut_val *parameters = yyjson_mut_obj(doc);
    if (parameters == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, parameters, "type", "object")) PANIC("Failed");
    if (!yyjson_mut_obj_add_val(doc, parameters, "properties", properties)) PANIC("Failed");

    yyjson_mut_val *required = yyjson_mut_arr(doc);
    if (required == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_arr_add_str(doc, required, "pattern")) PANIC("Failed");
    if (!yyjson_mut_obj_add_val(doc, parameters, "required", required)) PANIC("Failed");
    if (!yyjson_mut_obj_add_val(doc, function, "parameters", parameters)) PANIC("Failed");
    if (!yyjson_mut_obj_add_val(doc, schema, "function", function)) PANIC("Failed");

    return schema;
}
```

**Only differences:** Function name, tool name string, description string, parameter definitions.

##### Pattern 2: Response Building Duplication (150+ lines)

Same JSON envelope construction repeated in 5+ tool implementations:

**Files affected:**
- `src/tool_grep.c` (lines 83-119)
- `src/tool_glob.c` (lines 54-128)
- `src/tool_bash.c` (lines 75-108)
- `src/tool_file_read.c` (lines 65-102)
- `src/tool_file_write.c` (lines 68-104)
- `src/tool_response.c` (3 instances)

```c
static char *build_grep_success(void *parent, const char *output, size_t count)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);

    yyjson_mut_val *data = yyjson_mut_obj(doc);
    if (data == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // ONLY THESE TWO LINES DIFFER PER TOOL
    yyjson_mut_obj_add_str(doc, data, "output", output);
    yyjson_mut_obj_add_uint(doc, data, "count", count);
    // END UNIQUE SECTION

    yyjson_mut_obj_add_val(doc, root, "data", data);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}
```

#### Recommended Refactoring

##### Part 1: Data-Driven Schema Builder

```c
// src/tool.h - New API
typedef struct {
    const char *name;
    const char *description;
    bool required;
} ik_tool_param_def_t;

typedef struct {
    const char *name;
    const char *description;
    const ik_tool_param_def_t *params;
    size_t param_count;
} ik_tool_schema_def_t;

// Single implementation replaces all 5 build_*_schema functions
yyjson_mut_val *ik_tool_build_schema_from_def(yyjson_mut_doc *doc,
                                                const ik_tool_schema_def_t *def);
```

```c
// src/tool.c - Declarative schema definitions
static const ik_tool_param_def_t glob_params[] = {
    {"pattern", "Glob pattern (e.g., 'src/**/*.c')", true},
    {"path", "Base directory (default: cwd)", false}
};

static const ik_tool_schema_def_t glob_schema = {
    .name = "glob",
    .description = "Find files matching a glob pattern",
    .params = glob_params,
    .param_count = 2
};

static const ik_tool_param_def_t file_read_params[] = {
    {"file_path", "Path to file to read", true},
    {"offset", "Line number to start reading from", false},
    {"limit", "Number of lines to read", false}
};

static const ik_tool_schema_def_t file_read_schema = {
    .name = "file_read",
    .description = "Read contents of a file",
    .params = file_read_params,
    .param_count = 3
};

// And so on for other tools...

// Usage
yyjson_mut_val *ik_tool_build_glob_schema(yyjson_mut_doc *doc) {
    return ik_tool_build_schema_from_def(doc, &glob_schema);
}
```

##### Part 2: Response Builder with Callback

```c
// src/tool_response.h - New API
typedef void (*ik_tool_data_fn)(yyjson_mut_doc *doc, yyjson_mut_val *data, void *ctx);

char *ik_tool_response_success(void *parent, ik_tool_data_fn add_data, void *ctx);
char *ik_tool_response_error(void *parent, const char *message);
```

```c
// src/tool_response.c - Single implementation
char *ik_tool_response_success(void *parent, ik_tool_data_fn add_data, void *ctx)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);

    if (add_data != NULL) {
        yyjson_mut_val *data = yyjson_mut_obj(doc);
        if (data == NULL) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        add_data(doc, data, ctx);  // Callback adds tool-specific fields

        yyjson_mut_obj_add_val(doc, root, "data", data);
    }

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}
```

```c
// src/tool_grep.c - Clean usage
typedef struct {
    const char *output;
    size_t count;
} grep_data_t;

static void add_grep_data(yyjson_mut_doc *doc, yyjson_mut_val *data, void *ctx)
{
    grep_data_t *d = ctx;
    yyjson_mut_obj_add_str(doc, data, "output", d->output);
    yyjson_mut_obj_add_uint(doc, data, "count", d->count);
}

res_t ik_tool_exec_grep(void *parent, const char *pattern,
                         const char *glob_filter, const char *path)
{
    // ... execute grep logic ...

    grep_data_t data = {.output = output, .count = count};
    return OK(ik_tool_response_success(parent, add_grep_data, &data));
}
```

#### Benefits

- **Eliminates 350+ lines** of boilerplate
- **Adding new tools** becomes trivial - define schema struct, implement exec, done
- **Consistency** - all tool responses have identical structure
- **Testing** - uniform test patterns for all tools
- **Maintenance** - bug fixes to response format in one place

#### Impact on Adding New Tools

**Before (current):**
1. Copy-paste 35 lines of schema building code
2. Change name/description/params
3. Copy-paste 20 lines of response building code
4. Change data fields
5. Easy to introduce inconsistencies

**After (proposed):**
1. Define schema struct (5 lines)
2. Implement exec function
3. Define data callback (3 lines)
4. Done - guaranteed consistency

---

### 3. Fix Public API Namespace Pollution ⚠️ CONSISTENCY

**Priority:** Foundation for other work
**Effort:** Low (mechanical)
**Risk:** Low
**Value:** ⭐⭐⭐ Consistency, ⭐ Flexibility, ⭐⭐ Ease of Development

#### The Problem

**21 public functions** in headers violate the core naming principle from `project/naming.md`:

> "All public symbols follow: `ik_MODULE_THING`"

#### Violations by File

##### src/repl_event_handlers.h (10 violations)

```c
// Current (WRONG)
long calculate_select_timeout_ms(...);
res_t setup_fd_sets(...);
res_t handle_terminal_input(...);
res_t handle_curl_events(...);
void handle_agent_request_success(...);
void handle_tool_completion(...);
void handle_agent_tool_completion(...);
res_t calculate_curl_min_timeout(...);
res_t handle_select_timeout(...);
res_t poll_tool_completions(...);

// Should be
long ik_repl_calculate_select_timeout_ms(...);
res_t ik_repl_setup_fd_sets(...);
res_t ik_repl_handle_terminal_input(...);
res_t ik_repl_handle_curl_events(...);
void ik_repl_handle_agent_request_success(...);
void ik_repl_handle_tool_completion(...);
void ik_repl_handle_agent_tool_completion(...);
res_t ik_repl_calculate_curl_min_timeout(...);
res_t ik_repl_handle_select_timeout(...);
res_t ik_repl_poll_tool_completions(...);
```

##### src/commands.h (8 violations)

```c
// Current (WRONG)
res_t cmd_fork(...);
res_t cmd_kill(...);
res_t cmd_send(...);
res_t cmd_check_mail(...);
res_t cmd_read_mail(...);
res_t cmd_delete_mail(...);
res_t cmd_filter_mail(...);
res_t cmd_agents(...);

// Should be
res_t ik_cmd_fork(...);
res_t ik_cmd_kill(...);
res_t ik_cmd_send(...);
res_t ik_cmd_check_mail(...);
res_t ik_cmd_read_mail(...);
res_t ik_cmd_delete_mail(...);
res_t ik_cmd_filter_mail(...);
res_t ik_cmd_agents(...);
```

##### Other violations

```c
// src/config.h
res_t expand_tilde(...);  // → ik_cfg_expand_tilde

// src/repl.h
void update_nav_context(...);  // → ik_repl_update_nav_context

// src/openai/client.h
ik_msg_t *get_message_at_index(...);  // → ik_openai_get_message_at_index

// src/openai/client_multi_callbacks.h
size_t http_write_callback(...);  // → ik_openai_http_write_callback
```

#### Additional Naming Issues

##### Abbreviation Violations

From `project/naming.md` - these words **must not** be abbreviated:
- handler, manager, server, client
- function, parameter, shutdown, payload

**Violations found:**

```c
// src/debug_pipe.h
res_t ik_debug_mgr_create(...);         // → ik_debug_manager_create
res_t ik_debug_mgr_add_pipe(...);       // → ik_debug_manager_add_pipe
void ik_debug_mgr_add_to_fdset(...);    // → ik_debug_manager_add_to_fdset
res_t ik_debug_mgr_handle_ready(...);   // → ik_debug_manager_handle_ready

// src/shared.h
ik_debug_pipe_manager_t *debug_mgr;     // → debug_manager

// src/tool.c
void ik_tool_add_string_param(...);     // → ik_tool_add_string_parameter

// src/layer.h (function pointer types)
ik_layer_is_visible_fn                  // → _callback or _handler
ik_layer_get_height_fn
ik_layer_render_fn
```

#### Impact

- **Symbol collision risk** - `handle_terminal_input` could clash with other libraries
- **API inconsistency** - Violates ubiquitous language principle (DDD)
- **IDE pollution** - Unnamespaced symbols clutter global autocomplete
- **Greppability** - Hard to find "all REPL event handlers" with `grep "^ik_repl_"`
- **Documentation** - Unclear module ownership

#### Recommended Refactoring

Mechanical find/replace across codebase - low risk:

```bash
# Automated script approach
for old_name in calculate_select_timeout_ms setup_fd_sets handle_terminal_input; do
    new_name="ik_repl_${old_name}"
    git grep -l "${old_name}" | xargs sed -i "s/\b${old_name}\b/${new_name}/g"
done

# Then run make check to verify
make clean && make check
```

#### Why This is #3

- **Low risk, high consistency win**
- **Foundation for API stability** - do this before documenting public API
- **Quick wins** build momentum for larger refactorings

---

### 4. Standardize Initialization Patterns 📐 CLARITY

**Priority:** Reduces cognitive load
**Effort:** Medium
**Risk:** Medium
**Value:** ⭐⭐⭐ Consistency, ⭐⭐ Flexibility, ⭐⭐⭐ Ease of Development

#### The Problem

Analysis of all `_init` and `_create` functions revealed **inconsistent conventions** that force developers to remember special cases rather than following patterns.

##### Inconsistency 1: Return Type Confusion

Some functions return `res_t` but **only ever PANIC** (never return ERR):

```c
// src/openai/client.c:36 - Returns res_t but only PANICs
res_t ik_openai_conversation_create(void *parent)
{
    ik_openai_conversation_t *conv = talloc_zero(parent, ik_openai_conversation_t);
    if (!conv) PANIC("Failed to allocate conversation");  // Only failure mode
    return OK(conv);  // Why res_t?
}

// src/openai/client_msg.c:20 - Same problem
res_t ik_openai_msg_create(void *parent, const char *role, const char *content)
{
    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) PANIC("Failed to allocate message");  // Only failure mode
    return OK(msg);  // Should return pointer directly
}
```

Yet in the **same module** (client_msg.c):

```c
// src/openai/client_msg.c:45 - CORRECT pattern
ik_msg_t *ik_openai_msg_create_tool_call(void *parent, ...)
{
    ik_msg_t *msg = talloc_zero(parent, ik_msg_t);
    if (!msg) PANIC("Failed to allocate message");
    return msg;  // Direct pointer - correct!
}
```

**Confusion:** Same failure mode (OOM → PANIC), different return types.

##### Inconsistency 2: TALLOC_CTX Parameter Naming

Three different names for the same concept:

```c
// Pattern A: TALLOC_CTX *ctx
res_t ik_array_create(TALLOC_CTX *ctx, size_t element_size, size_t increment);
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared, ...);
res_t ik_db_init(TALLOC_CTX *mem_ctx, const char *conn_str, ...);  // mem_ctx variant

// Pattern B: void *parent
ik_scrollback_t *ik_scrollback_create(void *parent, int32_t terminal_width);
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out);
```

**Modules by naming convention:**

| Module | Parameter Name | Type |
|--------|---------------|------|
| array.c | `TALLOC_CTX *ctx` | Strong typed |
| agent.c | `TALLOC_CTX *ctx` | Strong typed |
| db/*.c | `TALLOC_CTX *mem_ctx` | Strong typed (variant) |
| layer.c | `TALLOC_CTX *ctx` | Strong typed |
| scrollback.c | `void *parent` | Weak typed |
| terminal.c | `void *parent` | Weak typed |
| render.c | `void *parent` | Weak typed |

##### Inconsistency 3: Breaking the DI Pattern

```c
// src/marks.h:20 - Takes repl_ctx instead of TALLOC_CTX (line 44 of marks.c)
res_t ik_mark_create(ik_repl_ctx_t *repl, const char *label);
```

**Problems:**
- First parameter is not a talloc context (violates convention)
- Function accesses `repl->current->conversation` internally (tight coupling)
- Can't create marks independently of REPL (violates DI principles)

**Should be:**
```c
res_t ik_mark_create(TALLOC_CTX *ctx, ik_openai_conversation_t *conv,
                     const char *label, ik_mark_t **out);
```

##### Inconsistency 4: db/session.c Parameter Order

```c
// src/db/session.h:18
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
```

**Problem:** First parameter is **not a TALLOC_CTX** - breaks the universal convention.

#### Ideal Pattern Documentation

Based on analysis, the codebase **should** follow these rules:

```c
// Rule 1: Return Pattern Decision Tree
// - Can fail with meaningful error (I/O, validation, parse)? → res_t + output param
// - Can only fail on OOM? → direct pointer + PANIC

// Rule 2: For failable operations (I/O, external dependencies)
res_t ik_module_init(TALLOC_CTX *ctx,           // Always first
                     /* config params */,        // Input parameters
                     ik_module_t **out);         // Always last

// Rule 3: For pure allocations (only OOM can fail)
ik_module_t *ik_module_create(TALLOC_CTX *ctx,  // Always first
                              /* params */);     // Other parameters

// Rule 4: Cleanup (when needed for external resources)
void ik_module_cleanup(ik_module_t *module);    // Supplement talloc, not replace
```

**Naming Rules:**
- TALLOC_CTX always named `ctx` (not `parent`, not `mem_ctx`, not `void *`)
- Output parameter always named `out` or `TYPE_out`
- Use `_init` for complex initialization with I/O
- Use `_create` for simple allocation
- Use `_cleanup` for explicit resource release (rare - talloc handles most)

#### Current Compliance

| Pattern | Compliant Modules | Violations |
|---------|------------------|------------|
| Return semantics | array, agent, terminal, db/connection, render, scrollback, layer, history | openai/client, openai/client_msg, marks |
| TALLOC_CTX first | array, agent, db, layer, terminal, render, scrollback | db/session (first param is db_ctx) |
| Consistent naming | array, agent, layer | scrollback, terminal, render (use `parent`), db (uses `mem_ctx`) |

**Estimated compliance:** ~85% of modules follow the ideal pattern.

#### Recommended Standardization

##### Phase 1: Fix Return Type Semantics

```c
// Change these from res_t to direct pointer (only PANIC, never ERR)
ik_openai_conversation_t *ik_openai_conversation_create(TALLOC_CTX *ctx);
ik_msg_t *ik_openai_msg_create(TALLOC_CTX *ctx, const char *role, const char *content);
```

##### Phase 2: Standardize TALLOC_CTX Naming

```c
// Rename all `void *parent` to `TALLOC_CTX *ctx`
ik_scrollback_t *ik_scrollback_create(TALLOC_CTX *ctx, int32_t terminal_width);
res_t ik_term_init(TALLOC_CTX *ctx, ik_term_ctx_t **out);

// Rename `mem_ctx` variants to `ctx`
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out_ctx);
```

##### Phase 3: Fix DI Violations

```c
// Decouple marks from REPL
res_t ik_mark_create(TALLOC_CTX *ctx,
                     ik_openai_conversation_t *conv,
                     const char *label,
                     ik_mark_t **out);

// Decouple db/session from db context (return res_t with session object)
res_t ik_db_session_create(TALLOC_CTX *ctx,
                           ik_db_ctx_t *db,
                           ik_db_session_t **out);
```

#### Benefits

- **Reduced cognitive load** - one pattern to remember
- **Better error semantics** - `res_t` means "check this", pointer means "only OOM"
- **Consistent DI** - all modules follow same dependency pattern
- **Easier onboarding** - new contributors learn one convention
- **Better testability** - consistent mocking points

---

### 5. Reconsider the Static Function Ban 💭 CONTROVERSIAL

**Priority:** Enables better abstractions
**Effort:** Low (policy change)
**Risk:** Low
**Value:** ⭐⭐ Consistency, ⭐⭐ Flexibility, ⭐⭐⭐ Ease of Development

#### The Current Rule (from .agents/skills/style.md)

> **Avoid Static Functions**
>
> Do not use `static` helper functions in implementation files. Instead, inline the code directly.
>
> **Why:** LCOV exclusion markers (`LCOV_EXCL_BR_LINE`) on PANIC/assert calls inside static functions are not reliably honored, breaking 100% branch coverage requirements.
>
> **Exception:** MOCKABLE wrapper functions (see `wrapper.h`) - these use static functions by design for the mocking interface.

#### The Reality: 50+ Static Functions Exist

The rule is **not being followed**, suggesting it may be counterproductive:

**Violations by file:**
- `src/commands.c`: 5 static command handlers (cmd_clear, cmd_help, cmd_model, etc.)
- `src/tool_grep.c`: 3 static helpers (build_grep_success, etc.)
- `src/tool_glob.c`: Similar pattern
- `src/layer_*.c`: All use static callback implementations
- `src/history.c`: 2 static parsing functions
- `src/logger.c`: 6 static helpers (ik_log_format_timestamp, etc.)
- `src/repl_actions_llm.c`: Static helper functions
- `src/config.c`: 1 static function

**Observation:** The functions exist anyway, often with LCOV exclusions that work fine.

#### The Duplication Cost

Banning abstractions leads to **repeated patterns**. Example from db/*.c, history.c, event_render.c (30+ repetitions):

```c
// Repeated in: db/migration.c, db/connection.c, db/agent.c, db/message.c,
//              db/session.c, db/replay.c, history.c, event_render.c
TALLOC_CTX *tmp = talloc_new(NULL);
if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
// ... work with tmp ...
talloc_free(tmp);
```

**Could be:**
```c
static TALLOC_CTX *tmp_ctx_create(void)
{
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return tmp;
}

// Usage
TALLOC_CTX *tmp = tmp_ctx_create();
// ... work ...
talloc_free(tmp);
```

**Or even better - scoped cleanup macro:**
```c
#define WITH_TMP_CTX(name) \
    for (TALLOC_CTX *name = tmp_ctx_create(); \
         name; \
         talloc_free(name), name = NULL)

// Usage
WITH_TMP_CTX(tmp) {
    // tmp automatically freed on scope exit
}
```

#### The Root Cause: LCOV Configuration

The static function ban treats the **symptom** (unreliable LCOV exclusions) rather than the **disease** (improper coverage configuration).

Modern LCOV (>= 1.14) **should** handle static function exclusions correctly. If it doesn't, the fix is:

##### Option 1: Function-Level Exclusions

```c
/* LCOV_EXCL_START */
static void helper_with_panics(void) {
    if (x == NULL) PANIC("...");  // No need for LCOV_EXCL_BR_LINE
}
/* LCOV_EXCL_STOP */
```

##### Option 2: Update LCOV Configuration

In Makefile, ensure `LCOV_EXCL_COVERAGE` handles static functions:

```makefile
LCOV_EXCL_COVERAGE := \
    --rc lcov_excl_br_line=LCOV_EXCL_BR_LINE \
    --rc lcov_excl_line=LCOV_EXCL_LINE \
    --rc lcov_excl_start=LCOV_EXCL_START \
    --rc lcov_excl_stop=LCOV_EXCL_STOP
```

##### Option 3: Inline Only Trivial Helpers

**Guideline:**
- **Inline:** 1-3 line helpers (no abstraction needed)
- **Static function:** Complex logic (>5 lines), especially if reused
- **Extract to module:** If used across multiple files

#### Recommended Policy Change

Replace blanket ban with **pragmatic guidelines**:

```markdown
## Static Functions

**Prefer static functions for:**
- Complex logic (>5 lines) repeated within a file
- Named code blocks that improve readability
- Helper callbacks (layer system, command handlers)

**Inline code when:**
- Helper is 1-3 lines
- Only used once
- No PANIC/assert inside

**Coverage handling:**
- Use function-level exclusions (LCOV_EXCL_START/STOP) for helpers with PANIC
- Ensure LCOV configuration handles static functions correctly
- Test via public API - static helpers don't need direct testing
```

#### Benefits of Allowing Static Functions

- **Reduce duplication** - Abstract common patterns (tmp context, error handling)
- **Improve readability** - Name complex code blocks
- **Enable testing** - Static helpers testable via public API
- **Maintain coverage** - With proper LCOV configuration
- **Standard C practice** - Static functions are idiomatic C

#### Why This is #5

- **Lowest priority** - doesn't block other work
- **Controversial** - challenges existing policy
- **Enables future refactoring** - makes #2 (tool abstractions) cleaner
- **Quality of life** - reduces developer frustration with duplication

---

## Implementation Priority Matrix

| Refactoring | Consistency | Flexibility | Ease of Dev | Effort | Risk | Lines Saved |
|-------------|-------------|-------------|-------------|--------|------|-------------|
| 1. Split agent god object | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | High | High | ~0 (restructure) |
| 2. Tool abstractions | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | Medium | Low | ~350 |
| 3. Fix namespace pollution | ⭐⭐⭐ | ⭐ | ⭐⭐ | Low | Low | ~0 (rename) |
| 4. Standardize init patterns | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Medium | Medium | ~0 (clarify) |
| 5. Reconsider static ban | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Low | Low | ~200 (future) |

## Recommended Implementation Order

### Phase 1: Foundation (Low Risk)
1. **#3 - Fix namespace pollution** (1-2 days)
   - Mechanical refactoring
   - Low risk
   - Establishes clean API surface

2. **#5 - Update static function policy** (discussion + doc update)
   - No code changes initially
   - Enables better abstractions in later phases

### Phase 2: High Value Wins
3. **#2 - Tool abstractions** (3-5 days)
   - Data-driven schema builder
   - Response builder with callbacks
   - Immediate value: easier to add new tools
   - Demonstrates pattern for other systems

### Phase 3: Standardization
4. **#4 - Standardize init patterns** (5-7 days)
   - Fix return type semantics (openai module)
   - Standardize TALLOC_CTX naming
   - Fix DI violations (marks, db/session)

### Phase 4: Architecture (High Impact, High Risk)
5. **#1 - Split agent god object** (10-15 days)
   - Requires stable foundation from phases 1-3
   - Create sub-context structs
   - Migrate incrementally
   - Run `make check` after each concern migration

**Total estimated time:** 20-30 days for all 5 refactorings

---

## Additional Findings from Deep Analysis

### What's Working Well ✅

1. **Error handling is exemplary**
   - Consistent ERR() macro usage with actionable messages
   - Proper handling of "error context lifetime trap" using talloc_steal
   - Clear separation: res_t (recoverable) vs assert (preconditions) vs PANIC (unrecoverable)
   - No violations of error handling patterns found

2. **No circular dependencies**
   - Clean acyclic dependency graph
   - Effective use of forward declarations
   - Well-defined module boundaries (mostly)

3. **Dependency Injection compliance: 8/10**
   - Most modules follow DI principles
   - Global state minimal and justified (panic handler, logger migration)
   - Composition root pattern in main()

4. **Talloc ownership is consistent**
   - Clear parent-child relationships
   - Destructors used appropriately for external resources
   - Borrowed vs owned references well-documented

### Lower Priority Issues 📋

#### Database Row Extraction Duplication

`src/db/agent.c` has **35-line field extraction block copy-pasted 4 times** (lines 143-181, 233-272, 331-369, 423-460):

```c
row->uuid = talloc_strdup(row, PQgetvalue_(res, i, 0));
if (row->uuid == NULL) PANIC("Out of memory");

if (!PQgetisnull(res, i, 1)) {
    row->name = talloc_strdup(row, PQgetvalue_(res, i, 1));
    if (row->name == NULL) PANIC("Out of memory");
} else {
    row->name = NULL;
}
// ... repeated for 7 fields
```

**Solution:** Extract helper:
```c
res_t ik_db_extract_agent_row(TALLOC_CTX *ctx, PGresult *res, int row_idx,
                               ik_db_agent_row_t **out);
```

#### CRLF Conversion Duplication

Same `\n` to `\r\n` conversion in 4 layer files (layer_input.c, layer_scrollback.c):

```c
if (text[i] == '\n') {
    ik_output_buffer_append(output, "\r\n", 2);
} else {
    ik_output_buffer_append(output, &text[i], 1);
}
```

**Solution:** Add helper:
```c
void ik_output_buffer_append_crlf(ik_output_buffer_t *buf, const char *text, size_t len);
```

#### Include Order Violations

style.md requires: Own header first → project headers (alpha) → system headers (alpha)

**Violations:**
- `src/db/message.c:14` - vendor/yyjson mixed with system headers
- `src/openai/client.c:14` - vendor/yyjson after system headers

Should be treated as project headers, not system headers.

#### Missing Abstractions

**Message Iterator:**
```c
// Repeated 4+ times
for (size_t i = 0; i < conv->message_count; i++) {
    ik_msg_t *msg = conv->messages[i];
    // ... process message
}
```

**Could be:**
```c
void ik_conversation_foreach(ik_openai_conversation_t *conv,
                             void (*callback)(ik_msg_t *msg, void *ctx),
                             void *ctx);
```

**Mutex Lock/Unlock (30+ instances):**
```c
pthread_mutex_lock_(&agent->tool_thread_mutex);
// ... critical section
pthread_mutex_unlock_(&agent->tool_thread_mutex);
```

**Could be:**
```c
#define WITH_MUTEX(mutex) \
    for (int _once = (pthread_mutex_lock_(mutex), 1); \
         _once; \
         _once = (pthread_mutex_unlock_(mutex), 0))

// Usage
WITH_MUTEX(&agent->tool_thread_mutex) {
    // ... critical section
    // Automatic unlock on scope exit
}
```

---

## Codebase Health Metrics

| Metric | Score | Notes |
|--------|-------|-------|
| Error Handling | 9/10 | Exemplary - follows documented framework |
| DI Compliance | 8/10 | Mostly good - a few violations (marks, logger legacy) |
| Naming Consistency | 7/10 | 21 namespace violations, abbreviation issues |
| Init Pattern Consistency | 7/10 | ~85% compliant, key violations in openai, marks |
| Bounded Context Separation | 6/10 | Main leak: agent mixes UI and LLM contexts |
| Code Duplication | 7/10 | Tool system has 350+ duplicated lines |
| Single Responsibility | 7/10 | Agent god object, db/agent.c too large |
| Static Function Policy | 5/10 | Policy exists but widely violated |

**Overall Codebase Quality: 7.5/10** - Well above average, with clear refactoring opportunities identified.

---

## Discussion Questions for Next Session

1. **Agent decomposition strategy**: Should we split in-place or create parallel structures during migration?

2. **Namespace refactoring timing**: Before or after tool abstractions? (Recommendation: before)

3. **Static function policy**: Fix LCOV configuration or keep inline preference? What's the real coverage tooling limitation?

4. **Init pattern migration**: Breaking change or deprecation path? (Affects external users if any)

5. **Tool vtable pattern**: Worth introducing for multi-provider future, or YAGNI?

6. **Module size threshold**: Should we enforce maximum file size (500 lines) via lint?

7. **Bounded context enforcement**: Should bounded contexts become separate directories? (e.g., `src/ui/`, `src/llm/`, `src/conversation/`)

---

## References

- **Analysis agents used:**
  - a5e6ec0: Initialization patterns analysis
  - a528776: Error handling analysis
  - ad909e8: Module boundaries and coupling analysis
  - ad6cf18: Naming consistency analysis
  - ac34aad: Code duplication and patterns analysis

- **Skills consulted:**
  - `.agents/skills/ddd.md` - Domain-Driven Design principles
  - `.agents/skills/di.md` - Dependency Injection patterns
  - `.agents/skills/errors.md` - Error handling framework
  - `.agents/skills/naming.md` - Naming conventions
  - `.agents/skills/style.md` - Code style guide
  - `.agents/skills/patterns/*.md` - Design patterns in C

- **Project docs:**
  - `project/naming.md` - Full naming specification
  - `project/error_handling.md` - Complete error handling guide
  - `project/return_values.md` - Return pattern documentation
  - `project/memory.md` - Talloc patterns and ownership
