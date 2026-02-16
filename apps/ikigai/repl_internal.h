// REPL internal functions - exposed for testing only
#pragma once

#include "apps/ikigai/repl.h"
#include "shared/error.h"
#include <stdbool.h>
#include <sys/select.h>

// Handle control socket events (accept, client messages)
// Called from the main event loop after select()
void ik_repl_handle_control_socket_events(ik_repl_ctx_t *repl, fd_set *read_fds);

// Drain one byte from key injection buffer and process it
// Sets *handled to true if a byte was processed
res_t ik_repl_handle_key_injection(ik_repl_ctx_t *repl, bool *handled);
