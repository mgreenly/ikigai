# Dead Code Analysis

Analysis of orphaned functions: defined in `src/` but not reachable from `main()`.

Run `make dead-code` to regenerate this list.

## Summary

| Category | Count | Action |
|----------|-------|--------|
| Test utilities | ~15 | Move to `tests/` |
| Array APIs | 19 | Delete unused surface |
| Dead code | ~30 | Delete with tests |

## Phase 1: Move Test Utilities to `tests/`

These functions exist to help test real entry points. Move them (and their tests) to `tests/`.

### Logger test fixtures

```
ik_log_init:src/logger.c:146
ik_log_shutdown:src/logger.c:164
ik_log_reinit:src/logger.c:177
```

### Pretty printers (test assertions/debugging)

```
ik_pp_header:src/pp_helpers.c:5
ik_pp_pointer:src/pp_helpers.c:14
ik_pp_size_t:src/pp_helpers.c:27
ik_pp_int32:src/pp_helpers.c:36
ik_pp_uint32:src/pp_helpers.c:45
ik_pp_string:src/pp_helpers.c:54
ik_pp_bool:src/pp_helpers.c:94
ik_format_indent:src/format.c:96
```

### Render test entry points (isolated testing)

```
ik_render_input_buffer:src/render.c:39
ik_render_scrollback:src/render.c:130
```

### State reset functions (between tests)

```
ik_scroll_detector_reset:src/scroll_detector.c:166
ik_completion_clear:src/completion.c:160
```

### Other test utilities

```
ik_log_error_json:src/logger.c:281
ik_log_fatal_json:src/logger.c:286
ik_log_info_json:src/logger.c:271
ik_logger_fatal_json:src/logger.c:407
```

## Phase 2: Delete Unused Array APIs

These are generic container methods that production never uses. Delete functions and their tests.

```
ik_array_capacity:src/array.c:170
ik_array_clear:src/array.c:147
ik_array_delete:src/array.c:119
ik_array_insert:src/array.c:90
ik_array_set:src/array.c:136
ik_byte_array_capacity:src/byte_array.c:54
ik_byte_array_clear:src/byte_array.c:36
ik_byte_array_delete:src/byte_array.c:24
ik_byte_array_insert:src/byte_array.c:18
ik_byte_array_set:src/byte_array.c:30
ik_line_array_append:src/line_array.c:12
ik_line_array_capacity:src/line_array.c:54
ik_line_array_clear:src/line_array.c:36
ik_line_array_create:src/line_array.c:6
ik_line_array_delete:src/line_array.c:24
ik_line_array_get:src/line_array.c:42
ik_line_array_insert:src/line_array.c:18
ik_line_array_set:src/line_array.c:30
ik_line_array_size:src/line_array.c:48
```

## Phase 3: Delete Dead Code

These are genuinely unreachable. Delete functions and their tests.

### Incomplete tool system (never wired up)

```
ik_tool_build_all:src/tool.c:215
ik_tool_build_bash_schema:src/tool.c:162
ik_tool_build_file_read_schema:src/tool.c:108
ik_tool_build_file_write_schema:src/tool.c:145
ik_tool_build_glob_schema:src/tool.c:91
ik_tool_build_grep_schema:src/tool.c:127
ik_tool_build_schema_from_def:src/tool.c:167
ik_tool_add_string_parameter:src/tool.c:36
ik_tool_arg_get_int:src/tool_arg_parser.c:63
ik_tool_response_success:src/tool_response.c:41
ik_tool_response_success_ex:src/tool_response.c:75
ik_tool_result_add_limit_metadata:src/tool.c:295
ik_tool_truncate_output:src/tool.c:252
ik_repl_complete_tool_execution:src/repl_tool.c:290
ik_repl_execute_pending_tool:src/repl_tool.c:63
ik_repl_handle_tool_completion:src/repl_event_handlers.c:284
ik_repl_start_tool_execution:src/repl_tool.c:187
```

### Vestigial functions

```
ik_agent_restore:src/agent.c:143
ik_ansi_init:src/ansi.c:72
ik_completion_matches_prefix:src/completion.c:217
ik_event_renders_visible:src/event_render.c:20
ik_history_save:src/history_io.c:243
ik_layer_cake_get_total_height:src/layer.c:127
ik_mark_rewind_to:src/marks.c:209
ik_msg_is_conversation_kind:src/msg.c:12
ik_separator_layer_set_debug:src/layer_separator.c:238
```

## Automation Notes

Once Phase 1 is complete (test utilities moved), the `/quality` pipeline can:

1. Run `make dead-code`
2. For each orphan, delete function + tests
3. Commit each deletion atomically (reversible)
4. Run `make check` to verify

## Script Location

- `scripts/dead-code.sh` - generates orphan list
- `make dead-code` - runs the script
