/**
 * @file scrollback_utils.h
 * @brief Utility functions for scrollback text analysis
 */

#ifndef IKIGAI_SCROLLBACK_UTILS_H
#define IKIGAI_SCROLLBACK_UTILS_H

#include <stddef.h>

/**
 * @brief Calculate display width of text, skipping ANSI escape sequences and newlines
 *
 * @param text   Text to measure
 * @param length Length of text
 * @return       Display width (sum of character widths)
 */
size_t ik_scrollback_calculate_display_width(const char *text, size_t length);

/**
 * @brief Count embedded newlines in text
 *
 * @param text   Text to scan
 * @param length Length of text
 * @return       Number of newline characters found
 */
size_t ik_scrollback_count_newlines(const char *text, size_t length);

/**
 * @brief Trim trailing whitespace from string
 *
 * Returns a new string with trailing whitespace removed.
 * Original string is not modified.
 *
 * @param parent Talloc parent context
 * @param text Input string (NULL returns empty string)
 * @param length Length of input string
 * @return New string with trailing whitespace removed (owned by parent)
 *
 * Assertions:
 * - parent must not be NULL
 */
char *ik_scrollback_trim_trailing(void *parent, const char *text, size_t length);

/**
 * @brief Format text as warning message with icon and color
 *
 * Applies warning styling: ⚠ prefix and yellow color (if colors enabled).
 * Returns a new string with styling applied. Use this for error messages
 * that should be displayed to the user.
 *
 * @param parent Talloc parent context
 * @param text Warning message text
 * @return Styled warning string (owned by parent), never NULL
 *
 * Assertions:
 * - parent must not be NULL
 * - text must not be NULL
 */
char *ik_scrollback_format_warning(void *parent, const char *text);

#endif // IKIGAI_SCROLLBACK_UTILS_H
