# Tool Discovery and Execution - Technical Plan

Implementation details for external tool architecture.

## Tool Discovery

### Startup Behavior

**Blocking scan on startup (Phases 2-5):**

1. REPL initialization calls `ik_tool_discovery_run()` (blocking)
2. Scan two locations for executable files:
   - `PREFIX/libexec/ikigai/` (system tools shipped with ikigai)
   - `~/.ikigai/tools/` (user-defined tools)
3. For each executable, spawn process with `--schema` flag and wait
4. 1 second timeout per tool
5. Build tool registry from successful schema responses
6. User tools override system tools (same name preference)
7. Discovery completes, terminal appears

**State tracking:**

```c
typedef enum {
    TOOL_SCAN_NOT_STARTED,
    TOOL_SCAN_IN_PROGRESS,   // Used during discovery
    TOOL_SCAN_COMPLETE,
    TOOL_SCAN_FAILED
} tool_scan_state_t;

struct ik_shared_ctx_t {
    tool_scan_state_t tool_scan_state;
    ik_tool_registry_t *tool_registry;
};
```

**Startup timing:**

With 6 tools and 1s timeout per tool, worst case is 6 seconds. Typical case (all tools respond quickly) is <1 second. Blocking is acceptable for initial implementation.

**Phase 6 converts to async:** Terminal appears immediately, discovery runs in background. See architecture.md Phase 6.

### Credential-Independent Discovery

**Design Decision:** Tools are advertised to the LLM regardless of their credential/configuration state.

**`--schema` call never requires credentials:**
- Tool returns schema even if API keys missing
- Tool returns schema even if dependencies unavailable
- Credential/configuration checks happen only during execution

**Rationale:**

1. **Discoverability** - User sees what's available
   - Custom tools appear in tool list
   - User learns features by trying them
   - No pre-configuration required

2. **Self-Service Setup** - Error messages guide configuration
   - User: "Query the database"
   - LLM calls unconfigured tool
   - Tool returns `MISSING_CREDENTIALS` with setup steps
   - User follows instructions, retries immediately

3. **Zero ikigai Complexity** - Tools manage themselves
   - ikigai never checks credentials
   - ikigai never stores credentials
   - ikigai never knows what credentials are needed
   - Each tool is self-contained

**Example Flow:**

```bash
# Discovery (startup) - succeeds without credentials
$ db-query --schema
{
  "name": "db_query",
  "description": "Query the customer database",
  "parameters": {...}
}
# → Tool added to registry, advertised to LLM

# Execution (first use) - tool checks credentials
$ echo '{"query": "recent signups"}' | db-query
{
  "error": "Database connection not configured",
  "error_code": "MISSING_CREDENTIALS",
  "help": {
    "steps": [
      "1. Get credentials from internal wiki",
      "2. Set DB_CONNECTION_STRING environment variable",
      "3. Try again - it'll work automatically"
    ]
  }
}
# → ikigai wraps in tool_success envelope, LLM shows user

# After user sets DB_CONNECTION_STRING
$ echo '{"query": "recent signups"}' | db-query
{
  "results": [...],
  "count": 10
}
# → Works immediately, no restart required
```

**Tool Implementation Pattern:**

```python
def execute(params):
    # Check credentials first
    api_key = get_api_key()
    if not api_key:
        return {
            "error": "API key not configured",
            "error_code": "MISSING_CREDENTIALS",
            "help": {
                "steps": ["1. Sign up...", "2. Set env var...", ...]
            }
        }

    # Credentials present - proceed with operation
    return do_search(api_key, params["query"])
```

**Not checked during `--schema`:**
- Credentials (API keys, tokens)
- External dependencies (network, databases)
- Configuration files
- Service availability

**Benefits:**
- Tools appear immediately
- User discovers features organically
- Setup on first use (just-in-time)
- No initialization blocker
- No restart required after setup

### Schema Protocol

**Request:**
```bash
/path/to/tool --schema
```

