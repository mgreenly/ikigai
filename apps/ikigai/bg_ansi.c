/**
 * @file bg_ansi.c
 * @brief ANSI escape sequence stripping for background process output
 */

#include "apps/ikigai/bg_ansi.h"

#include <stdint.h>
#include <talloc.h>

#include "shared/panic.h"

/* ================================================================
 * Internal helpers
 * ================================================================ */

/*
 * CSI sequence: \x1b[ followed by:
 *   - zero or more parameter bytes: 0x30-0x3F
 *   - zero or more intermediate bytes: 0x20-0x2F
 *   - one final byte: 0x40-0x7E
 *
 * Returns the index one past the final byte of the sequence,
 * or len if the sequence is unterminated (runs off the end).
 * On entry, i points to the '[' byte (the byte after \x1b).
 */
static size_t skip_csi(const char *input, size_t len, size_t i)
{
    /* skip '[' */
    i++;

    /* parameter bytes: 0x30-0x3F */
    while (i < len && (uint8_t)input[i] >= 0x30 && (uint8_t)input[i] <= 0x3F)
        i++;

    /* intermediate bytes: 0x20-0x2F */
    while (i < len && (uint8_t)input[i] >= 0x20 && (uint8_t)input[i] <= 0x2F)
        i++;

    /* final byte: 0x40-0x7E */
    if (i < len && (uint8_t)input[i] >= 0x40 && (uint8_t)input[i] <= 0x7E)
        i++;

    return i;
}

/*
 * OSC sequence: \x1b] followed by content terminated by:
 *   - ST: \x1b\ (two bytes)
 *   - BEL: \x07
 *
 * Returns the index one past the terminator,
 * or len if the sequence is unterminated.
 * On entry, i points to the ']' byte (the byte after \x1b).
 */
static size_t skip_osc(const char *input, size_t len, size_t i)
{
    /* skip ']' */
    i++;

    while (i < len) {
        if ((uint8_t)input[i] == 0x07) {
            /* BEL terminator */
            i++;
            return i;
        }
        if ((uint8_t)input[i] == 0x1B && i + 1 < len
                && (uint8_t)input[i + 1] == '\\') {
            /* ST terminator: \x1b\ */
            i += 2;
            return i;
        }
        i++;
    }

    return len; /* unterminated — drop to end */
}

/* ================================================================
 * Public API
 * ================================================================ */

char *bg_ansi_strip(TALLOC_CTX *ctx, const char *input, size_t len)
{
    if (len == 0) {
        char *empty = talloc_strdup(ctx, "");
        if (empty == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return empty;
    }

    /*
     * Output is at most len bytes plus a NUL. Allocate the full
     * input length — stripping only shrinks the output.
     */
    unsigned int alloc_len = (unsigned int)(len + 1);
    char *out = talloc_array(ctx, char, alloc_len);
    if (out == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t w = 0; /* write cursor */
    size_t i = 0; /* read cursor  */

    while (i < len) {
        if ((uint8_t)input[i] != 0x1B) {
            out[w++] = input[i++];
            continue;
        }

        /* ESC byte — look at the next byte */
        if (i + 1 >= len) {
            /* bare ESC at end of input: drop it */
            i++;
            continue;
        }

        uint8_t next = (uint8_t)input[i + 1];

        if (next == '[') {
            /* CSI sequence */
            i = skip_csi(input, len, i + 1);
        } else if (next == ']') {
            /* OSC sequence */
            i = skip_osc(input, len, i + 1);
        } else {
            /* Two-character escape (e.g. \x1bM, \x1b=, \x1b>) — drop both */
            i += 2;
        }
    }

    out[w] = '\0';

    /*
     * Resize the allocation to the actual output length.
     * talloc_realloc never fails when shrinking (returns same ptr).
     */
    unsigned int trim_len = (unsigned int)(w + 1);
    char *trimmed = talloc_realloc(ctx, out, char, trim_len);
    if (trimmed == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return trimmed;
}
