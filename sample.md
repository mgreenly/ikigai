# Task: Select Next Requirement

**UNATTENDED EXECUTION:** This task runs automatically without human oversight. You are part of a pipeline that is converting source code requirements into finished code. Your job is to select the next requirement for a worker agent to implement. After you select a requirement, the worker agent will implement it, the harness will commit changes and run quality checks, and fix agents will attempt repairs if checks fail. When all checks pass, the requirement is marked done and the pipeline returns to you to select the next requirement. Use the "Pending Requirements" and "Recent History" sections below to determine which requirement should be worked on next.

## Pending Requirements


- `2`: `src/providers/request_tools.c` does not contain tool definitions or `build_tool_parameters_json()`, tool population loop is replaced with stub comment (must complete before removing schema types from tool.h)

- `3`: Makefile `CLIENT_SOURCES`, `MODULE_SOURCES`, `MODULE_SOURCES_NO_DB` do not contain `src/tool.c`, `src/tool_dispatcher.c`, `src/tool_bash.c`, `src/tool_file_read.c`, `src/tool_file_write.c`, `src/tool_glob.c`, `src/tool_grep.c`, `src/tool_response.c` (requires requirements 1-2 complete or link fails on missing symbols)

- `4`: `src/tool.c` does not exist (requires requirement 3 complete or build fails)

- `5`: `src/tool_dispatcher.c` does not exist (requires requirement 3 complete or build fails)

- `6`: `src/tool_bash.c` does not exist (requires requirement 3 complete or build fails)

- `7`: `src/tool_file_read.c` does not exist (requires requirement 3 complete or build fails)

- `8`: `src/tool_file_write.c` does not exist (requires requirement 3 complete or build fails)

- `9`: `src/tool_glob.c` does not exist (requires requirement 3 complete or build fails)

- `10`: `src/tool_grep.c` does not exist (requires requirement 3 complete or build fails)

- `11`: `src/tool_response.c` does not exist (requires requirement 3 complete or build fails)

- `12`: `src/tool_response.h` does not exist (requires requirements 6-11 complete)

- `13`: `src/tool.h` contains only `ik_tool_call_t` typedef, `ik_tool_call_create()`, `ik_tool_arg_get_string()`, `ik_tool_arg_get_int()` declarations (requires requirements 4-10 complete)



## Recent History

```
{"timestamp":"2026-01-10T22:34:24-06:00","requirement_id":"read_file_error","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:34:32-06:00","requirement_id":"awaiting_task","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:34:43-06:00","requirement_id":"ralph-prompt20260110-1000104-af5ub","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:35:22-06:00","requirement_id":"1","requirement":"`src/repl_tool.c` `ik_tool_dispatch()` calls are replaced with a stub returning `{\"success\": false, \"error\": \"Tool system not yet implemented. Tool 'X' unavailable.\"}` (must stub callers before removing ik_tool_dispatch() implementation)","success":true,"message":"Requirement already implemented: Both ik_tool_dispatch() calls in src/repl_tool.c have been replaced with stubs returning \"Tool system not yet implemented\" error (lines 43-48 in tool_thread_worker and lines 91-95 in ik_repl_execute_pending_tool)"}
{"timestamp":"2026-01-10T22:37:15-06:00","requirement_id":"awaiting_clarification","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:37:47-06:00","requirement_id":"1","requirement":"`src/repl_tool.c` `ik_tool_dispatch()` calls are replaced with a stub returning `{\"success\": false, \"error\": \"Tool system not yet implemented. Tool 'X' unavailable.\"}` (must stub callers before removing ik_tool_dispatch() implementation)","success":true,"message":"Requirement already implemented: Both ik_tool_dispatch() calls in src/repl_tool.c have been replaced with stubs returning \"Tool system not yet implemented\" error (lines 43-48 in tool_thread_worker and lines 91-95 in ik_repl_execute_pending_tool)"}
{"timestamp":"2026-01-10T22:40:33-06:00","requirement_id":"tmp_file_request","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:40:39-06:00","requirement_id":"ready","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:42:14-06:00","requirement_id":"msg_01ready","requirement":null,"success":false,"message":"Invalid requirement ID selected"}
{"timestamp":"2026-01-10T22:42:53-06:00","requirement_id":"1","requirement":"`src/repl_tool.c` `ik_tool_dispatch()` calls are replaced with a stub returning `{\"success\": false, \"error\": \"Tool system not yet implemented. Tool 'X' unavailable.\"}` (must stub callers before removing ik_tool_dispatch() implementation)","success":true,"message":"Requirement already implemented: Both ik_tool_dispatch() calls in src/repl_tool.c have been replaced with stubs returning \"Tool system not yet implemented\" error (lines 43-48 in tool_thread_worker and lines 91-95 in ik_repl_execute_pending_tool)"}

```

## Output

For the final result, use the StructuredOutput tool to return a single JSON object with the selected ID as an integer:

{"id": 1}

Or if there are no remaining pending requirements, return the string "none":

{"id": "none"}
