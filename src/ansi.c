#include "ansi.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t ik_ansi_skip_csi(const char *text, size_t len, size_t pos)
{
    // Check if we have enough space for ESC + '['
    if (pos + 1 >= len) {
        return 0;
    }

    // Check if we're at the start of a CSI sequence: ESC '['
    if (text[pos] != '\x1b' || text[pos + 1] != '[') {
        return 0;
    }

    // Start at position after ESC '['
    size_t i = pos + 2;

    // Skip parameter bytes (0x30-0x3F) and intermediate bytes (0x20-0x2F)
    while (i < len) {
        unsigned char c = (unsigned char)text[i];

        // Parameter bytes: 0x30-0x3F (digits 0-9, semicolon, etc.)
        // Intermediate bytes: 0x20-0x2F (space, !, ", etc.)
        if ((c >= 0x30 && c <= 0x3F) || (c >= 0x20 && c <= 0x2F)) {
            i++;
            continue;
        }

        // Terminal byte: 0x40-0x7E (uppercase letters, @, etc.)
        if (c >= 0x40 && c <= 0x7E) {
            // Found terminal byte - return total bytes consumed
            return i + 1 - pos;
        }

        // Invalid character - not a valid CSI sequence
        return 0;
    }

    // Reached end of buffer without finding terminal byte
    return 0;
}

size_t ik_ansi_fg_256(char *buf, size_t buf_size, uint8_t color)
{
    // Format: \x1b[38;5;<color>m
    // Use snprintf to format the sequence
    int32_t written = snprintf(buf, buf_size, "\x1b[38;5;%" PRIu8 "m", color);

    // snprintf returns negative on error
    if (written < 0) { // LCOV_EXCL_BR_LINE
        return 0;      // LCOV_EXCL_LINE
    }

    // Check if buffer was too small (snprintf returns what would have been written)
    if ((size_t)written >= buf_size) {
        return 0;
    }

    // Return bytes written (excluding null terminator)
    return (size_t)written;
}

// Global color state
static bool g_colors_enabled = true;


bool ik_ansi_colors_enabled(void)
{
    return g_colors_enabled;
}