**Expected response (stdout):**
```json
{
  "name": "tool_name",
  "description": "Brief description of what this tool does",
  "parameters": {
    "param1": {
      "type": "string",
      "description": "Parameter description",
      "required": true
    }
  },
  "returns": {
    "type": "object",
    "properties": {
      "field1": {"type": "string", "description": "..."}
    }
  }
}
```

**Timeout:** 1 second per tool (aggressive but fair - schema should be instant)

**Failure handling:**

Per-tool failures (crash, timeout, invalid JSON):
- Debug message in scrollback: `"Debug: tool 'foo' schema failed (timeout)"`
- Skip that tool
- Continue with other tools
- Tool effectively doesn't exist to ikigai

**Parallel execution with blocking wrapper (Phases 2-5):**

```c
// Pseudo-code - async internals, blocking API
// ik_tool_discovery_run() does this internally:

// 1. Spawn all tools in parallel
for each dir in [system_dir, user_dir] {
    for each tool in dir {
        if (!is_executable(tool)) continue;
        spawn_async("tool --schema");  // Non-blocking spawn
    }
}

// 2. Collect results via select() loop (blocking)
while (tools_pending > 0) {
    select() on all process pipes;
    for each readable fd {
        read schema, parse, add to registry (or log failure);
    }
}
```

**Key insight:** The async implementation exists from Phase 2. `ik_tool_discovery_run()` wraps it in a blocking call. Phase 6 exposes the async primitives to the REPL event loop.

### /refresh Command

**Behavior:**

1. User executes `/refresh`
2. Clear existing registry
3. Call `ik_tool_discovery_run()` (blocking)
4. Show "Tools refreshed. N tools available."

**Blocking is intentional:** User explicitly requested refresh - they expect immediate feedback with accurate count.

## Tool Execution

### Execution Protocol

**Input:** JSON via stdin
```bash
echo '{"param1": "value"}' | /path/to/tool
```

**Output:** JSON via stdout
```json
{
  "field1": "result value",
  "field2": 123
}
```

**stderr:** Captured separately for debug, not sent to LLM

**Exit codes:**
- `0` = tool executed successfully (even if operation failed - check JSON result)
- `non-zero` = tool itself failed (crash, invalid params)

### Response Wrapper

ikigai wraps all tool responses before sending to LLM.

**Tool declares schema:**
```json
{
  "returns": {
    "stdout": {"type": "string"},
    "exit_code": {"type": "integer"}
  }
}
```

**ikigai advertises to LLM:**
```json
{
  "returns": {
    "tool_success": {
      "type": "boolean",
      "description": "True if tool executed, false if ikigai error (timeout/crash)"
    },
    "result": {
      "description": "Present only when tool_success is true",
      ...tool's declared schema...
    },
    "error": {
      "type": "string",
      "description": "Present only when tool_success is false"
    },
    "error_code": {
      "type": "string",
      "description": "Machine-readable error code when tool_success is false"
    }
  }
}
```

**Why this matters:**

Tool declares its contract. If tool crashes or times out, ikigai can't return the tool's schema shape. Wrapper provides consistent interface regardless of execution outcome.

### Error Handling - Two Layers

**Layer 1: Operation Failures** (tool executes, operation fails)

Tool returns structured error in its declared schema:

```json
{
  "stdout": "",
  "stderr": "bash: foo: command not found\n",
  "exit_code": 127
}
```

ikigai wraps this as successful execution:

```json
{
  "tool_success": true,
  "result": {
    "stdout": "",
    "stderr": "bash: foo: command not found\n",
    "exit_code": 127
  }
}
```

LLM sees `tool_success: true`, reads `exit_code: 127`, understands command failed.

**Layer 2: Tool Failures** (tool crashes, times out, invalid output)

Tool crashes or returns invalid JSON. ikigai constructs wrapper:

```json
{
  "tool_success": false,
  "error": "Tool 'bash' crashed with exit code 139",
  "error_code": "TOOL_CRASHED"
}
```

LLM sees `tool_success: false`, reads error message.

