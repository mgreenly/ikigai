// Terminal module - Raw mode and alternate screen management
#ifndef IK_TERMINAL_H
#define IK_TERMINAL_H

#include <termios.h>
#include "error.h"

// Terminal context for raw mode and alternate screen
typedef struct {
    int tty_fd;                    // Terminal file descriptor
    struct termios orig_termios;   // Original terminal settings
    int screen_rows;               // Terminal height
    int screen_cols;               // Terminal width
} ik_term_ctx_t;

// Initialize terminal (raw mode + alternate screen)
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out);

// Cleanup terminal (restore state)
void ik_term_cleanup(ik_term_ctx_t *ctx);

// Get terminal size
res_t ik_term_get_size(ik_term_ctx_t *ctx, int *rows_out, int *cols_out);

#endif // IK_TERMINAL_H
