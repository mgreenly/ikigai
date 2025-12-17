/**
 * @file scrollback.h
 * @brief Scrollback buffer for terminal output history
 *
 * Provides a scrollback buffer that stores historical output lines with
 * pre-computed layout information for efficient rendering and reflow.
 *
 * Design principles:
 * - Hot/cold data separation: Layout info (hot) is separate from text (cold)
 * - Pre-computed display widths: UTF-8 width calculated once on line creation
 * - O(1) arithmetic reflow: Physical lines = display_width / terminal_width
 * - Single contiguous text buffer for cache locality
 */

#ifndef IKIGAI_SCROLLBACK_H
#define IKIGAI_SCROLLBACK_H

#include "error.h"
#include <inttypes.h>
#include <stddef.h>

/**
 * @brief Layout information for a single line
 *
 * Pre-computed layout data that enables O(1) reflow on terminal resize.
 */
typedef struct {
    size_t display_width;    /**< Display width in columns (UTF-8 aware) */
    size_t physical_lines;   /**< Number of terminal rows this line occupies */
    size_t newline_count;    /**< Number of embedded newlines in text */
    size_t *segment_widths;  /**< Array of display widths between newlines */
} ik_line_layout_t;

/**
 * @brief Scrollback buffer context
 *
 * Stores historical output lines with separated hot/cold data:
 * - Hot data: layouts array (accessed during rendering/reflow)
 * - Cold data: text_buffer (only accessed when displaying specific lines)
 *
 * Lines are stored in insertion order (oldest first).
 */
typedef struct ik_scrollback_t {
    // Text storage (cold data - infrequently accessed)
    char *text_buffer;           /**< Single contiguous buffer for all line text */
    size_t *text_offsets;        /**< Array of offsets into text_buffer (one per line) */
    size_t *text_lengths;        /**< Array of text lengths in bytes (one per line) */

    // Layout storage (hot data - frequently accessed during rendering)
    ik_line_layout_t *layouts;   /**< Array of pre-computed layouts (one per line) */

    // Metadata
    size_t count;                /**< Number of lines currently stored */
    size_t capacity;             /**< Maximum lines before reallocation */
    int32_t cached_width;        /**< Terminal width used for layout calculations */
    size_t total_physical_lines; /**< Sum of all physical_lines (for viewport) */

    // Buffer management
    size_t buffer_used;          /**< Bytes used in text_buffer */
    size_t buffer_capacity;      /**< Total size of text_buffer in bytes */
} ik_scrollback_t;

/**
 * @brief Create a new scrollback buffer
 *
 * Allocates and initializes a scrollback buffer with specified terminal width.
 * Initial capacity is set to a reasonable default (16 lines, 1KB text buffer).
 *
 * @param parent Talloc parent context
 * @param terminal_width Terminal width in columns (must be > 0)
 * @return Pointer to allocated scrollback buffer (never NULL - PANICs on OOM)
 *
 * Assertions:
 * - terminal_width must be > 0
 */
ik_scrollback_t *ik_scrollback_create(void *parent, int32_t terminal_width);

/**
 * @brief Append a line to the scrollback buffer
 *
 * Adds a new line to the scrollback buffer. Calculates display width by
 * scanning UTF-8 text and computing physical lines based on current
 * terminal width. Grows arrays if needed (capacity doubling).
 *
 * @param scrollback Scrollback buffer
 * @param text Line text (UTF-8, may contain newlines)
 * @param length Text length in bytes
 * @return RES_OK on success, RES_ERR on failure (OOM)
 *
 * Assertions:
 * - scrollback must not be NULL
 * - text must not be NULL (unless length == 0)
 */
res_t ik_scrollback_append_line(ik_scrollback_t *scrollback, const char *text, size_t length);

/**
 * @brief Ensure layout cache is valid for given terminal width
 *
 * Recalculates physical_lines for all lines if terminal width has changed.
 * Uses O(n) arithmetic (no UTF-8 rescanning): physical_lines = display_width / width
 *
 * @param scrollback Scrollback buffer
 * @param terminal_width Terminal width in columns (must be > 0)
 *
 * Assertions:
 * - scrollback must not be NULL
 * - terminal_width must be > 0
 */
void ik_scrollback_ensure_layout(ik_scrollback_t *scrollback, int32_t terminal_width);

/**
 * @brief Get number of logical lines in scrollback
 *
 * @param scrollback Scrollback buffer
 * @return Number of lines
 *
 * Assertions:
 * - scrollback must not be NULL
 */
size_t ik_scrollback_get_line_count(const ik_scrollback_t *scrollback);

/**
 * @brief Get total number of physical lines (terminal rows)
 *
 * Returns the sum of all physical_lines across all logical lines.
 * Used for viewport calculation.
 *
 * @param scrollback Scrollback buffer
 * @return Total physical lines
 *
 * Assertions:
 * - scrollback must not be NULL
 */
size_t ik_scrollback_get_total_physical_lines(const ik_scrollback_t *scrollback);

/**
 * @brief Get text for a specific line
 *
 * Returns a pointer into the internal text buffer for the specified line.
 * The returned pointer is valid until the next scrollback modification.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Line index (0-based)
 * @param text_out Pointer to receive text buffer
 * @param length_out Pointer to receive text length
 * @return RES_OK on success, RES_ERR on failure (invalid index)
 *
 * Assertions:
 * - scrollback must not be NULL
 * - text_out must not be NULL
 * - length_out must not be NULL
 */
res_t ik_scrollback_get_line_text(ik_scrollback_t *scrollback,
                                  size_t line_index,
                                  const char **text_out,
                                  size_t *length_out);

