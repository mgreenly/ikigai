/**
 * @file commands_context_box.h
 * @brief Box-drawing primitives for the /context command
 */

#ifndef IK_COMMANDS_CONTEXT_BOX_H
#define IK_COMMANDS_CONTEXT_BOX_H

#include "shared/error.h"
#include <stdint.h>
#include <talloc.h>

/* Forward declaration */
typedef struct ik_scrollback_t ik_scrollback_t;

/* Box-drawing UTF-8 sequences (3 bytes / 1 display column each) */
#define CTX_BOX_H   "\xe2\x94\x80"  /* ─ U+2500 */
#define CTX_BOX_V   "\xe2\x94\x82"  /* │ U+2502 */
#define CTX_BOX_TL  "\xe2\x94\x8c"  /* ┌ U+250C */
#define CTX_BOX_TR  "\xe2\x94\x90"  /* ┐ U+2510 */
#define CTX_BOX_BL  "\xe2\x94\x94"  /* └ U+2514 */
#define CTX_BOX_BR  "\xe2\x94\x98"  /* ┘ U+2518 */
#define CTX_ELLIPSIS "\xe2\x80\xa6" /* … U+2026 */

#define CTX_MIN_WIDTH 60

/**
 * Render context: carries ctx, scrollback, width, and first error.
 * All render functions check err before proceeding, so callers need
 * not check each call individually.
 */
typedef struct {
    TALLOC_CTX    *ctx;
    ik_scrollback_t *sb;
    int            W;
    res_t          err;
} ctx_rend_t;

void ctx_rend_init(ctx_rend_t *r, TALLOC_CTX *ctx, ik_scrollback_t *sb, int W);

/* ── String helpers ── */
char *ctx_make_dashes(TALLOC_CTX *ctx, int n);
char *ctx_make_spaces(TALLOC_CTX *ctx, int n);
char *ctx_format_tok(TALLOC_CTX *ctx, int32_t n);
int   ctx_disp_width(const char *s);
char *ctx_trim_middle(TALLOC_CTX *ctx, const char *s, int max_cols);
char *ctx_make_tok_label(TALLOC_CTX *ctx, int32_t n);

/* ── Box line renderers ── */
void ctx_render_outer_title(ctx_rend_t *r);
void ctx_render_outer_blank(ctx_rend_t *r);
void ctx_render_outer_close(ctx_rend_t *r);
void ctx_render_group_header(ctx_rend_t *r, const char *name, const char *right);
void ctx_render_group_row(ctx_rend_t *r, const char *label, const char *right_str);
void ctx_render_group_footer(ctx_rend_t *r);
void ctx_render_total_line(ctx_rend_t *r, int32_t total);
void ctx_render_budget_line(ctx_rend_t *r, int32_t budget, int32_t total);

#endif /* IK_COMMANDS_CONTEXT_BOX_H */
