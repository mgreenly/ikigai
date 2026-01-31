// Signal handler module for REPL resize events
//
// This file contains signal handling infrastructure that is difficult to test
// in unit tests because it requires actual OS signal delivery. The core resize
// logic (ik_repl_handle_resize) is fully tested in repl_resize_test.c.

#include "signal_handler.h"
#include "repl.h"
#include "wrapper.h"
#include <signal.h>
#include <errno.h>


#include "poison.h"
// Global flag for SIGWINCH (terminal resize signal)
static volatile sig_atomic_t g_resize_pending = 0;

// SIGWINCH signal handler
// LCOV_EXCL_START
static void handle_sigwinch(int sig)
{
    (void)sig;  // Unused
    g_resize_pending = 1;
}

// LCOV_EXCL_STOP

res_t ik_signal_handler_init(void *parent)
{
    // Set up SIGWINCH handler for terminal resize
    struct sigaction sa;
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (posix_sigaction_(SIGWINCH, &sa, NULL) < 0) {
        return ERR(parent, IO, "Failed to set SIGWINCH handler");
    }

    return OK(NULL);
}

// LCOV_EXCL_START
res_t ik_signal_check_resize(ik_repl_ctx_t *repl)
{
    if (g_resize_pending) {
        g_resize_pending = 0;
        return ik_repl_handle_resize(repl);
    }
    return OK(NULL);
}

// LCOV_EXCL_STOP
