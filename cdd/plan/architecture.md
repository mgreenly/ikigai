# External Tool Architecture

Specification for the external tool system.

## Components

### Tool Registry (`src/tool_registry.c`)

Dynamic runtime registry populated from external tools:

```c
typedef struct {
    char *name;                    // "bash", "file_read", etc.
    char *path;                    // "/usr/libexec/ikigai/bash" or "~/.ikigai/tools/custom"
    yyjson_doc *schema_doc;        // Parsed schema from --schema call
    yyjson_val *schema_root;       // Root of schema (for building tools array)
} ik_tool_registry_entry_t;

typedef struct {
    ik_tool_registry_entry_t *entries;
    size_t count;
    size_t capacity;
} ik_tool_registry_t;

// Create registry
ik_tool_registry_t *ik_tool_registry_create(TALLOC_CTX *ctx);

// Scan directories, spawn --schema, populate registry
res_t ik_tool_registry_scan(ik_tool_registry_t *registry);

// Look up tool by name
ik_tool_registry_entry_t *ik_tool_registry_lookup(ik_tool_registry_t *registry, const char *name);

// Build tools array for LLM
yyjson_mut_val *ik_tool_registry_build_all(ik_tool_registry_t *registry, yyjson_mut_doc *doc);
```

### Tool Discovery Engine (`src/tool_discovery.c`)

Async internals with blocking wrapper. Implementation uses async primitives (spawn all tools in parallel, collect via select), but initially only exposes a blocking function.

**Phases 4-5: Blocking API only**

```c
// Blocking wrapper: spawns all tools, waits for completion, returns
// Internally uses async primitives but blocks until done
res_t ik_tool_discovery_run(TALLOC_CTX *ctx,
                             const char *system_dir,
                             const char *user_dir,
                             ik_tool_registry_t *registry);
```

**Internal async primitives (implemented in Phase 4, not exposed until Phase 6):**

```c
// Start async scan: spawn all tools with --schema, return immediately
res_t ik_tool_discovery_start(TALLOC_CTX *ctx,
                               const char *system_dir,
                               const char *user_dir,
                               ik_tool_registry_t *registry,
                               ik_tool_discovery_state_t **out_state);

// Add tool discovery fds to select() fdsets
void ik_tool_discovery_add_fds(ik_tool_discovery_state_t *state,
                                fd_set *read_fds,
                                int *max_fd);

// Process ready fds after select() returns
res_t ik_tool_discovery_process_fds(ik_tool_discovery_state_t *state,
                                     fd_set *read_fds);

// Check if scan is complete
bool ik_tool_discovery_is_complete(ik_tool_discovery_state_t *state);

// Finalize scan: cleanup resources
void ik_tool_discovery_finalize(ik_tool_discovery_state_t *state);
```

**`ik_tool_discovery_run()` implementation:**
```c
res_t ik_tool_discovery_run(...) {
    ik_tool_discovery_state_t *state;
    TRY(ik_tool_discovery_start(ctx, system_dir, user_dir, registry, &state));

    while (!ik_tool_discovery_is_complete(state)) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = 0;
        ik_tool_discovery_add_fds(state, &read_fds, &max_fd);
        select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        ik_tool_discovery_process_fds(state, &read_fds);
    }

    ik_tool_discovery_finalize(state);
    return OK(NULL);
}
```

**Phase 6:** Expose async primitives to event loop (they already exist, just not public).

### External Tool Executor (`src/tool_external.c`)

Execute external tools with JSON I/O:

```c
res_t ik_tool_external_exec(TALLOC_CTX *ctx,
                             const char *tool_path,
                             const char *arguments_json);
```

**Behavior:** Spawns tool process, writes arguments to stdin, reads stdout with 30s timeout, returns tool's JSON output or error.

### Response Wrapper (`src/tool_wrapper.c`)

Wraps external tool responses:

```c
// Wrap successful tool execution
char *ik_tool_wrap_success(TALLOC_CTX *ctx, const char *tool_result_json);
// Returns: {"tool_success": true, "result": {...original...}}

// Wrap tool failure (crash, timeout, invalid JSON)
char *ik_tool_wrap_failure(TALLOC_CTX *ctx, const char *error, const char *error_code,
                           int exit_code, const char *stdout_captured, const char *stderr_captured);
// Returns: {"tool_success": false, "error": "...", "error_code": "...", "exit_code": N, "stdout": "...", "stderr": "..."}
```

### Scan State Enum

```c
typedef enum {
    TOOL_SCAN_NOT_STARTED,
    TOOL_SCAN_IN_PROGRESS,
    TOOL_SCAN_COMPLETE,
    TOOL_SCAN_FAILED
} tool_scan_state_t;
```

