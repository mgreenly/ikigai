// Terminal module - Raw mode and alternate screen management
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include "panic.h"
#include "terminal.h"
#include "wrapper.h"

// Initialize terminal (raw mode + alternate screen)
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out)
{
    assert(parent != NULL);    // LCOV_EXCL_BR_LINE
    assert(ctx_out != NULL);   // LCOV_EXCL_BR_LINE

    // Open /dev/tty
    int tty_fd = ik_open_wrapper("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        return ERR(parent, IO, "Failed to open /dev/tty");
    }

    // Allocate context
    ik_term_ctx_t *ctx = ik_talloc_zero_wrapper(parent, sizeof(ik_term_ctx_t));
    if (ctx == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ctx->tty_fd = tty_fd;

    // Get original termios settings
    if (ik_tcgetattr_wrapper(tty_fd, &ctx->orig_termios) < 0) {
        ik_close_wrapper(tty_fd);
        return ERR(parent, IO, "Failed to get terminal attributes");
    }

    // Set raw mode
    struct termios raw = ctx->orig_termios;
    raw.c_iflag &= (uint32_t)(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
    raw.c_oflag &= (uint32_t)(~(OPOST));
    raw.c_cflag |= (CS8);
    raw.c_lflag &= (uint32_t)(~(ECHO | ICANON | IEXTEN | ISIG));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    // Apply raw mode immediately (no blocking)
    if (ik_tcsetattr_wrapper(tty_fd, TCSANOW, &raw) < 0) {
        ik_close_wrapper(tty_fd);
        return ERR(parent, IO, "Failed to set raw mode");
    }

    // Flush any stale input that was queued before raw mode
    if (ik_tcflush_wrapper(tty_fd, TCIFLUSH) < 0) {
        ik_tcsetattr_wrapper(tty_fd, TCSANOW, &ctx->orig_termios);
        (void)ik_close_wrapper(tty_fd);  // Explicitly ignore return value
        return ERR(parent, IO, "Failed to flush input");
    }

    // Enter alternate screen buffer
    const char *alt_screen = "\x1b[?1049h";
    if (ik_write_wrapper(tty_fd, alt_screen, 8) < 0) {
        ik_tcsetattr_wrapper(tty_fd, TCSANOW, &ctx->orig_termios);
        ik_close_wrapper(tty_fd);
        return ERR(parent, IO, "Failed to enter alternate screen");
    }

    // Get terminal size
    struct winsize ws;
    if (ik_ioctl_wrapper(tty_fd, TIOCGWINSZ, &ws) < 0) {
        // Restore before returning error
        const char *exit_alt = "\x1b[?1049l";
        (void)ik_write_wrapper(tty_fd, exit_alt, 8);
        ik_tcsetattr_wrapper(tty_fd, TCSANOW, &ctx->orig_termios);
        ik_close_wrapper(tty_fd);
        return ERR(parent, IO, "Failed to get terminal size");
    }

    ctx->screen_rows = (int)ws.ws_row;
    ctx->screen_cols = (int)ws.ws_col;

    *ctx_out = ctx;
    return OK(ctx);
}

// Cleanup terminal (restore state)
void ik_term_cleanup(ik_term_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    // Exit alternate screen buffer
    const char *exit_alt = "\x1b[?1049l";
    (void)ik_write_wrapper(ctx->tty_fd, exit_alt, 8);

    // Restore original termios settings (immediate, no blocking)
    ik_tcsetattr_wrapper(ctx->tty_fd, TCSANOW, &ctx->orig_termios);

    // Flush any remaining input
    (void)ik_tcflush_wrapper(ctx->tty_fd, TCIFLUSH);

    // Close tty file descriptor
    ik_close_wrapper(ctx->tty_fd);

    // Note: talloc_free(ctx) is caller's responsibility
}

// Get terminal size
res_t ik_term_get_size(ik_term_ctx_t *ctx, int *rows_out, int *cols_out)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(rows_out != NULL);   // LCOV_EXCL_BR_LINE
    assert(cols_out != NULL);   // LCOV_EXCL_BR_LINE

    struct winsize ws;
    if (ik_ioctl_wrapper(ctx->tty_fd, TIOCGWINSZ, &ws) < 0) {
        return ERR(talloc_parent(ctx), IO, "Failed to get terminal size");
    }

    ctx->screen_rows = (int)ws.ws_row;
    ctx->screen_cols = (int)ws.ws_col;

    *rows_out = ctx->screen_rows;
    *cols_out = ctx->screen_cols;

    return OK(NULL);
}
