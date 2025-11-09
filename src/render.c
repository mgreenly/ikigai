/**
 * @file render.c
 * @brief Rendering module implementation using libvterm
 */

#include "render.h"
#include "wrapper.h"
#include <assert.h>
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

/**
 * @brief Encode a single Unicode codepoint to UTF-8
 *
 * @param cp Unicode codepoint
 * @param utf8 Output buffer for UTF-8 bytes
 * @return Number of bytes written
 */
static int encode_codepoint_to_utf8(uint32_t cp, char *utf8)
{
    if (cp < 0x80) {
        utf8[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        utf8[0] = (char)(0xC0 | (cp >> 6));
        utf8[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        utf8[0] = (char)(0xE0 | (cp >> 12));
        utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        utf8[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        utf8[0] = (char)(0xF0 | (cp >> 18));
        utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        utf8[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

/**
 * @brief Write a VTerm cell to terminal file descriptor
 *
 * @param cell VTerm cell to write
 * @param tty_fd Terminal file descriptor
 * @param parent Talloc parent for error messages
 * @return RES_OK on success, RES_ERR on failure
 */
static res_t write_cell(const VTermScreenCell *cell, int32_t tty_fd, void *parent)
{
    if (cell->chars[0] == 0) {
        return OK(parent);
    }

    char utf8[7];
    int len = 0;

    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell->chars[i] != 0; i++) {
        len += encode_codepoint_to_utf8(cell->chars[i], utf8 + len);
    }

    if (ik_write_wrapper(tty_fd, utf8, (size_t)len) < 0) { /* LCOV_EXCL_BR_LINE */
        return ERR(parent, IO, "Failed to write cell"); /* LCOV_EXCL_LINE */
    }

    return OK(parent);
}

/**
 * @brief Write all cells from VTerm screen to terminal
 *
 * @param render Render context
 * @param vscreen VTerm screen
 * @param tty_fd Terminal file descriptor
 * @return RES_OK on success, RES_ERR on failure
 */
static res_t write_screen_cells(ik_render_ctx_t *render, VTermScreen *vscreen, int32_t tty_fd)
{
    for (int32_t row = 0; row < render->rows; row++) {
        for (int32_t col = 0; col < render->cols; col++) {
            VTermPos pos = {.row = row, .col = col};
            VTermScreenCell cell;
            vterm_screen_get_cell(vscreen, pos, &cell);

            res_t result = write_cell(&cell, tty_fd, render);
            if (is_err(&result)) {
                return result;
            }
        }
    }
    return OK(render);
}

res_t ik_render_blit(ik_render_ctx_t *render, int32_t tty_fd)
{
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vscreen != NULL); /* LCOV_EXCL_BR_LINE */
    assert(tty_fd >= 0); /* LCOV_EXCL_BR_LINE */

    VTerm *vt = (VTerm *)render->vterm;
    VTermScreen *vscreen = (VTermScreen *)render->vscreen;

    // Clear screen and move cursor to home position
    const char *clear_seq = "\x1b[H\x1b[2J";
    if (ik_write_wrapper(tty_fd, clear_seq, 7) < 0) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to clear screen"); /* LCOV_EXCL_LINE */
    }

    // Write all cells to terminal
    res_t result = write_screen_cells(render, vscreen, tty_fd);
    if (is_err(&result)) {
        return result;
    }

    // Get cursor position from vterm and position terminal cursor
    VTermState *state = vterm_obtain_state(vt);
    VTermPos cursor_pos;
    vterm_state_get_cursorpos(state, &cursor_pos);

    char cursor_seq[32];
    int len = snprintf(cursor_seq, sizeof(cursor_seq), "\x1b[%d;%dH",
                       cursor_pos.row + 1, cursor_pos.col + 1);
    if (len < 0 || (size_t)len >= sizeof(cursor_seq)) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to format cursor position"); /* LCOV_EXCL_LINE */
    }
    if (ik_write_wrapper(tty_fd, cursor_seq, (size_t)len) < 0) { /* LCOV_EXCL_BR_LINE */
        return ERR(render, IO, "Failed to set cursor position"); /* LCOV_EXCL_LINE */
    }

    return OK(render);
}
