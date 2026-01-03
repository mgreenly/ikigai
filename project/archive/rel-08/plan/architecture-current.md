# Current Tool Architecture

Reference document describing the existing internal tool system. This is what we're replacing.

## Components

**1. Tool Schema Definitions** (`src/tool.c`, `src/tool.h`)

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

**2. Schema Builder** (`src/tool.c`)

Builds JSON schema array for LLM:

```c
yyjson_mut_val *ik_tool_build_all(yyjson_mut_doc *doc)
{
    // Creates array with all 5 tools: glob, file_read, grep, file_write, bash
    // Returns yyjson array ready for LLM API request
}
```

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

Location: `src/openai/client.c:191`

```c
/* Build and add tools array */
yyjson_mut_val *tools_arr = ik_tool_build_all(doc);
if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) {
    PANIC("Failed to add tools array to JSON");
}
```

**Purpose:** Adds tool schemas to OpenAI API request so LLM knows available tools.

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
   └─> Compile-time: all tool schemas exist in binary

2. User sends message
   └─> OpenAI client builds request
       └─> [Integration Point A] Calls ik_tool_build_all()
           └─> Returns static JSON schema array
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