/**
 * @brief Find logical line index at a given physical row
 *
 * Given a physical row number (0-based from top of scrollback), finds
 * which logical line contains that row and the offset within that line.
 *
 * @param scrollback Scrollback buffer
 * @param physical_row Physical row index (0-based)
 * @param line_index_out Pointer to receive logical line index
 * @param row_offset_out Pointer to receive row offset within line
 * @return RES_OK on success, RES_ERR if physical_row is out of range
 *
 * Assertions:
 * - scrollback must not be NULL
 * - line_index_out must not be NULL
 * - row_offset_out must not be NULL
 */
res_t ik_scrollback_find_logical_line_at_physical_row(ik_scrollback_t *scrollback,
                                                      size_t physical_row,
                                                      size_t *line_index_out,
                                                      size_t *row_offset_out);

/**
 * @brief Clear all lines from the scrollback buffer
 *
 * Removes all lines from the scrollback buffer, resetting it to an empty state.
 * Preserves allocated capacity for efficient reuse.
 *
 * @param scrollback Scrollback buffer
 *
 * Assertions:
 * - scrollback must not be NULL
 */
void ik_scrollback_clear(ik_scrollback_t *scrollback);

/**
 * @brief Copy all lines from source scrollback to destination scrollback
 *
 * Copies all lines from the source scrollback buffer to the destination.
 * The destination scrollback should typically be empty before calling this.
 *
 * @param dest Destination scrollback buffer
 * @param src Source scrollback buffer
 * @return RES_OK on success, RES_ERR on failure (OOM)
 *
 * Assertions:
 * - dest must not be NULL
 * - src must not be NULL
 */
res_t ik_scrollback_copy_from(ik_scrollback_t *dest, const ik_scrollback_t *src);

/**
 * @brief Get byte offset at a given display column within a line
 *
 * Iterates through the line text, tracking display width while skipping
 * ANSI escape sequences, to find the byte offset where the specified
 * display column begins. Used for partial line rendering when scrolling.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Logical line index (0-based)
 * @param display_col Target display column (0-based)
 * @param byte_offset_out Pointer to receive byte offset
 * @return RES_OK on success, RES_ERR if line_index is out of range
 *
 * Notes:
 * - If display_col is beyond the line's display width, returns end of line
 * - ANSI escape sequences are skipped (they have 0 display width)
 * - UTF-8 multi-byte characters are handled correctly
 * - Wide characters (CJK) are counted as 2 display columns
 *
 * Assertions:
 * - scrollback must not be NULL
 * - byte_offset_out must not be NULL
 */
res_t ik_scrollback_get_byte_offset_at_display_col(ik_scrollback_t *scrollback,
                                                    size_t line_index,
                                                    size_t display_col,
                                                    size_t *byte_offset_out);

/**
 * @brief Calculate starting byte offset for rendering from a row offset
 *
 * Given a logical line and a starting row offset, calculates the corresponding
 * byte offset in the line's text. Handles embedded newlines using segment widths.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Logical line index
 * @param terminal_width Current terminal width
 * @param start_row_offset Physical row offset from start of line (0-based)
 * @return Byte offset into line text
 *
 * Assertions:
 * - scrollback must not be NULL
 */
size_t ik_scrollback_calc_start_byte_for_row(ik_scrollback_t *scrollback,
                                              size_t line_index,
                                              size_t terminal_width,
                                              size_t start_row_offset);

/**
 * @brief Calculate ending byte offset for rendering a row range
 *
 * Given a logical line and an ending row offset, calculates the corresponding
 * ending byte offset (exclusive) in the line's text. Returns whether we're
 * rendering to the end of the logical line.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Logical line index
 * @param terminal_width Current terminal width
 * @param end_row_offset Physical row offset from start of line (0-based, inclusive)
 * @param is_line_end_out Pointer to receive flag: true if rendering to end of line
 * @return Ending byte offset (exclusive)
 *
 * Assertions:
 * - scrollback must not be NULL
 * - is_line_end_out must not be NULL
 */
size_t ik_scrollback_calc_end_byte_for_row(ik_scrollback_t *scrollback,
                                            size_t line_index,
                                            size_t terminal_width,
                                            size_t end_row_offset,
                                            bool *is_line_end_out);

/**
 * @brief Calculate byte range for rendering physical rows within a logical line
 *
 * Given a logical line and a range of physical rows to render within that line,
 * calculates the corresponding byte offset range in the line's text. Handles
 * embedded newlines correctly by using segment widths.
 *
 * @param scrollback Scrollback buffer
 * @param line_index Logical line index
 * @param terminal_width Current terminal width
 * @param start_row_offset Physical row offset from start of line (0-based)
 * @param row_count Number of physical rows to include
 * @param start_byte_out Pointer to receive starting byte offset
 * @param end_byte_out Pointer to receive ending byte offset (exclusive)
 * @param is_line_end_out Pointer to receive flag: true if rendering to end of logical line
 *
 * Assertions:
 * - scrollback must not be NULL
 * - start_byte_out must not be NULL
 * - end_byte_out must not be NULL
 * - is_line_end_out must not be NULL
 */
void ik_scrollback_calc_byte_range_for_rows(ik_scrollback_t *scrollback,
                                             size_t line_index,
                                             size_t terminal_width,
                                             size_t start_row_offset,
                                             size_t row_count,
                                             size_t *start_byte_out,
                                             size_t *end_byte_out,
                                             bool *is_line_end_out);

#endif // IKIGAI_SCROLLBACK_H
