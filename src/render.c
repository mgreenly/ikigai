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
static int ik_render_destructor(ik_render_ctx_t *render) {
    if (render->vterm != NULL) { /* LCOV_EXCL_BR_LINE */
        vterm_free((VTerm *)render->vterm);
        render->vterm = NULL;
        render->vscreen = NULL;
    }
    return 0;
}

res_t ik_render_create(void *parent, int32_t rows, int32_t cols, ik_render_ctx_t **render_out) {
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

void ik_render_clear(ik_render_ctx_t *render) {
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vscreen != NULL); /* LCOV_EXCL_BR_LINE */

    VTermScreen *vscreen = (VTermScreen *)render->vscreen;
    vterm_screen_reset(vscreen, 1);
}

res_t ik_render_write_text(ik_render_ctx_t *render, const char *text, size_t len) {
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

res_t ik_render_set_cursor(ik_render_ctx_t *render, int32_t row, int32_t col) {
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

res_t ik_render_blit(ik_render_ctx_t *render, int32_t tty_fd) {
    assert(render != NULL); /* LCOV_EXCL_BR_LINE */
    assert(render->vscreen != NULL); /* LCOV_EXCL_BR_LINE */
    assert(tty_fd >= 0); /* LCOV_EXCL_BR_LINE */

    /* TODO: Implement blitting to terminal */
    (void)tty_fd; /* Suppress unused parameter warning for now */
    return OK(render);
}
