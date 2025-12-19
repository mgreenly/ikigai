// LCOV_EXCL_START - Panic handlers cannot be tested (they abort the process)
#include "panic.h"
#include "logger.h"
#include "terminal.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Global terminal context for panic handler
ik_term_ctx_t *g_term_ctx_for_panic = NULL;

// Global logger context for panic handler
ik_logger_t *volatile g_panic_logger = NULL;

/**
 * Safe string length calculation (async-signal-safe).
 * Avoids calling strlen() which may not be async-signal-safe.
 */
static size_t safe_strlen(const char *s)
{
    size_t len = 0;
    if (s) {
        while (s[len]) {
            len++;
        }
    }
    return len;
}

/**
 * Convert integer to string (async-signal-safe).
 * Hand-rolled to avoid snprintf() and other non-async-signal-safe functions.
 *
 * @param n Number to convert
 * @param buf Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
static int32_t int_to_str(int32_t n, char *buf, size_t size)
{
    if (size == 0) {
        return 0;
    }

    int32_t i = 0;
    int32_t is_neg = (n < 0);
    if (is_neg) {
        n = -n;
    }

    // Convert digits in reverse
    char tmp[16];
    int32_t j = 0;
    do {
        tmp[j++] = (char)('0' + (n % 10));
        n /= 10;
    } while (n > 0 && j < 15);

    // Add negative sign
    if (is_neg && i < (int32_t)(size - 1)) {
        buf[i++] = '-';
    }

    // Reverse into output buffer
    while (j > 0 && i < (int32_t)(size - 1)) {
        buf[i++] = tmp[--j];
    }
    buf[i] = '\0';
    return i;
}

/**
 * Helper to write data and explicitly ignore the result.
 * Required for _FORTIFY_SOURCE=2 where (void) cast doesn't suppress warnings.
 */
static inline void write_ignore(int fd, const void *buf, size_t count)
{
    ssize_t result = write(fd, buf, count);
    (void)result;  // Explicitly ignore - we're in a panic handler
}

/**
 * Async-signal-safe panic implementation.
 *
 * This function:
 * 1. Restores terminal if g_term_ctx_for_panic is set
 * 2. Writes error message using only write() syscalls
 * 3. Calls abort()
 *
 * IMPORTANT: This function is async-signal-safe (with one caveat).
 * - Uses only write() for output (async-signal-safe)
 * - Uses only stack allocations (no malloc)
 * - tcsetattr() is NOT async-signal-safe but necessary for cleanup
 *
 * @param msg Error message (should be string literal)
 * @param file Source file name (typically __FILE__)
 * @param line Source line number (typically __LINE__)
 */
void ik_panic_impl(const char *msg, const char *file, int32_t line)
{
    // Restore terminal state if available
    if (g_term_ctx_for_panic != NULL) {
        // Full terminal reset sequence:
        // - Show cursor (may be hidden in scrollback mode)
        // - Reset text attributes (future-proof for colors)
        // - Exit alternate screen buffer
        const char reset_seq[] = "\x1b[?25h\x1b[0m\x1b[?1049l";
        write_ignore(g_term_ctx_for_panic->tty_fd, reset_seq, 18);

        // Restore original termios
        // NOTE: tcsetattr() is not async-signal-safe per POSIX,
        // but it's necessary for terminal cleanup and generally safe in practice
        tcsetattr(g_term_ctx_for_panic->tty_fd, TCSANOW,
                  &g_term_ctx_for_panic->orig_termios);
    }

    // Best-effort logger write BEFORE stderr output
    ik_logger_t *logger = g_panic_logger;  // Read volatile once
    if (logger != NULL) {
        int fd = ik_logger_get_fd(logger);
        if (fd >= 0) {
            char buf[512];
            int len = snprintf(buf, sizeof(buf),
                "{\"level\":\"fatal\",\"event\":\"panic\","
                "\"message\":\"%s\",\"file\":\"%s\",\"line\":%d}\n",
                msg ? msg : "", file ? file : "", line);
            if (len > 0 && (size_t)len < sizeof(buf)) {
                write_ignore(fd, buf, (size_t)len);
            }
        }
    }

    // Format line number
    char line_buf[16];
    int32_t line_len = int_to_str(line, line_buf, sizeof(line_buf));

    // Write error message to stderr using only async-signal-safe write()
    // Note: Using write_ignore() helper to satisfy _FORTIFY_SOURCE=2
    write_ignore(STDERR_FILENO, "FATAL: ", 7);
    if (msg) {
        write_ignore(STDERR_FILENO, msg, safe_strlen(msg));
    }
    write_ignore(STDERR_FILENO, "\n  at ", 6);
    if (file) {
        write_ignore(STDERR_FILENO, file, safe_strlen(file));
    }
    write_ignore(STDERR_FILENO, ":", 1);
    write_ignore(STDERR_FILENO, line_buf, (size_t)line_len);
    write_ignore(STDERR_FILENO, "\n", 1);

    // Abort process
    abort();
}

/**
 * Talloc abort handler.
 * Called when talloc detects internal errors (e.g., double free, corruption).
 *
 * @param reason Reason string from talloc (may be NULL)
 */
void ik_talloc_abort_handler(const char *reason)
{
    ik_panic_impl(reason ? reason : "talloc abort", "<talloc>", 0);
}

// LCOV_EXCL_STOP
