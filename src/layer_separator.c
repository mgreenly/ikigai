// Separator layer wrapper
#include "layer_wrappers.h"
#include "panic.h"
#include "scrollback_utils.h"
#include "wrapper.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// Unicode box-drawing character U+2500 (─) in UTF-8: 0xE2 0x94 0x80
#define BOX_DRAWING_LIGHT_HORIZONTAL "\xE2\x94\x80"
#define BOX_DRAWING_LIGHT_HORIZONTAL_LEN 3

// ANSI escape codes
#define ANSI_DIM "\x1b[2m"
#define ANSI_RESET "\x1b[0m"

// Unicode navigation arrows (UTF-8)
#define ARROW_UP "\xE2\x86\x91"      // ↑ U+2191
#define ARROW_DOWN "\xE2\x86\x93"    // ↓ U+2193
#define ARROW_LEFT "\xE2\x86\x90"    // ← U+2190
#define ARROW_RIGHT "\xE2\x86\x92"   // → U+2192

// Debug info pointers (optional - can be NULL)
typedef struct {
    size_t *viewport_offset;     // Scroll offset
    size_t *viewport_row;        // First visible row
    size_t *viewport_height;     // Terminal rows
    size_t *document_height;     // Total document height
    uint64_t *render_elapsed_us; // Elapsed time from previous render
} ik_separator_debug_t;

// Navigation context (for agent tree navigation)
typedef struct {
    const char *parent_uuid;        // Parent agent UUID (NULL if root)
    const char *prev_sibling_uuid;  // Previous sibling UUID (NULL if none)
    const char *current_uuid;       // Current agent UUID (required)
    const char *next_sibling_uuid;  // Next sibling UUID (NULL if none)
    size_t child_count;             // Number of children (0 if none)
} ik_separator_nav_context_t;

// Separator layer data
typedef struct {
    bool *visible_ptr;                  // Raw pointer to visibility flag
    ik_separator_debug_t debug;         // Debug info pointers (all can be NULL)
    ik_separator_nav_context_t nav_ctx; // Navigation context
} ik_separator_layer_data_t;

// Separator layer callbacks
static bool separator_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);           // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL);     // LCOV_EXCL_BR_LINE

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;
    return *data->visible_ptr;
}

static size_t separator_get_height(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 1; // Separator is always 1 row
}

static void separator_render(const ik_layer_t *layer,
                             ik_output_buffer_t *output,
                             size_t width,
                             size_t start_row,
                             size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE
    (void)start_row; // Separator is only 1 row, so start_row is always 0
    (void)row_count; // Always render the full separator

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;

    // Build navigation context string if current_uuid is set
    char nav_str[256] = "";
    size_t nav_len = 0;

    if (data->nav_ctx.current_uuid != NULL) {
        char parent_str[32];
        char prev_str[32];
        char curr_str[32];
        char next_str[32];
        char child_str[32];

        // Format parent indicator
        if (data->nav_ctx.parent_uuid != NULL) {
            snprintf(parent_str, sizeof(parent_str), ARROW_UP "%.6s...", data->nav_ctx.parent_uuid);
        } else {
            snprintf(parent_str, sizeof(parent_str), ANSI_DIM ARROW_UP "-" ANSI_RESET);
        }

        // Format prev sibling indicator
        if (data->nav_ctx.prev_sibling_uuid != NULL) {
            snprintf(prev_str, sizeof(prev_str), ARROW_LEFT "%.6s...", data->nav_ctx.prev_sibling_uuid);
        } else {
            snprintf(prev_str, sizeof(prev_str), ANSI_DIM ARROW_LEFT "-" ANSI_RESET);
        }

        // Format current agent (truncated)
        snprintf(curr_str, sizeof(curr_str), "[%.6s...]", data->nav_ctx.current_uuid);

        // Format next sibling indicator
        if (data->nav_ctx.next_sibling_uuid != NULL) {
            snprintf(next_str, sizeof(next_str), ARROW_RIGHT "%.6s...", data->nav_ctx.next_sibling_uuid);
        } else {
            snprintf(next_str, sizeof(next_str), ANSI_DIM ARROW_RIGHT "-" ANSI_RESET);
        }

        // Format child count indicator
        if (data->nav_ctx.child_count > 0) {
            snprintf(child_str, sizeof(child_str), ARROW_DOWN "%zu", data->nav_ctx.child_count);
        } else {
            snprintf(child_str, sizeof(child_str), ANSI_DIM ARROW_DOWN "-" ANSI_RESET);
        }

        // Assemble navigation context string
        nav_len = (size_t)snprintf(nav_str, sizeof(nav_str), " %s %s %s %s %s ",
                                   parent_str, prev_str, curr_str, next_str, child_str);
    }

    // Build debug string if debug info available
    char debug_str[128] = "";
    size_t debug_len = 0;

    if (data->debug.viewport_offset != NULL) {
        // Calculate scrollback rows from doc height: doc = sb + 1 + input(1) + 1
        size_t sb_rows = 0;
        if (data->debug.document_height && *data->debug.document_height >= 3) {
            sb_rows = *data->debug.document_height - 3;
        }
        // Get elapsed time from previous render
        uint64_t render_us = 0;
        if (data->debug.render_elapsed_us) {
            render_us = *data->debug.render_elapsed_us;
        }
        // Format render time: show in ms if >= 1000us, otherwise us
        if (render_us >= 1000) {
            // LCOV_EXCL_BR_START - Defensive: these pointers set together with viewport_offset
            debug_len = (size_t)snprintf(debug_str, sizeof(debug_str),
                                         " off=%zu row=%zu h=%zu doc=%zu sb=%zu t=%.1fms ",
                                         data->debug.viewport_offset ? *data->debug.viewport_offset : 0,
                                         data->debug.viewport_row ? *data->debug.viewport_row : 0,
                                         data->debug.viewport_height ? *data->debug.viewport_height : 0,
                                         data->debug.document_height ? *data->debug.document_height : 0,
                                         sb_rows,
                                         (double)render_us / 1000.0);
            // LCOV_EXCL_BR_STOP
        } else {
            // LCOV_EXCL_BR_START - Defensive: these pointers set together with viewport_offset
            debug_len = (size_t)snprintf(debug_str, sizeof(debug_str),
                                         " off=%zu row=%zu h=%zu doc=%zu sb=%zu t=%" PRIu64 "us ",
                                         data->debug.viewport_offset ? *data->debug.viewport_offset : 0,
                                         data->debug.viewport_row ? *data->debug.viewport_row : 0,
                                         data->debug.viewport_height ? *data->debug.viewport_height : 0,
                                         data->debug.document_height ? *data->debug.document_height : 0,
                                         sb_rows,
                                         render_us);
            // LCOV_EXCL_BR_STOP
        }
    }

    // Calculate VISUAL width of info strings (excluding ANSI escape codes)
    // nav_str and debug_str contain ANSI escape codes that consume bytes but
    // don't contribute to visual width. Use ik_scrollback_calculate_display_width
    // to get the actual terminal column count.
    size_t nav_visual = ik_scrollback_calculate_display_width(nav_str, nav_len);
    size_t debug_visual = ik_scrollback_calculate_display_width(debug_str, debug_len);
    size_t info_visual = nav_visual + debug_visual;

    // Calculate how many separator chars to draw (leave room for visual width)
    size_t sep_chars = width;
    // LCOV_EXCL_BR_START - Defensive: info string typically much shorter than width
    if (info_visual > 0 && info_visual < width) {
        // LCOV_EXCL_BR_STOP
        sep_chars = width - info_visual;
    }

    // Render separator chars
    for (size_t i = 0; i < sep_chars; i++) {
        ik_output_buffer_append(output, BOX_DRAWING_LIGHT_HORIZONTAL, BOX_DRAWING_LIGHT_HORIZONTAL_LEN);
    }

    // Append navigation context if present
    if (nav_len > 0) {
        ik_output_buffer_append(output, nav_str, nav_len);
    }

    // Append debug string if present
    if (debug_len > 0) {
        ik_output_buffer_append(output, debug_str, debug_len);
    }

    // Add \x1b[K\r\n at end of line (clear to end of line before newline)
    ik_output_buffer_append(output, "\x1b[K\r\n", 5);
}