### Standard ikigai Error Codes

When `tool_success: false`:

- `TOOL_TIMEOUT` - Tool exceeded execution timeout
- `TOOL_CRASHED` - Tool exited with non-zero code
- `INVALID_OUTPUT` - Tool returned malformed JSON
- `INVALID_PARAMS` - ikigai failed to marshal parameters (should not happen)
- `TOOL_NOT_FOUND` - Tool not in registry (registry refresh needed?)

### Execution Flow

```
1. LLM returns tool call: {"tool": "bash", "parameters": {...}}
2. ikigai looks up "bash" in tool registry
3. If not found: return {"tool_success": false, "error": "Tool not found"}
4. Spawn tool process with stdin/stdout/stderr pipes
5. Write parameters JSON to stdin, close
6. Read stdout until EOF (with timeout)
7. If timeout: kill process, return TOOL_TIMEOUT wrapper
8. If exit non-zero: return TOOL_CRASHED wrapper
9. Parse stdout as JSON
10. If invalid: return INVALID_OUTPUT wrapper
11. If valid: return {"tool_success": true, "result": ...}
```

## Implementation Phases

**See architecture.md for authoritative phase breakdown with human verification gates.**

Summary:
1. **Phase 1:** Remove internal tools
2. **Phase 2:** Sync infrastructure (blocking discovery)
3. **Phase 3:** First tool (bash)
4. **Phase 4:** Remaining tools (one at a time)
5. **Phase 5:** Commands (/tool, /refresh)
6. **Phase 6:** Async optimization

## Build System

### Project Structure

System tools are built and installed alongside ikigai:

```
ikigai/
├── src/
│   ├── client.c                    # Main ikigai binary
│   ├── tools/                      # Tool implementations in C
│   │   ├── bash/
│   │   │   └── main.c
│   │   ├── file_read/
│   │   │   └── main.c
│   │   ├── file_write/
│   │   │   └── main.c
│   │   ├── file_edit/
│   │   │   └── main.c
│   │   ├── glob/
│   │   │   └── main.c
│   │   └── grep/
│   │       └── main.c
│   └── ...
├── bin/                            # Build output (main binary)
│   └── ikigai
├── libexec/                        # Build output (tool executables)
│   └── ikigai/
│       ├── bash                    # Built from src/tools/bash/main.c
│       ├── file-read               # Built from src/tools/file_read/main.c
│       ├── file-write              # Built from src/tools/file_write/main.c
│       ├── file-edit               # Built from src/tools/file_edit/main.c
│       ├── glob                    # Built from src/tools/glob/main.c
│       └── grep                    # Built from src/tools/grep/main.c
└── tests/
    └── tools/                      # Tool-specific tests
```

### Makefile Integration

Tools compile like test binaries (separate executables, independent link steps):

```makefile
# Tool binaries (C implementations)
TOOL_BASH_SOURCES = src/tools/bash/main.c
TOOL_BASH_OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(TOOL_BASH_SOURCES))

TOOL_FILE_READ_SOURCES = src/tools/file_read/main.c
TOOL_FILE_READ_OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(TOOL_FILE_READ_SOURCES))

# Similar patterns for file_write, glob, grep...

# Compiled tools
libexec/ikigai/bash: $(TOOL_BASH_OBJ)
	@mkdir -p libexec/ikigai
	$(CC) $(LDFLAGS) -o $@ $^ $(CLIENT_LIBS)

libexec/ikigai/file-read: $(TOOL_FILE_READ_OBJ)
	@mkdir -p libexec/ikigai
	$(CC) $(LDFLAGS) -o $@ $^ $(CLIENT_LIBS)

# Similar rules for file-write, glob, grep...

TOOL_TARGETS = libexec/ikigai/bash libexec/ikigai/file-read libexec/ikigai/file-write libexec/ikigai/file-edit libexec/ikigai/glob libexec/ikigai/grep

# Add to all target
all: $(CLIENT_TARGET) $(TOOL_TARGETS)
```

