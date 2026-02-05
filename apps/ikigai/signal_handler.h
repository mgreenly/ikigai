// Signal handler module for REPL resize events
#pragma once

#include "shared/error.h"
#include "apps/ikigai/repl.h"

// Initialize signal handlers for REPL (sets up SIGWINCH)
res_t ik_signal_handler_init(void *parent);

// Check if resize is pending and handle it
res_t ik_signal_check_resize(ik_repl_ctx_t *repl);
