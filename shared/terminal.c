// Terminal module - Raw mode and alternate screen management
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include "shared/logger.h"
#include "shared/panic.h"
#include "shared/terminal.h"
#include "shared/wrapper.h"


#include "shared/poison.h"
// Terminal escape sequences
#define ESC_ALT_SCREEN_ENTER "\x1b[?1049h"
#define ESC_ALT_SCREEN_EXIT "\x1b[?1049l"
#define ESC_TERMINAL_RESET "\x1b[?25h\x1b[0m"  // Show cursor + reset attributes
#define ESC_CSI_U_QUERY "\x1b[?u"              // Query CSI u support
#define ESC_CSI_U_ENABLE "\x1b[>9u"            // Enable CSI u with flag 9
#define ESC_CSI_U_DISABLE "\x1b[<u"            // Disable CSI u

// Probe for CSI u support
static bool probe_csi_u_support(int tty_fd)
{
    // Send query
    const char *query = ESC_CSI_U_QUERY;
    if (posix_write_(tty_fd, query, 4) < 0) {
        return false;
    }

    // Wait for response with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tty_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms

    int ready = posix_select_(tty_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready <= 0) {
        return false;  // Timeout or error - no CSI u support
    }

    // Read response - format: ESC[?flags u
    char buf[32];
    ssize_t n = posix_read_(tty_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        return false;
    }
    buf[n] = '\0';

    // Check for ESC[? prefix and u suffix
    if (n >= 4 && buf[0] == '\x1b' && buf[1] == '[' && buf[2] == '?') {
        // Look for 'u' terminator
        for (ssize_t i = 3; i < n; i++) {
            if (buf[i] == 'u') {
                return true;
            }
        }
    }

    return false;
}

// Enable CSI u and read/verify the response
// Returns true if successfully enabled, false otherwise
static bool enable_csi_u(int tty_fd, ik_logger_t *logger)
{
    // Send enable command with flag 9 (disambiguate + report all keys)
    if (posix_write_(tty_fd, ESC_CSI_U_ENABLE, 5) < 0) {
        return false;
    }

    // Wait for response with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tty_fd, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms

    int ready = posix_select_(tty_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ready <= 0) {
        // Some terminals don't send a response to enable command
        // This is OK - assume it worked if probe succeeded
        return true;
    }

    // Read response - terminals may echo back the enabled flags
    // Format could be: ESC[?<flags>u or other variations
    char buf[32];
    ssize_t n = posix_read_(tty_fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        return false;
    }
    buf[n] = '\0';

    // Parse the response to see what flags were actually enabled
    // Expected format: ESC[?<flags>u
    if (n >= 4 && buf[0] == '\x1b' && buf[1] == '[' && buf[2] == '?') {
        // Extract the flags value
        int flags = 0;
        for (ssize_t i = 3; i < n; i++) {
            if (buf[i] == 'u') {
                // Log to JSONL
                if (logger != NULL) {
                    yyjson_mut_doc *doc = ik_log_create();
                    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);  // LCOV_EXCL_BR_LINE
                    yyjson_mut_obj_add_str(doc, root, "event", "csi_u_enabled");
                    yyjson_mut_obj_add_int(doc, root, "flags", flags);  // LCOV_EXCL_BR_LINE
                    ik_logger_debug_json(logger, doc);
                }

                return true;
            } else if (buf[i] >= '0' && buf[i] <= '9') {
                flags = flags * 10 + (buf[i] - '0');
            }
        }
    }

    // Log unexpected response to JSONL
    if (logger != NULL) {
        yyjson_mut_doc *doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(doc, root, "event", "csi_u_unexpected_response");
        yyjson_mut_obj_add_int(doc, root, "response_length", (int)n);  // LCOV_EXCL_BR_LINE
        // Add hex dump of response
        char hex[128] = {0};
        for (ssize_t i = 0; i < n && i < 32; i++) {  // LCOV_EXCL_BR_LINE (i<32 unreachable: buf is 32 bytes max)
            size_t offset = (size_t)(i * 3);
            snprintf(hex + offset, sizeof(hex) - offset, "%02x ", (unsigned char)buf[i]);
        }
        yyjson_mut_obj_add_str(doc, root, "response_hex", hex);
        ik_logger_debug_json(logger, doc);
    }

    // Even if response format is unexpected, assume it worked
    return true;
}

