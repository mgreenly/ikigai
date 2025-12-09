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

// Terminal escape sequences
#define ESC_ALT_SCREEN_ENTER "\x1b[?1049h"
#define ESC_ALT_SCREEN_EXIT "\x1b[?1049l"
#define ESC_MOUSE_ENABLE "\x1b[?1006h"  // SGR mouse mode (button events with coordinates)
#define ESC_MOUSE_DISABLE "\x1b[?1006l"
#define ESC_TERMINAL_RESET "\x1b[?25h\x1b[0m"  // Show cursor + reset attributes

// Initialize terminal (raw mode + alternate screen)
res_t ik_term_init(void *parent, ik_term_ctx_t **ctx_out)
{
    assert(parent != NULL);    // LCOV_EXCL_BR_LINE
    assert(ctx_out != NULL);   // LCOV_EXCL_BR_LINE

    // Open /dev/tty
    int tty_fd = posix_open_("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        return ERR(parent, IO, "Failed to open /dev/tty");
    }

    // Allocate context
    ik_term_ctx_t *ctx = talloc_zero_(parent, sizeof(ik_term_ctx_t));
    if (ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ctx->tty_fd = tty_fd;

    // Get original termios settings
    if (posix_tcgetattr_(tty_fd, &ctx->orig_termios) < 0) {
        posix_close_(tty_fd);
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
    if (posix_tcsetattr_(tty_fd, TCSANOW, &raw) < 0) {
        posix_close_(tty_fd);
        return ERR(parent, IO, "Failed to set raw mode");
    }

    // Flush any stale input that was queued before raw mode
    if (posix_tcflush_(tty_fd, TCIFLUSH) < 0) {
        posix_tcsetattr_(tty_fd, TCSANOW, &ctx->orig_termios);
        (void)posix_close_(tty_fd);  // Explicitly ignore return value
        return ERR(parent, IO, "Failed to flush input");
    }

    // Enter alternate screen buffer
    if (posix_write_(tty_fd, ESC_ALT_SCREEN_ENTER, 8) < 0) {
        posix_tcsetattr_(tty_fd, TCSANOW, &ctx->orig_termios);
        posix_close_(tty_fd);
        return ERR(parent, IO, "Failed to enter alternate screen");
    }

    // Enable alternate scroll mode (wheel -> arrows)
    if (posix_write_(tty_fd, ESC_MOUSE_ENABLE, 8) < 0) {
        (void)posix_write_(tty_fd, ESC_ALT_SCREEN_EXIT, 8);
        posix_tcsetattr_(tty_fd, TCSANOW, &ctx->orig_termios);
        posix_close_(tty_fd);
        return ERR(parent, IO, "Failed to enable alternate scroll mode");
    }

    // Get terminal size
    struct winsize ws;
    if (posix_ioctl_(tty_fd, TIOCGWINSZ, &ws) < 0) {
        // Restore before returning error
        (void)posix_write_(tty_fd, ESC_MOUSE_DISABLE, 8);
        (void)posix_write_(tty_fd, ESC_ALT_SCREEN_EXIT, 8);
        posix_tcsetattr_(tty_fd, TCSANOW, &ctx->orig_termios);
        posix_close_(tty_fd);
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

    // Disable alternate scroll mode and exit alternate screen
    (void)posix_write_(ctx->tty_fd, ESC_MOUSE_DISABLE, 8);
    (void)posix_write_(ctx->tty_fd, ESC_TERMINAL_RESET, 10);
    (void)posix_write_(ctx->tty_fd, ESC_ALT_SCREEN_EXIT, 8);

    // Restore original termios settings (immediate, no blocking)
    posix_tcsetattr_(ctx->tty_fd, TCSANOW, &ctx->orig_termios);

    // Flush any remaining input
    (void)posix_tcflush_(ctx->tty_fd, TCIFLUSH);

    // Close tty file descriptor
    posix_close_(ctx->tty_fd);

    // Note: talloc_free(ctx) is caller's responsibility
}

// Get terminal size
res_t ik_term_get_size(ik_term_ctx_t *ctx, int *rows_out, int *cols_out)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(rows_out != NULL);   // LCOV_EXCL_BR_LINE
    assert(cols_out != NULL);   // LCOV_EXCL_BR_LINE

    struct winsize ws;
    if (posix_ioctl_(ctx->tty_fd, TIOCGWINSZ, &ws) < 0) {
        return ERR(talloc_parent(ctx), IO, "Failed to get terminal size");
    }

    ctx->screen_rows = (int)ws.ws_row;
    ctx->screen_cols = (int)ws.ws_col;

    *rows_out = ctx->screen_rows;
    *cols_out = ctx->screen_cols;

    return OK(NULL);
}
