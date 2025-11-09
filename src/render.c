/**
 * @file render.c
 * @brief Rendering module implementation using libvterm
 */

#include "render.h"
#include "wrapper.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>
#include <vterm.h>

/**
 * @brief Destructor for render context
 *
 * Frees the VTerm instance when the render context is destroyed.
 *
 * @param ptr Pointer to the render context
 * @return 0 on success
 */
static int ik_render_destructor(ik_render_ctx_t *render)
{
    if (render->vterm != NULL) { /* LCOV_EXCL_BR_LINE */
        vterm_free((VTerm *)render->vterm);
        render->vterm = NULL;
        render->vscreen = NULL;
    }
    return 0;
}

/**
 * @brief Encode a Unicode codepoint to UTF-8
 *
 * @param codepoint Unicode codepoint to encode
 * @param buf Output buffer (must be at least 4 bytes)
 * @return Number of bytes written, or -1 on error
 */
static int32_t encode_utf8(uint32_t codepoint, char *buf)
{
    if (codepoint <= 0x7F) {
        // 1-byte sequence
        buf[0] = (char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        // 2-byte sequence
        buf[0] = (char)(0xC0 | (codepoint >> 6));
        buf[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint <= 0xFFFF) {
        // 3-byte sequence
        buf[0] = (char)(0xE0 | (codepoint >> 12));
        buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        // 4-byte sequence
        buf[0] = (char)(0xF0 | (codepoint >> 18));
        buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }
    return -1; // Invalid codepoint
}

res_t ik_render_create(void *parent, int32_t rows, int32_t cols, ik_render_ctx_t **render_out)
{
    assert(render_out != NULL); /* LCOV_EXCL_BR_LINE */
    assert(rows > 0); /* LCOV_EXCL_BR_LINE */
    assert(cols > 0); /* LCOV_EXCL_BR_LINE */

    /* Allocate render context */
    ik_render_ctx_t *render = ik_talloc_zero_wrapper(parent, sizeof(ik_render_ctx_t));
    if (render == NULL) {
        return ERR(parent, OOM, "Failed to allocate render context");
    }

    /* Create VTerm */
    VTerm *vt = vterm_new(rows, cols);
    if (vt == NULL) { /* LCOV_EXCL_BR_LINE */
        talloc_free(render); /* LCOV_EXCL_LINE */
        return ERR(parent, OOM, "Failed to create VTerm"); /* LCOV_EXCL_LINE */
    }

    /* Enable UTF-8 mode */
    vterm_set_utf8(vt, 1);

    /* Get VTermScreen */
    VTermScreen *vscreen = vterm_obtain_screen(vt);
    if (vscreen == NULL) { /* LCOV_EXCL_BR_LINE */
        vterm_free(vt); /* LCOV_EXCL_LINE */
        talloc_free(render); /* LCOV_EXCL_LINE */
        return ERR(parent, IO, "Failed to obtain VTermScreen"); /* LCOV_EXCL_LINE */
    }

    /* Initialize the screen */
    vterm_screen_reset(vscreen, 1);

    /* Initialize render context */
    render->vterm = (void *)vt;
    render->vscreen = (void *)vscreen;
    render->rows = rows;
    render->cols = cols;

    /* Set up destructor to free vterm */
    talloc_set_destructor(render, ik_render_destructor);

    *render_out = render;
    return OK(render);
}

void ik_render_clear(ik_render_ctx_t *render)
{
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vscreen != NULL); /* LCOV_EXCL_BR_LINE */

    VTermScreen *vscreen = (VTermScreen *)render->vscreen;
    vterm_screen_reset(vscreen, 1);
}

res_t ik_render_write_text(ik_render_ctx_t *render, const char *text, size_t len)
{
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vterm != NULL); /* LCOV_EXCL_BR_LINE */
    assert(text != NULL || len == 0); /* LCOV_EXCL_BR_LINE */

    if (len == 0) {
        return OK(render);
    }

    VTerm *vt = (VTerm *)render->vterm;
    size_t written = vterm_input_write(vt, text, len);
    if (written != len) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to write text to vterm"); /* LCOV_EXCL_LINE */
    }

    return OK(render);
}

res_t ik_render_set_cursor(ik_render_ctx_t *render, int32_t row, int32_t col)
{
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vterm != NULL); /* LCOV_EXCL_BR_LINE */
    assert(row >= 0 && row < render->rows); /* LCOV_EXCL_BR_LINE */
    assert(col >= 0 && col < render->cols); /* LCOV_EXCL_BR_LINE */

    /* Use ANSI escape sequence to set cursor position */
    char escape_seq[32];
    int len = snprintf(escape_seq, sizeof(escape_seq), "\x1b[%d;%dH", row + 1, col + 1);
    if (len < 0 || (size_t)len >= sizeof(escape_seq)) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to format cursor position"); /* LCOV_EXCL_LINE */
    }

    VTerm *vt = (VTerm *)render->vterm;
    vterm_input_write(vt, escape_seq, (size_t)len);
    return OK(render);
}

res_t ik_render_blit(ik_render_ctx_t *render, int32_t tty_fd)
{
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vscreen != NULL); /* LCOV_EXCL_BR_LINE */
    assert(tty_fd >= 0); /* LCOV_EXCL_BR_LINE */

    VTermScreen *vscreen = (VTermScreen *)render->vscreen;
    VTerm *vt = (VTerm *)render->vterm;

    // Clear screen and move to home position
    const char *clear_and_home = "\x1b[2J\x1b[H";
    ssize_t n = ik_write_wrapper(tty_fd, clear_and_home, strlen(clear_and_home));
    if (n < 0 || (size_t)n != strlen(clear_and_home)) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to clear screen"); /* LCOV_EXCL_LINE */
    }

    // Read and write all cells from VTermScreen
    VTermPos pos;
    VTermScreenCell cell;
    char utf8_buf[16];

    for (pos.row = 0; pos.row < render->rows; pos.row++) {
        for (pos.col = 0; pos.col < render->cols; pos.col++) {
            vterm_screen_get_cell(vscreen, pos, &cell);

            // Skip empty cells
            if (cell.chars[0] == 0) {
                continue;
            }

            // Convert codepoints to UTF-8 and write to terminal
            // Cell can contain multiple codepoints (for combining characters)
            for (int32_t i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i] != 0; i++) { /* LCOV_EXCL_BR_LINE */
                int32_t len = encode_utf8(cell.chars[i], utf8_buf);
                if (len > 0) { /* LCOV_EXCL_BR_LINE */
                    n = ik_write_wrapper(tty_fd, utf8_buf, (size_t)len);
                    if (n < 0 || n != len) { /* LCOV_EXCL_BR_LINE */
                        return ERR(render, IO, "Failed to write cell to terminal"); /* LCOV_EXCL_LINE */
                    }
                }
            }
        }
    }

    // Get cursor position from vterm and set it on the terminal
    VTermState *state = vterm_obtain_state(vt);
    VTermPos cursor_pos;
    vterm_state_get_cursorpos(state, &cursor_pos);

    // Position cursor using ANSI escape sequence (convert 0-based to 1-based)
    char cursor_seq[32];
    int32_t len = snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%d;%dH",
                           cursor_pos.row + 1, cursor_pos.col + 1);
    if (len < 0 || (size_t)len >= sizeof(cursor_seq)) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to format cursor position"); /* LCOV_EXCL_LINE */
    }

    n = ik_write_wrapper(tty_fd, cursor_seq, (size_t)len);
    if (n < 0 || (size_t)n != (size_t)len) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to set cursor position"); /* LCOV_EXCL_LINE */
    }

    return OK(render);
}