## Credential Management

ikigai never manages credentials. Tools are self-contained.

**ikigai does NOT:**
- Check for credentials
- Store credentials
- Validate credentials
- Know what credentials are needed

**Tools are responsible for:**
- Checking their own credentials on execution
- Returning helpful `MISSING_CREDENTIALS` errors
- Providing setup instructions in error messages

## Data Flow

```
1. ikigai starts
   └─> REPL init creates tool_registry
       └─> Calls ik_tool_discovery_run() (BLOCKING)
           └─> Scans PREFIX/libexec/ikigai/ and ~/.ikigai/tools/
           └─> Each tool called with --schema (1s timeout per tool)
               └─> Successful schemas added to registry
               └─> Failed schemas → debug message logged
       └─> Discovery completes, terminal appears

2. User sends message
   └─> OpenAI client builds request
       └─> Calls ik_tool_registry_build_all()
           └─> Returns dynamic JSON schema array from registry
               └─> Sends to LLM API

3. LLM responds with tool call
   └─> REPL extracts tool_name, arguments
       └─> Looks up tool in registry
           └─> Spawns external process: echo 'arguments' | tool
               └─> Reads stdout (JSON result)
                   └─> Wraps in tool_success envelope
                       └─> Result sent back to LLM
```

**Phase 6 changes this flow:** Discovery becomes async, terminal appears immediately, discovery runs in background. See Phase 6 in Migration Strategy.

## Integration Points

### Registry Access Pattern

The REPL owns the tool registry and passes it to the OpenAI client during request building.

**Ownership:**
- `ik_repl_ctx_t` owns `tool_registry` (created during REPL init)
- Registry lifetime matches REPL lifetime
- All components receive registry as parameter (no global state)

**Function signature change:**

```c
// src/openai/client.c
res_t ik_openai_create_request(TALLOC_CTX *ctx,
                                ik_tool_registry_t *tool_registry,  // NEW: Registry parameter
                                ik_chat_ctx_t *chat_ctx,
                                ik_openai_request_t **out_request);
```

**Call site pattern:**

```c
// src/repl_send.c (or wherever request is built)
res_t result = ik_openai_create_request(
    ctx,
    repl_ctx->tool_registry,  // Pass registry from REPL context
    chat_ctx,
    &request
);

// Inside ik_openai_create_request():
yyjson_mut_val *tools_array = ik_tool_registry_build_all(tool_registry, doc);
if (tools_array && yyjson_mut_arr_size(tools_array) > 0) {
    yyjson_mut_obj_add_val(doc, root, "tools", tools_array);
}
```

**Key invariants:**
- Registry is read-only during request building
- Registry must be in TOOL_SCAN_COMPLETE state before building request
- Empty registry (no tools) is valid - omit "tools" field from request

### Tool Execution Threading

External tool execution reuses the existing background thread pattern from `src/repl_tool.c`.

**Threading model:**
- External tools execute in a background thread (tool does not block REPL event loop)
- `ik_tool_external_exec()` is called from the background thread (blocking call within thread)
- The REPL event loop continues processing events while tool executes in background
- The 30s timeout is enforced within `ik_tool_external_exec()` using alarm/SIGALRM or poll with timeout
- Thread synchronization follows existing pattern: mutex-protected result passing

**Call flow:**

```
1. LLM returns tool_call → REPL transitions to EXECUTING_TOOL state
   └─> ik_agent_start_tool_execution(agent)
       └─> Create tool_thread_ctx (memory context for thread allocations)
       └─> Copy tool_name and arguments into thread context
       └─> Set tool_thread_complete = false, tool_thread_running = true
       └─> pthread_create(&agent->tool_thread, NULL, tool_thread_worker, args)
           └─> Thread starts running in background
           └─> REPL returns to event loop (spinner animates)

2. Background thread executes tool
   └─> tool_thread_worker(args)
       └─> Look up tool in registry: ik_tool_registry_lookup(registry, tool_name)
       └─> Execute external tool: ik_tool_external_exec(ctx, tool_path, arguments_json)
           └─> Spawn process with stdin/stdout/stderr pipes
           └─> Write arguments_json to stdin, close stdin
           └─> Read stdout with 30s timeout (alarm/SIGALRM or poll)
           └─> Parse JSON result or create error envelope
           └─> Wrap result: ik_tool_wrap_success() or ik_tool_wrap_failure()
           └─> Return wrapped result
       └─> Store result: agent->tool_thread_result = result.ok
       └─> Signal completion:
           pthread_mutex_lock(&agent->tool_thread_mutex)
           agent->tool_thread_complete = true
           pthread_mutex_unlock(&agent->tool_thread_mutex)

3. Event loop detects completion (checked each iteration)
   └─> if (agent->tool_thread_running && agent->tool_thread_complete)
       └─> ik_agent_complete_tool_execution(agent)
           └─> pthread_join(agent->tool_thread, NULL) // Clean up thread resources
           └─> Steal result from thread context: talloc_steal(agent, agent->tool_thread_result)
           └─> Add tool_call message to conversation
           └─> Add tool_result message to conversation
           └─> Display in scrollback via event renderer
           └─> Persist to database
           └─> talloc_free(agent->tool_thread_ctx)
           └─> Reset thread state: tool_thread_running = false, tool_thread_complete = false
           └─> Transition back to WAITING_FOR_LLM state
```

