/**
 * @file commands_context_box.c
 * @brief Box-drawing primitives for the /context command
 */

#include "apps/ikigai/commands_context_box.h"
#include "apps/ikigai/scrollback.h"
#include "shared/panic.h"

#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

void ctx_rend_init(ctx_rend_t *r, TALLOC_CTX *ctx, ik_scrollback_t *sb, int W)
{
    r->ctx = ctx;
    r->sb  = sb;
    r->W   = W;
    r->err = OK(NULL);
}

/* Build string of n CTX_BOX_H chars (3 bytes each, 1 display col each) */
char *ctx_make_dashes(TALLOC_CTX *ctx, int n)
{
    if (n <= 0) {
        char *s = talloc_strdup(ctx, "");
        if (!s) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return s;
    }
    char *buf = talloc_size(ctx, (size_t)(n * 3 + 1));
    if (!buf) PANIC("OOM");  /* LCOV_EXCL_LINE */
    for (int i = 0; i < n; i++) {
        buf[i * 3 + 0] = (char)0xe2;
        buf[i * 3 + 1] = (char)0x94;
        buf[i * 3 + 2] = (char)0x80;
    }
    buf[n * 3] = '\0';
    return buf;
}

/* Build string of n spaces */
char *ctx_make_spaces(TALLOC_CTX *ctx, int n)
{
    if (n <= 0) {
        char *s = talloc_strdup(ctx, "");
        if (!s) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return s;
    }
    char *buf = talloc_size(ctx, (size_t)(n + 1));
    if (!buf) PANIC("OOM");  /* LCOV_EXCL_LINE */
    memset(buf, ' ', (size_t)n);
    buf[n] = '\0';
    return buf;
}

/* Format integer with thousands commas */
char *ctx_format_tok(TALLOC_CTX *ctx, int32_t n)
{
    char buf[32];
    if (n < 0) n = 0;
    if (n >= 1000000) {
        snprintf(buf, sizeof(buf), "%d,%03d,%03d",
                 n / 1000000, (n / 1000) % 1000, n % 1000);
    } else if (n >= 1000) {
        snprintf(buf, sizeof(buf), "%d,%03d", n / 1000, n % 1000);
    } else {
        snprintf(buf, sizeof(buf), "%d", n);
    }
    char *s = talloc_strdup(ctx, buf);
    if (!s) PANIC("OOM");  /* LCOV_EXCL_LINE */
    return s;
}

/*
 * Display width of string with ASCII and multi-byte UTF-8 sequences.
 * Each complete UTF-8 code point counts as 1 display column.
 */
int ctx_disp_width(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int w = 0;
    while (*p) {
        if (*p >= 0xf0)      p += 4;
        else if (*p >= 0xe0) p += 3;
        else if (*p >= 0xc0) p += 2;
        else                 p += 1;
        w++;
    }
    return w;
}

/* Middle-trim string to at most max_cols display columns */
char *ctx_trim_middle(TALLOC_CTX *ctx, const char *s, int max_cols)
{
    if (ctx_disp_width(s) <= max_cols) {
        char *r = talloc_strdup(ctx, s);
        if (!r) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return r;
    }
    int avail = max_cols - 1;
    if (avail <= 0) {
        char *r = talloc_strdup(ctx, CTX_ELLIPSIS);
        if (!r) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return r;
    }
    int right = avail / 2;
    int left  = avail - right;
    int len   = (int)strlen(s);
    char *r = talloc_asprintf(ctx, "%.*s" CTX_ELLIPSIS "%.*s",
                              left, s, right, s + len - right);
    if (!r) PANIC("OOM");  /* LCOV_EXCL_LINE */
    return r;
}

/* Build "{n} tok" label */
char *ctx_make_tok_label(TALLOC_CTX *ctx, int32_t n)
{
    char *s = ctx_format_tok(ctx, n);
    char *label = talloc_asprintf(ctx, "%s tok", s);
    if (!label) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(s);
    return label;
}

/* Append a line to scrollback; store first error in r->err */
static void ctx_emit(ctx_rend_t *r, const char *line)
{
    if (is_err(&r->err)) return;
    r->err = ik_scrollback_append_line(r->sb, line, strlen(line));
}

/* Outer title: ┌─ Context ──...──┐  (W cols) */
void ctx_render_outer_title(ctx_rend_t *r)
{
    char *d = ctx_make_dashes(r->ctx, r->W - 12);
    char *line = talloc_asprintf(r->ctx,
        CTX_BOX_TL CTX_BOX_H " Context %s" CTX_BOX_TR, d);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(d);
    ctx_emit(r, line);
    talloc_free(line);
}

/* Outer blank: │<W-2 spaces>│ */
void ctx_render_outer_blank(ctx_rend_t *r)
{
    char *sp = ctx_make_spaces(r->ctx, r->W - 2);
    char *line = talloc_asprintf(r->ctx, CTX_BOX_V "%s" CTX_BOX_V, sp);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(sp);
    ctx_emit(r, line);
    talloc_free(line);
}

/* Outer close: └──...──┘  (W cols) */
void ctx_render_outer_close(ctx_rend_t *r)
{
    char *d = ctx_make_dashes(r->ctx, r->W - 2);
    char *line = talloc_asprintf(r->ctx, CTX_BOX_BL "%s" CTX_BOX_BR, d);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(d);
    ctx_emit(r, line);
    talloc_free(line);
}