// Create separator layer
ik_layer_t *ik_separator_layer_create(TALLOC_CTX *ctx,
                                      const char *name,
                                      bool *visible_ptr)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(name != NULL);         // LCOV_EXCL_BR_LINE
    assert(visible_ptr != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate separator data
    ik_separator_layer_data_t *data = talloc(ctx, ik_separator_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->visible_ptr = visible_ptr;
    // Initialize debug pointers to NULL (no debug info by default)
    data->debug.viewport_offset = NULL;
    data->debug.viewport_row = NULL;
    data->debug.viewport_height = NULL;
    data->debug.document_height = NULL;
    data->debug.render_elapsed_us = NULL;
    // Initialize navigation context to NULL (no nav context by default)
    data->nav_ctx.parent_uuid = NULL;
    data->nav_ctx.prev_sibling_uuid = NULL;
    data->nav_ctx.current_uuid = NULL;
    data->nav_ctx.next_sibling_uuid = NULL;
    data->nav_ctx.child_count = 0;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           separator_is_visible,
                           separator_get_height,
                           separator_render);
}

// Set debug info pointers on separator layer
void ik_separator_layer_set_debug(ik_layer_t *layer,
                                  size_t *viewport_offset,
                                  size_t *viewport_row,
                                  size_t *viewport_height,
                                  size_t *document_height,
                                  uint64_t *render_elapsed_us)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;
    data->debug.viewport_offset = viewport_offset;
    data->debug.viewport_row = viewport_row;
    data->debug.viewport_height = viewport_height;
    data->debug.document_height = document_height;
    data->debug.render_elapsed_us = render_elapsed_us;
}

// Set navigation context on separator layer
void ik_separator_layer_set_nav_context(ik_layer_t *layer,
                                        const char *parent_uuid,
                                        const char *prev_sibling_uuid,
                                        const char *current_uuid,
                                        const char *next_sibling_uuid,
                                        size_t child_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    ik_separator_layer_data_t *data = (ik_separator_layer_data_t *)layer->data;
    data->nav_ctx.parent_uuid = parent_uuid;
    data->nav_ctx.prev_sibling_uuid = prev_sibling_uuid;
    data->nav_ctx.current_uuid = current_uuid;
    data->nav_ctx.next_sibling_uuid = next_sibling_uuid;
    data->nav_ctx.child_count = child_count;
}