**Integration with existing pattern:**

The external tool system integrates with `src/repl_tool.c` by:
1. Keeping the same threading primitives: `pthread_create`, `pthread_join`, `pthread_mutex_lock/unlock`
2. Using the same memory ownership model: main thread owns thread context, thread allocates into it
3. Following the same completion protocol: thread sets flag under mutex, main thread polls flag in event loop
4. Maintaining the same state transitions: EXECUTING_TOOL during execution, WAITING_FOR_LLM after completion

**Key changes from internal tools:**

| Aspect | Internal Tools | External Tools |
|--------|---------------|----------------|
| Execution | `ik_tool_dispatch()` calls C function directly | `ik_tool_external_exec()` spawns process, does I/O |
| Timeout | No timeout (C function assumed fast) | 30s timeout via alarm/SIGALRM or poll |
| Error handling | Return ERR() from C function | Wrap failures with `ik_tool_wrap_failure()` |
| Thread context | Minimal (just result storage) | Includes process pipes, buffers, exit code |

## Tool Schema Format

External tool returns via `--schema`:

```json
{
  "name": "bash",
  "description": "Execute a shell command",
  "parameters": {
    "command": {
      "type": "string",
      "description": "Command to execute",
      "required": true
    }
  },
  "returns": {
    "type": "object",
    "properties": {
      "stdout": {"type": "string"},
      "stderr": {"type": "string"},
      "exit_code": {"type": "integer"}
    }
  }
}
```

ikigai transforms for LLM (adds provider wrapper + response wrapper).

### Schema Transformation

External tools return a simple JSON schema format via `--schema`, but LLM providers like OpenAI require a specific function calling format. The transformation happens inside `ik_tool_registry_build_all()` when building the tools array for API requests.

**Transformation helper function:**

```c
// src/tool_registry.c
yyjson_mut_val *ik_tool_schema_to_openai(yyjson_mut_doc *doc, yyjson_val *tool_schema);
```

**Field mapping:**

| External Format | OpenAI Format | Transformation |
|-----------------|---------------|----------------|
| `name` | `function.name` | Direct copy |
| `description` | `function.description` | Direct copy |
| `parameters` object | `function.parameters.properties` | Extract `required` fields into array |

**Detailed transformation:**

1. **Top-level wrapper:** OpenAI requires `"type": "function"` with nested `"function"` object
2. **Parameters object:**
   - External: flat object with `name: {type, description, required}` entries
   - OpenAI: `parameters.type = "object"`, `parameters.properties = {...}`, `parameters.required = [...]`
3. **Required field extraction:** Iterate external parameters, collect names where `required: true`, build array

**Input example (external tool schema):**

```json
{
  "name": "bash",
  "description": "Execute a shell command",
  "parameters": {
    "command": {
      "type": "string",
      "description": "Command to execute",
      "required": true
    }
  }
}
```

**Output example (OpenAI function format):**

```json
{
  "type": "function",
  "function": {
    "name": "bash",
    "description": "Execute a shell command",
    "parameters": {
      "type": "object",
      "properties": {
        "command": {
          "type": "string",
          "description": "Command to execute"
        }
      },
      "required": ["command"]
    }
  }
}
```

**Implementation notes:**

