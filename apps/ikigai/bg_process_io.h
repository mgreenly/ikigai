/**
 * @file bg_process_io.h
 * @brief Background process I/O — stdin write, output append, output read
 *
 * Extends bg_process_t with I/O operations: writing to stdin via the PTY
 * master_fd, appending raw output bytes to disk and updating the line index,
 * and reading output from disk via pread using three modes (tail, range,
 * since-last).
 *
 * All operations are single-threaded (main thread only).
 */

#ifndef IK_BG_PROCESS_IO_H
#define IK_BG_PROCESS_IO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <talloc.h>

#include "shared/error.h"
#include "apps/ikigai/bg_process.h"

/**
 * Output read mode for bg_process_read_output().
 */
typedef enum {
    BG_READ_TAIL,       /**< Last tail_lines complete lines.                  */
    BG_READ_RANGE,      /**< Lines start_line..end_line (1-indexed, inclusive).*/
    BG_READ_SINCE_LAST, /**< Bytes from cursor to end; advances cursor.       */
} bg_read_mode_t;

/**
 * Write input to the process stdin (PTY master_fd).
 *
 * Appends a newline when append_newline is true. Returns ERR_IO if the
 * write fails (e.g., process already exited and PTY returns EIO). Returns
 * ERR_INVALID_ARG if stdin has already been closed via bg_process_close_stdin().
 *
 * @param proc           Process. Must not be NULL.
 * @param data           Bytes to write. Must not be NULL.
 * @param len            Number of bytes in data. 0 is a no-op for data.
 * @param append_newline If true, write '\n' after data.
 * @return OK on success.
 *         ERR_INVALID_ARG if proc or data is NULL, or if stdin is closed.
 *         ERR_IO if the write fails.
 */
res_t bg_process_write_stdin(bg_process_t *proc,
                             const char *data, size_t len,
                             bool append_newline);

/**
 * Close stdin on a PTY process by writing Ctrl-D (\x04).
 *
 * Sets proc->stdin_open = false after writing. The master_fd remains open
 * for reading output. No-op (returns OK) if stdin is already closed.
 *
 * @param proc  Process. Must not be NULL.
 * @return OK on success.
 *         ERR_IO if the write fails.
 */
res_t bg_process_close_stdin(bg_process_t *proc);

/**
 * Append raw bytes to the disk output file and update the line index.
 *
 * Writes data to proc->output_fd and records newline byte offsets in
 * proc->line_index. Updates proc->total_bytes by len. Called by the event
 * loop reader when data arrives on master_fd.
 *
 * No-op if output_fd is -1 (file not open); still updates the line index.
 *
 * @param proc  Process. Must not be NULL.
 * @param data  Bytes to append. Must not be NULL.
 * @param len   Number of bytes in data. Must be > 0.
 * @return OK on success.
 *         ERR_INVALID_ARG if proc or data is NULL, or len is 0.
 *         ERR_IO if the write to output_fd fails.
 */
res_t bg_process_append_output(bg_process_t *proc,
                               const uint8_t *data, size_t len);

/**
 * Read output from the disk output file using the line index.
 *
 * Allocates a NUL-terminated buffer (talloc child of ctx) containing the
 * requested output. Returns OK with *out_buf=NULL and *out_len=0 when there
 * is nothing to return (empty file, out-of-bounds range, or no new output
 * since last cursor position).
 *
 * Modes:
 *   BG_READ_TAIL       — last tail_lines complete lines (clamped to available).
 *   BG_READ_RANGE      — lines start_line..end_line (1-indexed, inclusive).
 *   BG_READ_SINCE_LAST — bytes from proc->cursor to proc->total_bytes; advances
 *                        proc->cursor to proc->total_bytes after the read.
 *
 * TAIL and RANGE modes do not update proc->cursor.
 *
 * @param proc        Process. Must not be NULL.
 * @param ctx         Talloc parent for the output buffer. Must not be NULL.
 * @param mode        Read mode.
 * @param tail_lines  Lines to return in BG_READ_TAIL mode. Must be > 0.
 * @param start_line  First line in BG_READ_RANGE mode (1-indexed).
 * @param end_line    Last line in BG_READ_RANGE mode (1-indexed, inclusive).
 * @param out_buf     Set to allocated buffer on success; NULL if nothing to read.
 * @param out_len     Set to byte count of *out_buf on success; 0 if nothing.
 * @return OK on success.
 *         ERR_INVALID_ARG if required pointers are NULL or tail_lines <= 0.
 *         ERR_OUT_OF_RANGE if BG_READ_RANGE requests lines outside [1..count].
 *         ERR_IO if pread fails or output_fd is -1.
 */
res_t bg_process_read_output(bg_process_t *proc,
                             TALLOC_CTX *ctx,
                             bg_read_mode_t mode,
                             int64_t tail_lines,
                             int64_t start_line,
                             int64_t end_line,
                             uint8_t **out_buf,
                             size_t *out_len);

#endif /* IK_BG_PROCESS_IO_H */
