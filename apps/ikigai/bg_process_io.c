/**
 * @file bg_process_io.c
 * @brief Background process I/O — stdin write, output append, output read
 */

#include "apps/ikigai/bg_process_io.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <talloc.h>

#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper.h"

#include "shared/poison.h"

/* ================================================================
 * bg_process_write_stdin
 * ================================================================ */

res_t bg_process_write_stdin(bg_process_t *proc,
                             const char *data, size_t len,
                             bool append_newline)
{
    assert(proc != NULL); // LCOV_EXCL_BR_LINE

    if (data == NULL) {
        return ERR(proc, INVALID_ARG, "data is NULL");
    }

    if (!proc->stdin_open) {
        return ERR(proc, INVALID_ARG, "stdin is already closed");
    }

    if (len > 0) {
        ssize_t n = posix_write_(proc->master_fd, data, len);
        if (n < 0) {
            return ERR(proc, IO, "write to stdin failed: %s", strerror(errno));
        }
    }

    if (append_newline) {
        char nl = '\n';
        ssize_t n = posix_write_(proc->master_fd, &nl, 1);
        if (n < 0) {
            return ERR(proc, IO, "write newline to stdin failed: %s",
                       strerror(errno));
        }
    }

    return OK(NULL);
}

/* ================================================================
 * bg_process_close_stdin
 * ================================================================ */

res_t bg_process_close_stdin(bg_process_t *proc)
{
    assert(proc != NULL); // LCOV_EXCL_BR_LINE

    if (!proc->stdin_open) {
        return OK(NULL);
    }

    char eof_byte = '\x04';
    ssize_t n = posix_write_(proc->master_fd, &eof_byte, 1);
    if (n < 0) {
        return ERR(proc, IO, "write Ctrl-D to stdin failed: %s", strerror(errno));
    }

    proc->stdin_open = false;
    return OK(NULL);
}

/* ================================================================
 * bg_process_append_output
 * ================================================================ */

res_t bg_process_append_output(bg_process_t *proc,
                               const uint8_t *data, size_t len)
{
    assert(proc != NULL); // LCOV_EXCL_BR_LINE

    if (data == NULL || len == 0) {
        return ERR(proc, INVALID_ARG, "data is NULL or len is 0");
    }

    if (proc->output_fd >= 0) {
        ssize_t n = posix_write_(proc->output_fd, data, len);
        if (n < 0) {
            return ERR(proc, IO, "write to output file failed: %s",
                       strerror(errno));
        }
    }

    bg_line_index_append(proc->line_index, data, len);
    proc->total_bytes += (int64_t)len;

    return OK(NULL);
}

/* ================================================================
 * bg_process_read_output
 * ================================================================ */

res_t bg_process_read_output(bg_process_t *proc,
                             TALLOC_CTX *ctx,
                             bg_read_mode_t mode,
                             int64_t tail_lines,
                             int64_t start_line,
                             int64_t end_line,
                             uint8_t **out_buf,
                             size_t *out_len)
{
    assert(proc != NULL); // LCOV_EXCL_BR_LINE

    if (ctx == NULL || out_buf == NULL || out_len == NULL) {
        return ERR(proc, INVALID_ARG, "ctx, out_buf, or out_len is NULL");
    }

    *out_buf = NULL;
    *out_len = 0;

    if (mode == BG_READ_SINCE_LAST) {
        if (proc->cursor >= proc->total_bytes) {
            return OK(NULL); /* no new output since last read */
        }

        if (proc->output_fd < 0) {
            return ERR(proc, IO, "output file not open");
        }

        size_t bytes_to_read = (size_t)(proc->total_bytes - proc->cursor);
        uint8_t *buf = talloc_size(ctx, bytes_to_read + 1);
        if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        ssize_t n = posix_pread_(proc->output_fd, buf, bytes_to_read,
                                 proc->cursor);
        if (n < 0) {
            talloc_free(buf);
            return ERR(proc, IO, "pread failed: %s", strerror(errno));
        }

        buf[n] = '\0';
        proc->cursor += (int64_t)n;

        *out_buf = buf;
        *out_len = (size_t)n;
        return OK(NULL);
    }

    int64_t total_lines = bg_line_index_count(proc->line_index);

    if (mode == BG_READ_TAIL) {
        if (tail_lines <= 0) {
            return ERR(proc, INVALID_ARG, "tail_lines must be > 0");
        }
        if (total_lines == 0) {
            return OK(NULL);
        }
        start_line = total_lines - tail_lines + 1;
        if (start_line < 1) start_line = 1;
        end_line = total_lines;
    }

    /* BG_READ_RANGE (or tail after resolving start/end) */
    if (total_lines == 0) {
        return OK(NULL);
    }

    off_t  offset = 0;
    size_t length = 0;
    res_t r = bg_line_index_get_range(proc->line_index, start_line, end_line,
                                     &offset, &length);
    if (is_err(&r)) return r;

    if (length == 0) {
        return OK(NULL);
    }

    if (proc->output_fd < 0) {
        return ERR(proc, IO, "output file not open");
    }

    uint8_t *buf = talloc_size(ctx, length + 1);
    if (buf == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ssize_t n = posix_pread_(proc->output_fd, buf, length, offset);
    if (n < 0) {
        talloc_free(buf);
        return ERR(proc, IO, "pread failed: %s", strerror(errno));
    }

    buf[n] = '\0';

    *out_buf = buf;
    *out_len = (size_t)n;
    return OK(NULL);
}
