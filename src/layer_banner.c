// Banner layer wrapper
#include "layer_wrappers.h"

#include "ansi.h"
#include "layer.h"
#include "panic.h"
#include "version.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>


#include "poison.h"
// Unicode box-drawing characters (UTF-8 encoding, 3 bytes each, 1 display column)
#define BOX_DOUBLE_HORIZONTAL "\xE2\x95\x90"        // ═ (U+2550)
#define BOX_ROUNDED_DOWN_RIGHT "\xE2\x95\xAD"       // ╭ (U+256D)
#define BOX_ROUNDED_DOWN_LEFT "\xE2\x95\xAE"        // ╮ (U+256E)
#define BOX_LIGHT_VERTICAL "\xE2\x94\x82"           // │ (U+2502)
#define BOX_ROUNDED_UP_RIGHT "\xE2\x95\xB0"         // ╰ (U+2570)
#define BOX_ROUNDED_UP_LEFT "\xE2\x95\xAF"          // ╯ (U+2571)
#define BOX_LIGHT_HORIZONTAL "\xE2\x94\x80"         // ─ (U+2500)
#define BLACK_CIRCLE "\xE2\x97\x8F"                 // ● (U+25CF)

#define UNICODE_CHAR_LEN 3
#define UNICODE_DISPLAY_WIDTH 1

// Banner layer data
typedef struct {
    bool *visible_ptr;  // Raw pointer to visibility flag
} ik_banner_layer_data_t;

// Banner layer callbacks
static bool banner_is_visible(const ik_layer_t *layer)
{
    assert(layer != NULL);           // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL);     // LCOV_EXCL_BR_LINE

    ik_banner_layer_data_t *data = (ik_banner_layer_data_t *)layer->data;
    return *data->visible_ptr;
}

static size_t banner_get_height(const ik_layer_t *layer, size_t width)
{
    (void)layer;
    (void)width;
    return 6; // Banner layer is always 6 rows
}

