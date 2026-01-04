# Current Tool Architecture

Reference document describing the existing internal tool system. This is what we're replacing.

## Components

**1. Tool Schema Definitions** (`src/providers/request_tools.c`, lines 22-79)

Static compile-time definitions for each tool:

```c
static const ik_tool_param_def_t glob_params[] = {
    {"pattern", "Glob pattern (e.g., 'src/**/*.c')", true},
    {"path", "Base directory (default: cwd)", false}
};

static const ik_tool_schema_def_t glob_schema_def = {
    .name = "glob",
    .description = "Find files matching a glob pattern",
    .params = glob_params,
    .param_count = 2
};
```

**2. Schema Builder** (`src/providers/request_tools.c`, lines 88-155)

Builds JSON parameter schema from tool definition:

```c
static char *build_tool_parameters_json(TALLOC_CTX *ctx, const ik_tool_schema_def_t *def)
{
    // Creates JSON schema string for a single tool's parameters
    // Called during request building for each of the 5 tools
}
```

Note: The legacy `ik_tool_build_all()` function in `src/tool.c` still exists but is NOT used by the main flow.

**3. Tool Dispatcher** (`src/tool_dispatcher.c`)

Routes tool calls to implementations:

```c
res_t ik_tool_dispatch(void *parent, const char *tool_name, const char *arguments)
{
    if (strcmp(tool_name, "glob") == 0) {
        char *pattern = ik_tool_arg_get_string(parent, arguments, "pattern");
        char *path = ik_tool_arg_get_string(parent, arguments, "path");
        return ik_tool_exec_glob(parent, pattern, path);
    }
    // ... similar for file_read, grep, file_write, bash
    // Returns: {"error": "Unknown tool: X"} for unknown tools
}
```

**4. Tool Implementations** (`src/tool_*.c`)

Each tool has dedicated implementation file:

```c
res_t ik_tool_exec_bash(void *parent, const char *command)
{
    // Execute via popen(), capture output
    // Returns: {"success": true, "data": {"output": "...", "exit_code": N}}
}
```

## Integration Points

**Integration Point A: LLM Request Building**

Location: `src/providers/request_tools.c` (lines 283-298)

```c
const ik_tool_schema_def_t *tool_defs[] = {
    &glob_schema_def,
    &file_read_schema_def,
    &grep_schema_def,
    &file_write_schema_def,
    &bash_schema_def
};

for (size_t i = 0; i < 5; i++) {
    char *params_json = build_tool_parameters_json(req, tool_defs[i]);
    res = ik_request_add_tool(req, tool_defs[i]->name, tool_defs[i]->description, params_json, false);
    // ...
}
```

**Purpose:** `ik_request_build_from_conversation()` populates `req->tools[]` from hard-coded static definitions. Provider serializers then iterate over `req->tools[]` to build the API-specific JSON.

**Integration Point B: Tool Execution**

Location: `src/repl_tool.c:43` (background thread) and `src/repl_tool.c:88` (synchronous)

```c
// Execute tool
res_t tool_res = ik_tool_dispatch(repl, tc->name, tc->arguments);
if (is_err(&tool_res)) PANIC("tool dispatch failed");
char *result_json = tool_res.ok;
```

**Purpose:** When LLM returns tool call, dispatch to implementation and get JSON result.

## Data Flow

```
1. ikigai starts
   └─> Compile-time: all tool schema definitions exist in binary
       (static structs in src/providers/request_tools.c)

2. User sends message
   └─> ik_request_build_from_conversation() creates request
       └─> [Integration Point A] Loops over hard-coded tool_defs[]
           └─> Calls ik_request_add_tool() for each tool
               └─> Populates req->tools[] array
   └─> Provider serializer iterates req->tools[]
       └─> Builds API-specific JSON format
           └─> Sends to LLM API

3. LLM responds with tool call
   └─> REPL extracts tool_name, arguments
       └─> [Integration Point B] Calls ik_tool_dispatch(tool_name, arguments)
           └─> Dispatcher routes to ik_tool_exec_*()
               └─> Tool executes, returns JSON
                   └─> Result sent back to LLM
```

## Current Formats

### Tool Schema (sent to LLM)

```json
[
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
]
```

### Tool Result (from execution)

```json
{
  "success": true,
  "data": {
    "output": "command output here",
    "exit_code": 0
  }
}
```

Or error:

```json
{
  "success": false,
  "error": "Error message here"
}
```