### Installation

Package manager installs to standard FHS locations:

```makefile
install: all
	# Main binary
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 bin/ikigai $(DESTDIR)$(PREFIX)/bin/ikigai

	# System tools (helper executables, not in PATH)
	install -d $(DESTDIR)$(PREFIX)/libexec/ikigai
	install -m 755 libexec/ikigai/* $(DESTDIR)$(PREFIX)/libexec/ikigai/

	# Config
	install -d $(DESTDIR)$(sysconfdir)/ikigai
	install -m 644 etc/ikigai/config.json $(DESTDIR)$(sysconfdir)/ikigai/config.json
```

### Runtime Tool Discovery

ikigai discovers tools from two locations:

```c
// Pseudo-code
void discover_tools(ik_repl_ctx_t *ctx) {
    // System tools (installed with package)
    scan_directory(PREFIX "/libexec/ikigai");

    // User tools (user-managed)
    scan_directory(expand_tilde("~/.ikigai/tools"));

    // User tools override system tools (same name)
}
```

**PREFIX determination:**

Development mode:
```bash
make PREFIX=$PWD    # Builds with local path for dev testing
./bin/ikigai        # Discovers tools at ./libexec/ikigai/
```

Installed mode:
```bash
make install PREFIX=/usr/local
/usr/local/bin/ikigai  # Discovers tools at /usr/local/libexec/ikigai/
```

**Implementation:**

Option 1: Compile-time constant:
```c
#ifndef PREFIX
#define PREFIX "/usr/local"
#endif
```

Option 2: Runtime detection:
```c
// If ./libexec/ikigai/ exists relative to binary, use it (dev mode)
// Otherwise use PREFIX/libexec/ikigai/ (installed mode)
```

Option 1 is simpler and standard. Option 2 enables running from any location without recompilation.

### Packaging

**Monolithic package (rel-08 approach):**

```
Package: ikigai
Files:
  /usr/bin/ikigai
  /usr/libexec/ikigai/bash
  /usr/libexec/ikigai/file-read
  /usr/libexec/ikigai/file-write
  /usr/libexec/ikigai/file-edit
  /usr/libexec/ikigai/glob
  /usr/libexec/ikigai/grep
  /etc/ikigai/config.json
```

Future releases could split into separate packages (ikigai-core, ikigai-tools) if tool count grows large.

### Tool Naming Convention

**Source:** Use underscore in directory names (`file_read/`)

**Binary:** Use hyphen in executable names (`file-read`)

**Tool name advertised to LLM:** Convert to underscore (`file_read`)

**Rationale:**
- Hyphen is more common for executable names (FHS convention)
- Underscore works better in source directories (valid C identifiers)
- LLM sees consistent underscore naming

## Design Decisions

### Execution Timeout

**Decision:** Fixed 30s default. No per-tool override in rel-08.

Tools that need longer execution should be designed to return progress or stream results. 30s is generous for typical operations.

### Tool Validation

**Decision:** No validation by ikigai. Tools validate their own parameters.

Rationale:
- Keeps ikigai simple
- Avoids duplicate validation logic
- Tools return structured errors for invalid params
- LLM handles errors gracefully

### Schema Versioning

**Decision:** No versioning in rel-08. `/refresh` re-scans and picks up new schemas.

Breaking changes require coordination between tool updates and ikigai restarts. Future releases may add version negotiation if needed.

## Testing Strategy

### Unit Tests

- Schema parser (valid JSON, invalid JSON, missing fields)
- Response wrapper (successful execution, tool crash, timeout)
- Registry operations (add tool, lookup, iterate)

### Integration Tests

- Discovery: scan directory, collect schemas, build registry
- Execution: spawn tool, send params, receive result
- Error cases: tool crash, timeout, invalid JSON
- Wrapper: verify LLM sees consistent shape

### End-to-End Tests

- Full flow: startup scan → user submit → LLM call → tool execution → LLM response
- `/refresh` command with new tool added
- Failed tool during scan (timeout, crash)
