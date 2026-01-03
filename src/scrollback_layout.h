/**
 * @file scrollback_layout.h
 * @brief Layout calculation for scrollback lines
 */

#ifndef IKIGAI_SCROLLBACK_LAYOUT_H
#define IKIGAI_SCROLLBACK_LAYOUT_H

#include "error.h"
#include "scrollback.h"

#include <inttypes.h>
#include <stddef.h>

/**
 * @brief Calculate layout information for a line of text
 *
 * Scans UTF-8 text to calculate display width and physical lines needed.
 * Handles embedded newlines by segmenting the text and calculating rows
 * for each segment based on terminal width.
 *
 * @param parent Talloc parent context for segment_widths allocation
 * @param text Line text (UTF-8, may contain newlines)
 * @param length Text length in bytes
 * @param terminal_width Terminal width in columns
 * @param layout_out Pointer to receive calculated layout
 * @return RES_OK on success, RES_ERR on failure (OOM)
 *
 * Assertions:
 * - parent must not be NULL
 * - layout_out must not be NULL
 */
res_t ik_scrollback_calculate_layout(void *parent,
                                     const char *text,
                                     size_t length,
                                     int32_t terminal_width,
                                     ik_line_layout_t *layout_out);

#endif // IKIGAI_SCROLLBACK_LAYOUT_H
