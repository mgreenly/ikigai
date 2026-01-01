# Forbidden Internal Wrappers

These internal function wrappers exist in `src/wrapper_internal.h` and are used by tests. No new internal wrappers should be added - mock at external dependency level instead.

| Wrapper | Test Files |
|---------|------------|
| `ik_db_init_` | 3 |
| `ik_db_message_insert_` | 13 |
| `ik_scrollback_append_line_` | 3 |
| `ik_repl_render_frame_` | 6 |
| `ik_agent_get_provider_` | 4 |
| `ik_request_build_from_conversation_` | 4 |
| `ik_http_multi_create_` | 2 |
| `ik_http_multi_info_read_` | 1 |
| `ik_agent_start_tool_execution_` | 3 |
| `ik_agent_should_continue_tool_loop_` | 3 |
| `ik_repl_submit_tool_loop_continuation_` | 3 |
| `ik_agent_add_message_` | 3 |
| `ik_agent_transition_to_idle_` | 2 |