static void banner_render(const ik_layer_t *layer,
                         ik_output_buffer_t *output,
                         size_t width,
                         size_t start_row,
                         size_t row_count)
{
    assert(layer != NULL);       // LCOV_EXCL_BR_LINE
    assert(layer->data != NULL); // LCOV_EXCL_BR_LINE

    // Calculate visible row range
    size_t end_row = start_row + row_count;

    // Build color escape sequences
    char color_border[16];
    char color_eyes[16];
    char color_pupils[16];
    char color_smile[16];
    char color_version[16];
    char color_tagline[16];

    size_t len_border = ik_ansi_fg_256(color_border, sizeof(color_border), 245);    // Dim gray
    size_t len_eyes = ik_ansi_fg_256(color_eyes, sizeof(color_eyes), 81);           // Cyan
    size_t len_pupils = ik_ansi_fg_256(color_pupils, sizeof(color_pupils), 214);    // Amber/gold
    size_t len_smile = ik_ansi_fg_256(color_smile, sizeof(color_smile), 211);       // Coral/pink
    size_t len_version = ik_ansi_fg_256(color_version, sizeof(color_version), 153); // Soft blue
    size_t len_tagline = ik_ansi_fg_256(color_tagline, sizeof(color_tagline), 250); // Light gray

    // Row 0: Top border (═ repeated for terminal width)
    if (0 >= start_row && 0 < end_row) {
        ik_output_buffer_append(output, color_border, len_border);
        for (size_t i = 0; i < width; i++) {
            ik_output_buffer_append(output, BOX_DOUBLE_HORIZONTAL, UNICODE_CHAR_LEN);
        }
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }

    // Row 1:  ╭─╮╭─╮
    if (1 >= start_row && 1 < end_row) {
        ik_output_buffer_append(output, " ", 1);
        ik_output_buffer_append(output, color_eyes, len_eyes);
        ik_output_buffer_append(output, BOX_ROUNDED_DOWN_RIGHT, UNICODE_CHAR_LEN);  // ╭
        ik_output_buffer_append(output, BOX_LIGHT_HORIZONTAL, UNICODE_CHAR_LEN);    // ─
        ik_output_buffer_append(output, BOX_ROUNDED_DOWN_LEFT, UNICODE_CHAR_LEN);   // ╮
        ik_output_buffer_append(output, BOX_ROUNDED_DOWN_RIGHT, UNICODE_CHAR_LEN);  // ╭
        ik_output_buffer_append(output, BOX_LIGHT_HORIZONTAL, UNICODE_CHAR_LEN);    // ─
        ik_output_buffer_append(output, BOX_ROUNDED_DOWN_LEFT, UNICODE_CHAR_LEN);   // ╮
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }

    // Row 2: (│●││●│)    Ikigai vX.X.X
    if (2 >= start_row && 2 < end_row) {
        ik_output_buffer_append(output, color_smile, len_smile);
        ik_output_buffer_append(output, "(", 1);                                    // ( ear
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, color_eyes, len_eyes);
        ik_output_buffer_append(output, BOX_LIGHT_VERTICAL, UNICODE_CHAR_LEN);      // │
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, color_pupils, len_pupils);
        ik_output_buffer_append(output, BLACK_CIRCLE, UNICODE_CHAR_LEN);            // ●
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, color_eyes, len_eyes);
        ik_output_buffer_append(output, BOX_LIGHT_VERTICAL, UNICODE_CHAR_LEN);      // │
        ik_output_buffer_append(output, BOX_LIGHT_VERTICAL, UNICODE_CHAR_LEN);      // │
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, color_pupils, len_pupils);
        ik_output_buffer_append(output, BLACK_CIRCLE, UNICODE_CHAR_LEN);            // ●
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, color_eyes, len_eyes);
        ik_output_buffer_append(output, BOX_LIGHT_VERTICAL, UNICODE_CHAR_LEN);      // │
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, color_smile, len_smile);
        ik_output_buffer_append(output, ")", 1);                                    // ) ear
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "    ", 4);
        ik_output_buffer_append(output, color_version, len_version);
        ik_output_buffer_append(output, "Ikigai v", 8);
        ik_output_buffer_append(output, IK_VERSION, strlen(IK_VERSION));
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }

    // Row 3:  ╰─╯╰─╯    Agentic Orchestration
    if (3 >= start_row && 3 < end_row) {
        ik_output_buffer_append(output, " ", 1);
        ik_output_buffer_append(output, color_eyes, len_eyes);
        ik_output_buffer_append(output, BOX_ROUNDED_UP_RIGHT, UNICODE_CHAR_LEN);    // ╰
        ik_output_buffer_append(output, BOX_LIGHT_HORIZONTAL, UNICODE_CHAR_LEN);    // ─
        ik_output_buffer_append(output, BOX_ROUNDED_UP_LEFT, UNICODE_CHAR_LEN);     // ╯
        ik_output_buffer_append(output, BOX_ROUNDED_UP_RIGHT, UNICODE_CHAR_LEN);    // ╰
        ik_output_buffer_append(output, BOX_LIGHT_HORIZONTAL, UNICODE_CHAR_LEN);    // ─
        ik_output_buffer_append(output, BOX_ROUNDED_UP_LEFT, UNICODE_CHAR_LEN);     // ╯
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "     ", 5);
        ik_output_buffer_append(output, color_tagline, len_tagline);
        ik_output_buffer_append(output, "Agentic Orchestration", 21);
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }

    // Row 4:   ╰──╯
    if (4 >= start_row && 4 < end_row) {
        ik_output_buffer_append(output, "  ", 2);
        ik_output_buffer_append(output, color_smile, len_smile);
        ik_output_buffer_append(output, BOX_ROUNDED_UP_RIGHT, UNICODE_CHAR_LEN);    // ╰
        ik_output_buffer_append(output, BOX_LIGHT_HORIZONTAL, UNICODE_CHAR_LEN);    // ─
        ik_output_buffer_append(output, BOX_LIGHT_HORIZONTAL, UNICODE_CHAR_LEN);    // ─
        ik_output_buffer_append(output, BOX_ROUNDED_UP_LEFT, UNICODE_CHAR_LEN);     // ╯
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }

    // Row 5: Bottom border (═ repeated for terminal width)
    if (5 >= start_row && 5 < end_row) {
        ik_output_buffer_append(output, color_border, len_border);
        for (size_t i = 0; i < width; i++) {
            ik_output_buffer_append(output, BOX_DOUBLE_HORIZONTAL, UNICODE_CHAR_LEN);
        }
        ik_output_buffer_append(output, IK_ANSI_RESET, 4);
        ik_output_buffer_append(output, "\x1b[K\r\n", 5);
    }
}

// Create banner layer
ik_layer_t *ik_banner_layer_create(TALLOC_CTX *ctx,
                                   const char *name,
                                   bool *visible_ptr)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(name != NULL);         // LCOV_EXCL_BR_LINE
    assert(visible_ptr != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate banner data
    ik_banner_layer_data_t *data = talloc(ctx, ik_banner_layer_data_t);
    if (data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    data->visible_ptr = visible_ptr;

    // Create layer
    return ik_layer_create(ctx, name, data,
                           banner_is_visible,
                           banner_get_height,
                           banner_render);
}
