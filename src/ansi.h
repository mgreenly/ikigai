#ifndef IK_ANSI_H
#define IK_ANSI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Color constants (256-color palette indices)
#define IK_ANSI_GRAY_SUBDUED 242
#define IK_ANSI_GRAY_LIGHT   249

// SGR sequence literals
#define IK_ANSI_RESET "\x1b[0m"

/**
 * Skip ANSI CSI (Control Sequence Introducer) escape sequence if present at position.
 *
 * CSI sequences are used for terminal control, including SGR (Select Graphic Rendition)
 * color codes. They have the format: ESC [ <params> <terminal_byte>
 *
 * Sequence structure:
 * - ESC '[' (0x1b 0x5b) - CSI introducer
 * - Parameter bytes (0x30-0x3F): digits, semicolons, etc.
 * - Intermediate bytes (0x20-0x2F): optional modifiers
 * - Terminal byte (0x40-0x7E): command letter (e.g., 'm' for SGR)
 *
 * Examples:
 * - \x1b[0m          - Reset (4 bytes)
 * - \x1b[38;5;242m   - 256-color foreground (11 bytes)
 * - \x1b[48;5;249m   - 256-color background (11 bytes)
 *
 * @param text  Text buffer containing potential escape sequence
 * @param len   Length of text buffer
 * @param pos   Position to check for escape sequence start
 * @return      Number of bytes in the CSI sequence, or 0 if not a valid CSI sequence
 */
size_t ik_ansi_skip_csi(const char *text, size_t len, size_t pos);

/**
 * Build foreground color sequence into buffer using 256-color palette.
 *
 * Generates a CSI SGR sequence to set foreground color to a 256-color palette index.
 * Format: \x1b[38;5;<color>m
 *
 * Examples:
 * - color 0:   "\x1b[38;5;0m"   (10 bytes + null)
 * - color 242: "\x1b[38;5;242m" (12 bytes + null)
 * - color 255: "\x1b[38;5;255m" (12 bytes + null)
 *
 * @param buf       Buffer to write the sequence into
 * @param buf_size  Size of buffer (must be at least 12 bytes for 256-color)
 * @param color     256-color palette index (0-255)
 * @return          Number of bytes written (excluding null terminator), or 0 on error
 */
size_t ik_ansi_fg_256(char *buf, size_t buf_size, uint8_t color);


/**
 * Check if colors are enabled.
 *
 * Returns the current color enablement state, which is set by ik_ansi_init().
 * If ik_ansi_init() has not been called, colors are enabled by default.
 *
 * @return true if colors should be used, false otherwise
 */
bool ik_ansi_colors_enabled(void);

#endif // IK_ANSI_H
