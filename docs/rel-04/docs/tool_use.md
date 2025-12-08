# Tool Implementation Guide

Source of truth for implementing tools in ikigai. Follow these patterns when adding new tools.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      Tool System                             │
├─────────────────────────────────────────────────────────────┤
│  Schema Layer     │  Dispatcher Layer  │  Executor Layer    │
│  ik_tool_build_*  │  ik_tool_dispatch  │  ik_tool_exec_*    │
│  (JSON schemas)   │  (JSON → params)   │  (execute + result)│
└─────────────────────────────────────────────────────────────┘
```

## Adding a New Tool

### 1. Define Schema

Create a function that returns the OpenAI tool schema:

```c
// In src/tool.h
res_t ik_tool_build_mytool_schema(void *parent);

// In src/tool.c
res_t ik_tool_build_mytool_schema(void *parent) {
    // Build JSON schema using yyjson
    // Return OK(json_string) or ERR(...)
}
```

### 2. Add Executor

Create the execution function with typed parameters:

```c
// In src/tool.h
res_t ik_tool_exec_mytool(void *parent, const char *param1, int32_t param2);

// In src/tool.c
res_t ik_tool_exec_mytool(void *parent, const char *param1, int32_t param2) {
    // Execute tool logic
    // Return result in envelope format (see below)
}
```

### 3. Register in Dispatcher

Update `ik_tool_dispatch()` to route to your executor:

```c
if (strcmp(name, "mytool") == 0) {
    const char *param1 = ik_tool_arg_get_string(args, "param1");
    int32_t param2 = ik_tool_arg_get_int(args, "param2");
    return ik_tool_exec_mytool(parent, param1, param2);
}
```

## Result Format

All tools return JSON in a consistent envelope format.

### Success

```json
{"success": true, "data": {...tool-specific fields...}}
```

### Error

```json
{"success": false, "error": "Human-readable error message"}
```

See `docs/tool-result-format.md` for complete specification and per-tool examples.

## Key Decisions

### Dispatcher Parses JSON Arguments

The dispatcher extracts typed parameters from the JSON arguments string and passes them to executors. Executors receive typed C values, not raw JSON.

**Rationale**: Centralizes JSON parsing, simplifies executor testing, makes signatures self-documenting.

### Tools Return res_t with JSON String

Executors return `res_t` where:
- `OK(json_string)` - Tool completed (success or operational error in envelope)
- `ERR(...)` - System error (malloc failed, internal bug)

**Rationale**: Distinguishes recoverable operational errors (file not found) from unrecoverable system errors.

### Bash Exit Codes Are Not Errors

A bash command with non-zero exit code returns:
```json
{"success": true, "data": {"output": "...", "exit_code": 1}}
```

This is `success: true` because the tool executed successfully. The exit code is data for the model to interpret.

**Rationale**: Exit codes carry semantic meaning (grep returns 1 for no matches). The model needs this information.

### Output Truncation

All tools use `ik_tool_truncate_output()` before building results. This enforces `max_output_size` from config uniformly.

## Testing Requirements

1. Test success path with valid inputs
2. Test error path (file not found, permission denied, invalid input)
3. Test edge cases (empty input, very large output)
4. Verify result matches envelope format
5. 100% coverage required

## File Locations

| Component | Location |
|-----------|----------|
| Tool types | `src/tool.h` |
| Schema builders | `src/tool.c` |
| Executors | `src/tool.c` |
| Dispatcher | `src/tool.c` |
| Argument parser | `src/tool.c` |
| Truncation utility | `src/tool.c` |
| Unit tests | `tests/unit/tool/` |
