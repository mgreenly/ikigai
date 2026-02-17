// Terminal module - Raw mode and alternate screen management
#ifndef IK_TERMINAL_H
#define IK_TERMINAL_H

#include <stdbool.h>
#include <termios.h>
#include "shared/error.h"
#include "shared/logger.h"

// Terminal context for raw mode and alternate screen
typedef struct {
    int tty_fd;                    // Terminal file descriptor
    struct termios orig_termios;   // Original terminal settings
    int screen_rows;               // Terminal height
    int screen_cols;               // Terminal width
    bool csi_u_supported;          // True if CSI u protocol is available
} ik_term_ctx_t;

// Initialize terminal (raw mode + alternate screen)
res_t ik_term_init(TALLOC_CTX *ctx, ik_logger_t *logger, ik_term_ctx_t **ctx_out);

// Initialize terminal with pre-opened file descriptor (for testing with PTY)
res_t ik_term_init_with_fd(TALLOC_CTX *ctx, ik_logger_t *logger, int fd, ik_term_ctx_t **ctx_out);

// Initialize headless terminal (no real TTY, canned values)
// Infallible: PANICs on OOM, never fails otherwise
ik_term_ctx_t *ik_term_init_headless(TALLOC_CTX *ctx);

// Cleanup terminal (restore state)
void ik_term_cleanup(ik_term_ctx_t *ctx);

// Get terminal size
res_t ik_term_get_size(ik_term_ctx_t *ctx, int *rows_out, int *cols_out);

#endif // IK_TERMINAL_H
