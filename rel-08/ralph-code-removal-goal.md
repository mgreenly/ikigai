## Objective

The internal tool system is being replaced with an external tool architecture. This goal covers removal only - the replacement comes later.

Remove all internal tool implementation code. After completion, the system builds and runs but tool calls return stub responses.

## Reference

`rel-08/plan/removal-specification.md` - detailed spec with exact changes.

## Outcomes

1. `src/repl_tool.c` - tool dispatch returns `{"success": false, "error": "Tool system not yet implemented. Tool 'X' unavailable."}`

2. `src/providers/request_tools.c` - no tool definitions, empty tools array sent to providers

3. `src/repl_callbacks.c` - `ik_tool_call_create()` returns NULL

4. Makefile does not reference: `tool.c`, `tool_dispatcher.c`, `tool_bash.c`, `tool_file_read.c`, `tool_file_write.c`, `tool_glob.c`, `tool_grep.c`, `tool_response.c`

5. These files do not exist: `src/tool_dispatcher.c`, `src/tool_bash.c`, `src/tool_file_read.c`, `src/tool_file_write.c`, `src/tool_glob.c`, `src/tool_grep.c`, `src/tool_response.c`, `src/tool_response.h`

6. `src/tool.h` contains only: `ik_tool_call_t`, `ik_tool_call_create()`, `ik_tool_arg_get_string()`, `ik_tool_arg_get_int()`

## Acceptance

- `check-build` → `{"ok": true}`
- `check-unit` → `{"ok": true}`