- `ik_tool_registry_build_all()` iterates all registry entries, calling `ik_tool_schema_to_openai()` for each
- The `returns` field from external schema is ignored (not part of OpenAI function schema)
- Empty parameters object results in `"parameters": {"type": "object", "properties": {}}` (no required array)
- Invalid external schemas are skipped with debug message (don't crash registry building)

## Tool Result Format

Successful execution:

```json
{
  "tool_success": true,
  "result": {
    "stdout": "command output",
    "stderr": "",
    "exit_code": 0
  }
}
```

Tool failure (crash/timeout):

```json
{
  "tool_success": false,
  "error": "Tool 'bash' timed out after 30s",
  "error_code": "TOOL_TIMEOUT",
  "exit_code": null,
  "stdout": "partial output...",
  "stderr": ""
}
```

## Commands

User-facing commands for tool inspection and management. Local commands (no LLM call).

### Command Signatures

```c
res_t ik_cmd_tool(void *ctx, ik_repl_ctx_t *repl, const char *args);
res_t ik_cmd_refresh(void *ctx, ik_repl_ctx_t *repl, const char *args);
```

### `/tool` - List or Inspect Tools

**File:** `src/commands_tool.c`

**Behavior:**

| Input | Output |
|-------|--------|
| `/tool` | List all tools with names and descriptions |
| `/tool NAME` | Show full schema for named tool |
| `/tool UNKNOWN` | Error message with hint to run `/tool` |

**Output format:** See user stories `list-tools.md` and `inspect-tool.md` for exact format.

**Error handling:** Unknown tool returns `ERR(ctx, ERR_INVALID_ARG, ...)` after displaying error in scrollback.

### `/refresh` - Reload Tool Registry

**File:** `src/commands_tool.c`

**Behavior:**
1. Display "Refreshing tools..."
2. Clear existing registry
3. Call `ik_tool_discovery_run()` (blocks until complete)
4. Display "Tools refreshed. N tools available."

**UX:** Blocking (~1 second). User requested refresh, expects immediate accurate feedback.

**Error handling:** Discovery failure sets `tool_scan_state = TOOL_SCAN_FAILED` and returns error.

### Command Registration

| Command | Description |
|---------|-------------|
| `tool` | List or inspect tools (usage: /tool [NAME]) |
| `refresh` | Reload tool registry from disk |

### Integration Points

**File structure:**
- `src/commands.c` - Add registry entries, forward declarations
- `src/commands_tool.c` - New file with `ik_cmd_tool()` and `ik_cmd_refresh()` implementations
- `src/commands_tool.h` - New header with function declarations

**Dependencies:**
- Commands access `repl->tool_registry` (REPL owns registry)
- Commands output to `repl->current->scrollback` (existing pattern)
- Commands return `res_t` for error handling (existing pattern)
- Commands persist to database via `ik_cmd_persist_to_db()` (existing pattern)

**Error handling:**
- Unknown tool name → helpful error message with `/tool` suggestion
- Empty registry → "No tools available" message
- Scan failures → logged but don't crash (memory state is authoritative)

## Migration Strategy

**Approach:** Tools-first implementation. Build and test all external tools as standalone executables, then integrate into ikigai.

**Human verification required between each phase.** Orchestration stops after each phase for manual verification before proceeding.

### Phase 1: First Tool (bash)

**Goal:** Build first external tool as standalone executable. Test without ikigai integration.

**Build:**
- `src/tools/bash/main.c` - bash tool implementation
- Makefile rule for `libexec/ikigai/bash`

**Validate standalone (no ikigai needed):**
- `--schema` returns valid JSON
- Execution accepts JSON stdin, returns JSON stdout
- Error cases return proper JSON error responses

**⏸ STOP: Human Verification**
- [ ] `libexec/ikigai/bash --schema` outputs valid schema
- [ ] `echo '{"command":"echo hello"}' | libexec/ikigai/bash` returns JSON
- [ ] Invalid input returns JSON error (not crash)
- [ ] Tool handles missing parameters gracefully

---

### Phase 2: Remaining Tools

**Goal:** Build remaining tools as standalone executables. Each tool verified before proceeding to next.

**Order:**
1. **file_read** - File reading
2. **file_write** - File writing
3. **file_edit** - File editing with text replacement
4. **glob** - Pattern matching
5. **grep** - Content search

**Per tool:**
1. Create `src/tools/<tool>/main.c`
2. Add Makefile rule
3. Test `--schema` output
4. Test execution with sample input
5. Test error handling

**⏸ STOP: Human Verification (after each tool)**
- [ ] Tool builds successfully
- [ ] `--schema` returns valid JSON
- [ ] Direct execution works with valid input
- [ ] Error cases return JSON errors (not crashes)

---

### Phase 3: Remove Internal Tool System

**Goal:** Clean slate. Delete internal tools, stub integration points.

**Delete:**
- `src/tool_dispatcher.c` - Internal routing
- `src/tool_bash.c` - Built-in bash
- `src/tool_file_read.c` - Built-in file read
- `src/tool_file_write.c` - Built-in file write
- `src/tool_glob.c` - Built-in glob
- `src/tool_grep.c` - Built-in grep
- `ik_tool_build_all()` - Static schema builder
- `ik_tool_dispatch()` - Internal dispatcher

**Keep (still useful):**
- `src/tool.h` - Types like `ik_tool_call_t`
- `src/tool_arg_parser.c` - Useful for external tools

**Stub integration points:**
- `src/openai/client.c` - Return empty tools array
- `src/repl_tool.c` - Return "no tools available" error

**⏸ STOP: Human Verification**
- [ ] Compiles without errors
- [ ] All existing tests pass
- [ ] LLM requests succeed (with no tools)
- [ ] Tool calls return "no tools available" error

---

### Phase 4: Sync Infrastructure

**Goal:** Build external tool infrastructure with blocking (sync) discovery and execution. Test against tools built in Phases 1-2.

**New files:**
- `src/tool_registry.c/.h` - Dynamic registry
- `src/tool_discovery.c/.h` - Blocking schema collection (sync, not async)
- `src/tool_external.c/.h` - Process execution with pipes
- `src/tool_wrapper.c/.h` - Response envelope

**Integration:**
- `src/shared.h` - Add `tool_registry` and `tool_scan_state` fields
- `src/repl_init.c` - Call blocking `ik_tool_discovery_run()` at startup
- `src/openai/client.c` - Use `ik_tool_registry_build_all()`
- `src/repl_tool.c` - Use registry lookup + `ik_tool_external_exec()`

**Key constraint:** Discovery is BLOCKING. `ik_tool_discovery_run()` blocks until all tools are scanned. No event loop integration yet.

**⏸ STOP: Human Verification**
- [ ] Registry creates and stores entries
- [ ] Blocking discovery scans directories, finds all 6 tools
- [ ] Schema parsing works for all tools
- [ ] External execution spawns process, captures stdout
- [ ] Response wrapper produces correct JSON envelope
- [ ] LLM sees all 6 tools in tools array
- [ ] LLM can call each tool end-to-end

---

### Phase 5: Commands

**Goal:** User-facing commands for tool inspection and management.

**Implement:**
- `/tool` - List all available tools
- `/tool NAME` - Show schema for specific tool
- `/refresh` - Re-run blocking discovery

**⏸ STOP: Human Verification**
- [ ] `/tool` lists all 6 tools with descriptions
- [ ] `/tool bash` shows bash schema
- [ ] `/tool unknown` shows helpful error
- [ ] `/refresh` re-scans and updates registry

---

### Phase 6: Async Optimization

**Goal:** Expose existing async internals to event loop for non-blocking startup.

**Context:** The async primitives (`_start`, `_add_fds`, `_process_fds`, `_is_complete`, `_finalize`) already exist from Phase 4. They're used internally by `ik_tool_discovery_run()`. Phase 6 exposes them publicly and integrates with the REPL event loop.

**Changes:**
- `src/tool_discovery.h` - Make async functions public (move from static to header)
- `src/repl.h` - Add `tool_discovery` field to `ik_repl_ctx_t`
- `src/repl_init.c` - Call `ik_tool_discovery_start()` instead of `_run()`
- `src/repl.c` - Integrate discovery fds with event loop select()
- `src/repl_actions_llm.c` - Wait for scan if in progress when user submits

**Behavior after change:**
- Terminal appears immediately
- User can type while discovery runs in background
- If user submits before discovery complete: show spinner, wait
- Discovery typically completes in <1 second

**⏸ STOP: Human Verification**
- [ ] Startup is non-blocking (terminal appears immediately)
- [ ] Discovery completes in background
- [ ] User submit waits if discovery in progress
- [ ] All tools still discovered correctly
- [ ] No race conditions or hangs

## Testing Strategy

### Unit Tests

- `tests/unit/tool/registry_test.c` - Registry operations
- `tests/unit/tool/discovery_test.c` - Schema parsing, timeout handling
- `tests/unit/tool/external_exec_test.c` - Process spawning, wrapping
- `tests/unit/tool/wrapper_test.c` - Response envelope wrapping

### Integration Tests

- `tests/integration/external_tool_test.c` - End-to-end external tool execution
- `tests/integration/tool_discovery_test.c` - Full scan and registry population

## Design Decisions

1. **Schema transformation:** ikigai wraps external tool schemas into provider format (OpenAI, Anthropic). Tools return simple JSON schema, ikigai handles provider differences.

2. **Schema versioning:** Not in rel-08. `/refresh` re-scans and picks up new schemas.

3. **Error codes:** Standard codes recommended but not enforced. Tools can use any error format. ikigai only enforces the response wrapper.
