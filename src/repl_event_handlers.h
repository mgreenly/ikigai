#pragma once

#include "error.h"
#include <sys/select.h>
#include <stdbool.h>

// Forward declaration
typedef struct ik_repl_ctx_t ik_repl_ctx_t;

// Calculate timeout for select() considering spinner and curl timeouts
long calculate_select_timeout_ms(ik_repl_ctx_t *repl, long curl_timeout_ms);

// Setup fd_sets for terminal and curl_multi
res_t setup_fd_sets(ik_repl_ctx_t *repl, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd_out);

// Handle terminal input events
res_t handle_terminal_input(ik_repl_ctx_t *repl, int terminal_fd, bool *should_exit);

// Handle curl events (HTTP requests)
res_t handle_curl_events(ik_repl_ctx_t *repl, int ready);

// Handle request success (LLM response complete)
void handle_request_success(ik_repl_ctx_t *repl);

// Handle tool thread completion
void handle_tool_completion(ik_repl_ctx_t *repl);
