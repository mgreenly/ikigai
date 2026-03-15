/**
 * @file bg_ansi.h
 * @brief ANSI escape sequence stripping for background process output
 *
 * Strips ANSI escape sequences from raw PTY output, producing clean
 * text suitable for LLM consumption. Raw output is preserved on disk;
 * this module is used only when serving output to the LLM via tools.
 *
 * Single-threaded. All calls must occur on the main thread.
 */

#ifndef IK_BG_ANSI_H
#define IK_BG_ANSI_H

#include <stddef.h>
#include <talloc.h>

/**
 * Strip ANSI escape sequences from a buffer.
 *
 * Removes CSI sequences (\x1b[...), OSC sequences (\x1b]...),
 * two-character escape sequences (\x1b + single byte), and any
 * partial/malformed sequences. Preserves all other bytes exactly.
 *
 * The returned string is NUL-terminated. Input bytes are not required
 * to be NUL-terminated; len controls how many bytes are processed.
 *
 * Partial sequences at end of input (e.g. bare \x1b with no following
 * byte, or an unterminated CSI) are silently dropped.
 *
 * @param ctx    Talloc parent. The returned buffer is a talloc child of ctx.
 * @param input  Input bytes (may be NULL if len == 0).
 * @param len    Number of bytes in input.
 * @return       Talloc-allocated NUL-terminated string with escapes removed.
 *               Never NULL; panics on OOM.
 */
char *bg_ansi_strip(TALLOC_CTX *ctx, const char *input, size_t len);

#endif /* IK_BG_ANSI_H */