// Initialize headless terminal (no TTY, no I/O)
ik_term_ctx_t *ik_term_init_headless(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_term_ctx_t *term_ctx = talloc_zero_(ctx, sizeof(ik_term_ctx_t));
    if (term_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    term_ctx->tty_fd = -1;
    term_ctx->screen_rows = 50;
    term_ctx->screen_cols = 100;
    term_ctx->csi_u_supported = false;

    return term_ctx;
}

/* LCOV_EXCL_START */
// Initialize terminal (raw mode + alternate screen)
res_t ik_term_init(TALLOC_CTX *ctx, ik_logger_t *logger, ik_term_ctx_t **ctx_out)
{
    assert(ctx != NULL);
    assert(ctx_out != NULL);

    int tty_fd = posix_open_("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        return ERR(ctx, IO, "Failed to open /dev/tty");
    }
    return ik_term_init_with_fd(ctx, logger, tty_fd, ctx_out);
}

/* LCOV_EXCL_STOP */

// Initialize terminal with pre-opened file descriptor (for testing with PTY)
res_t ik_term_init_with_fd(TALLOC_CTX *ctx, ik_logger_t *logger, int tty_fd, ik_term_ctx_t **ctx_out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(ctx_out != NULL);   // LCOV_EXCL_BR_LINE

    // Allocate context
    ik_term_ctx_t *term_ctx = talloc_zero_(ctx, sizeof(ik_term_ctx_t));
    if (term_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    term_ctx->tty_fd = tty_fd;

    // Get original termios settings
    if (posix_tcgetattr_(tty_fd, &term_ctx->orig_termios) < 0) {
        posix_close_(tty_fd);
        return ERR(ctx, IO, "Failed to get terminal attributes");
    }

    // Set raw mode
    struct termios raw = term_ctx->orig_termios;
    raw.c_iflag &= (uint32_t)(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
    raw.c_oflag &= (uint32_t)(~(OPOST));
    raw.c_cflag |= (CS8);
    raw.c_lflag &= (uint32_t)(~(ECHO | ICANON | IEXTEN | ISIG));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    // Apply raw mode immediately (no blocking)
    errno = 0;  // Clear errno before call
    int tcset_ret = posix_tcsetattr_(tty_fd, TCSANOW, &raw);
    if (tcset_ret < 0) {
        posix_close_(tty_fd);
        return ERR(ctx, IO, "Failed to set raw mode");
    }

    // Flush any stale input that was queued before raw mode
    if (posix_tcflush_(tty_fd, TCIFLUSH) < 0) {
        posix_tcsetattr_(tty_fd, TCSANOW, &term_ctx->orig_termios);
        (void)posix_close_(tty_fd);  // Explicitly ignore return value
        return ERR(ctx, IO, "Failed to flush input");
    }

    // Enter alternate screen buffer
    if (posix_write_(tty_fd, ESC_ALT_SCREEN_ENTER, 8) < 0) {
        posix_tcsetattr_(tty_fd, TCSANOW, &term_ctx->orig_termios);
        posix_close_(tty_fd);
        return ERR(ctx, IO, "Failed to enter alternate screen");
    }

    // Clear screen once on initialization (subsequent renders use cursor positioning)
    if (posix_write_(tty_fd, "\x1b[2J\x1b[H", 6) < 0) {
        posix_tcsetattr_(tty_fd, TCSANOW, &term_ctx->orig_termios);
        posix_close_(tty_fd);
        return ERR(ctx, IO, "Failed to clear screen");
    }

    // Probe for CSI u support and enable if available
    term_ctx->csi_u_supported = probe_csi_u_support(tty_fd);
    if (term_ctx->csi_u_supported) {
        // Enable CSI u with flag 9 (disambiguate + report all keys)
        // This also reads and verifies the response
        if (!enable_csi_u(tty_fd, logger)) {
            // Not critical - continue without CSI u
            term_ctx->csi_u_supported = false;
        }
    }

    // Get terminal size
    struct winsize ws;
    if (posix_ioctl_(tty_fd, TIOCGWINSZ, &ws) < 0) {
        // Restore before returning error
        (void)posix_write_(tty_fd, ESC_ALT_SCREEN_EXIT, 8);
        posix_tcsetattr_(tty_fd, TCSANOW, &term_ctx->orig_termios);
        posix_close_(tty_fd);
        return ERR(ctx, IO, "Failed to get terminal size");
    }

    term_ctx->screen_rows = (int)ws.ws_row;
    term_ctx->screen_cols = (int)ws.ws_col;

    *ctx_out = term_ctx;
    return OK(term_ctx);
}

// Cleanup terminal (restore state)
void ik_term_cleanup(ik_term_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->tty_fd < 0) {
        return;
    }

    // Disable CSI u if it was enabled
    if (ctx->csi_u_supported) {
        (void)posix_write_(ctx->tty_fd, ESC_CSI_U_DISABLE, 4);
    }

    // Exit alternate screen
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

    if (ctx->tty_fd >= 0) {
        struct winsize ws;
        if (posix_ioctl_(ctx->tty_fd, TIOCGWINSZ, &ws) < 0) {
            return ERR(talloc_parent(ctx), IO, "Failed to get terminal size");
        }

        ctx->screen_rows = (int)ws.ws_row;
        ctx->screen_cols = (int)ws.ws_col;
    }

    *rows_out = ctx->screen_rows;
    *cols_out = ctx->screen_cols;

    return OK(NULL);
}