/*
 * Group header: │ ┌─ {name} ──...── {right} ─┐│  (W cols)
 *   inner = W-3 = 3 + name_len + 1 + dashes + 1 + right_len + 3
 *   dashes = W - 11 - name_len - right_len
 */
void ctx_render_group_header(ctx_rend_t *r, const char *name, const char *right)
{
    int n = r->W - 11 - ctx_disp_width(name) - ctx_disp_width(right);
    if (n < 1) n = 1;
    char *d = ctx_make_dashes(r->ctx, n);
    char *line = talloc_asprintf(r->ctx,
        CTX_BOX_V " " CTX_BOX_TL CTX_BOX_H " %s %s %s " CTX_BOX_H CTX_BOX_TR CTX_BOX_V,
        name, d, right);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(d);
    ctx_emit(r, line);
    talloc_free(line);
}

/*
 * Group row: │ │ {label} {spaces} {right_str}││  (W cols)
 *   content area = W-6 cols; right_str may be NULL
 */
void ctx_render_group_row(ctx_rend_t *r, const char *label, const char *right_str)
{
    int cw   = r->W - 6;
    if (cw < 0) cw = 0;
    int rlen = right_str ? ctx_disp_width(right_str) : 0;
    int lmax = (rlen > 0) ? cw - rlen - 1 : cw;
    if (lmax < 0) lmax = 0;

    char *tl  = ctx_trim_middle(r->ctx, label, lmax);
    int   pad = cw - ctx_disp_width(tl) - rlen;
    if (pad < 0) pad = 0;
    char *sp  = ctx_make_spaces(r->ctx, pad);
    char *line;
    if (rlen > 0) {
        line = talloc_asprintf(r->ctx,
            CTX_BOX_V " " CTX_BOX_V " %s%s%s" CTX_BOX_V CTX_BOX_V,
            tl, sp, right_str);
    } else {
        line = talloc_asprintf(r->ctx,
            CTX_BOX_V " " CTX_BOX_V " %s%s" CTX_BOX_V CTX_BOX_V,
            tl, sp);
    }
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(tl);
    talloc_free(sp);
    ctx_emit(r, line);
    talloc_free(line);
}

/* Group footer: │ └──...──┘│  (W cols) */
void ctx_render_group_footer(ctx_rend_t *r)
{
    int n = r->W - 5;
    if (n < 0) n = 0;
    char *d = ctx_make_dashes(r->ctx, n);
    char *line = talloc_asprintf(r->ctx,
        CTX_BOX_V " " CTX_BOX_BL "%s" CTX_BOX_BR CTX_BOX_V, d);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(d);
    ctx_emit(r, line);
    talloc_free(line);
}

/*
 * Total separator: │ ── Total ──...── {tok} tok ──── │  (W cols)
 *   prefix: 11 cols; suffix: tok_len + 12 cols; dashes = W - 23 - tok_len
 */
void ctx_render_total_line(ctx_rend_t *r, int32_t total)
{
    char *ts  = ctx_format_tok(r->ctx, total);
    int   n   = r->W - 23 - (int)strlen(ts);
    if (n < 1) n = 1;
    char *d   = ctx_make_dashes(r->ctx, n);
    char *line = talloc_asprintf(r->ctx,
        CTX_BOX_V " " CTX_BOX_H CTX_BOX_H " Total %s %s tok "
        CTX_BOX_H CTX_BOX_H CTX_BOX_H CTX_BOX_H " " CTX_BOX_V,
        d, ts);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(d);
    talloc_free(ts);
    ctx_emit(r, line);
    talloc_free(line);
}

/* Budget line: │ Budget: {budget} tok    Used: {pct}%    Remaining: {rem}  │ */
void ctx_render_budget_line(ctx_rend_t *r, int32_t budget, int32_t total)
{
    char *bs  = ctx_format_tok(r->ctx, budget);
    int32_t rem = (budget > total) ? budget - total : 0;
    char *rs  = ctx_format_tok(r->ctx, rem);
    double pct = (budget > 0) ? (double)total / (double)budget * 100.0 : 0.0;
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%.1f%%", pct);
    char *content = talloc_asprintf(r->ctx,
        " Budget: %s tok    Used: %s    Remaining: %s", bs, pbuf, rs);
    if (!content) PANIC("OOM");  /* LCOV_EXCL_LINE */
    int clen = ctx_disp_width(content);
    if (clen > r->W - 2) {
        char *trimmed = ctx_trim_middle(r->ctx, content, r->W - 2);
        talloc_free(content);
        content = trimmed;
        clen = r->W - 2;
    }
    int pad  = r->W - 2 - clen;
    if (pad < 0) pad = 0;
    char *sp   = ctx_make_spaces(r->ctx, pad);
    char *line = talloc_asprintf(r->ctx, CTX_BOX_V "%s%s" CTX_BOX_V, content, sp);
    if (!line) PANIC("OOM");  /* LCOV_EXCL_LINE */
    talloc_free(bs);
    talloc_free(rs);
    talloc_free(content);
    talloc_free(sp);
    ctx_emit(r, line);
    talloc_free(line);
}
