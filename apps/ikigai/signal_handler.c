// Signal handler module for REPL events (resize, quit)
//
// This file contains signal handling infrastructure that is difficult to test
// in unit tests because it requires actual OS signal delivery. The core resize
// logic (ik_repl_handle_resize) is fully tested in repl_resize_test.c.

#include "apps/ikigai/signal_handler.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/repl.h"
#include "shared/wrapper.h"
#include <signal.h>
#include <errno.h>


#include "shared/poison.h"
// Global flag for SIGWINCH (terminal resize signal)
static volatile sig_atomic_t g_resize_pending = 0;

// Global flag for SIGINT/SIGTERM (quit signals)
static volatile sig_atomic_t g_quit_pending = 0;

// SIGWINCH signal handler
// LCOV_EXCL_START
static void handle_sigwinch(int sig)
{
    (void)sig;
    g_resize_pending = 1;
}

static void handle_quit_signal(int sig)
{
    (void)sig;
    g_quit_pending = 1;
}

// LCOV_EXCL_STOP

res_t ik_signal_handler_init(void *parent)
{
    struct sigaction sa;

    // Set up SIGWINCH handler for terminal resize
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (posix_sigaction_(SIGWINCH, &sa, NULL) < 0) {
        return ERR(parent, IO, "Failed to set SIGWINCH handler");
    }

    // Set up SIGINT handler for clean shutdown
    sa.sa_handler = handle_quit_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (posix_sigaction_(SIGINT, &sa, NULL) < 0) {
        return ERR(parent, IO, "Failed to set SIGINT handler");
    }

    // Set up SIGTERM handler for clean shutdown
    sa.sa_handler = handle_quit_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (posix_sigaction_(SIGTERM, &sa, NULL) < 0) {
        return ERR(parent, IO, "Failed to set SIGTERM handler");
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

void ik_signal_check_quit(ik_repl_ctx_t *repl)
{
    if (g_quit_pending) {
        g_quit_pending = 0;
        // Same shutdown path as /exit: invalidate all providers, then quit
        for (size_t i = 0; i < repl->agent_count; i++) {
            ik_agent_invalidate_provider(repl->agents[i]);
        }
        repl->quit = true;
    }
}

// LCOV_EXCL_STOP
