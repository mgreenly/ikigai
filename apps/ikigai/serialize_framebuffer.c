#include "serialize.h"

#include "shared/error.h"
#include "shared/panic.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

typedef struct {
    int32_t fg;
    bool has_fg;
    bool bold;
    bool dim;
    bool reverse;
} style_t;

typedef struct {
    char *text;
    uint32_t text_len;
    uint32_t text_cap;
    style_t style;
} span_t;

typedef struct {
    int32_t row;
    span_t *spans;
    uint32_t span_count;
    uint32_t span_cap;
} line_t;

typedef struct {
    line_t *lines;
    int32_t current_row;
    style_t current_style;
    span_t current_span;
    TALLOC_CTX *tmp;
} parse_state_t;

void ik_serialize_flush_span(parse_state_t *state);
void ik_serialize_add_char(parse_state_t *state, char c);
void ik_serialize_handle_fg_color(parse_state_t *state, const uint8_t *fb, size_t *i, size_t len);
void ik_serialize_handle_reset(parse_state_t *state, size_t *i);
void ik_serialize_handle_bold(parse_state_t *state, size_t *i);
void ik_serialize_handle_dim(parse_state_t *state, size_t *i);
void ik_serialize_handle_reverse(parse_state_t *state, size_t *i);
char *ik_serialize_escape_text(TALLOC_CTX *ctx, const char *text, uint32_t len);
char *ik_serialize_build_style_json(char *json, const style_t *style);
char *ik_serialize_build_span_json(char *json, span_t *span, TALLOC_CTX *tmp);
char *ik_serialize_build_line_json(char *json, line_t *line, TALLOC_CTX *tmp);
void ik_serialize_handle_escape_sequence(parse_state_t *state, const uint8_t *fb, size_t *i, size_t len);
void ik_serialize_parse_framebuffer(parse_state_t *state, const uint8_t *fb, size_t len, int32_t rows);
void ik_serialize_ensure_empty_rows(line_t *lines, int32_t rows);
char *ik_serialize_build_json(TALLOC_CTX *ctx, line_t *lines, int32_t rows, int32_t cols,
                               int32_t cursor_row, int32_t cursor_col, bool cursor_visible,
                               TALLOC_CTX *tmp);

void ik_serialize_flush_span(parse_state_t *state)
{
    if (state->current_span.text_len == 0) {
        return;
    }

    line_t *line = &state->lines[state->current_row];
    if (line->span_count >= line->span_cap) {
        line->span_cap *= 2;
        line->spans = talloc_realloc(state->lines, line->spans, span_t, line->span_cap);
        if (line->spans == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    line->spans[line->span_count] = state->current_span;
    line->span_count++;

    state->current_span.text_len = 0;
    state->current_span.text_cap = 256;
    state->current_span.text = talloc_zero_array(state->tmp, char, 256);
    if (state->current_span.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
}

void ik_serialize_add_char(parse_state_t *state, char c)
{
    if (state->current_span.text_len >= state->current_span.text_cap) {
        state->current_span.text_cap *= 2;
        state->current_span.text = talloc_realloc(state->tmp, state->current_span.text, char, state->current_span.text_cap);
        if (state->current_span.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    state->current_span.text[state->current_span.text_len++] = c;
}

void ik_serialize_handle_fg_color(parse_state_t *state, const uint8_t *fb, size_t *i, size_t len)
{
    if (*i + 4 >= len) return;
    if (fb[*i] != '3' || fb[*i + 1] != '8' || fb[*i + 2] != ';' || fb[*i + 3] != '5' || fb[*i + 4] != ';') {
        return;
    }

    *i += 5;
    int32_t color = 0;
    while (*i < len && isdigit(fb[*i])) {
        color = color * 10 + (fb[*i] - '0');
        (*i)++;
    }
    if (*i < len && fb[*i] == 'm') {
        (*i)++;
        ik_serialize_flush_span(state);
        state->current_style.fg = color;
        state->current_style.has_fg = true;
        state->current_span.style = state->current_style;
    }
}

void ik_serialize_handle_reset(parse_state_t *state, size_t *i)
{
    *i += 2;
    ik_serialize_flush_span(state);
    state->current_style = (style_t){0};
    state->current_span.style = state->current_style;
}

void ik_serialize_handle_bold(parse_state_t *state, size_t *i)
{
    *i += 2;
    ik_serialize_flush_span(state);
    state->current_style.bold = true;
    state->current_span.style = state->current_style;
}

void ik_serialize_handle_dim(parse_state_t *state, size_t *i)
{
    *i += 2;
    ik_serialize_flush_span(state);
    state->current_style.dim = true;
    state->current_span.style = state->current_style;
}

void ik_serialize_handle_reverse(parse_state_t *state, size_t *i)
{
    *i += 2;
    ik_serialize_flush_span(state);
    state->current_style.reverse = true;
    state->current_span.style = state->current_style;
}

char *ik_serialize_escape_text(TALLOC_CTX *ctx, const char *text, uint32_t len)
{
    char *escaped = talloc_array(ctx, char, len * 2 + 1);
    if (escaped == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    size_t escaped_len = 0;
    for (size_t j = 0; j < len; j++) {
        char c = text[j];
        if (c == '"' || c == '\\') {
            escaped[escaped_len++] = '\\';
        }
        escaped[escaped_len++] = c;
    }
    escaped[escaped_len] = '\0';
    return escaped;
}

char *ik_serialize_build_style_json(char *json, const style_t *style)
{
    bool first_attr = true;
    if (style->has_fg) {
        json = talloc_asprintf_append(json, "\"fg\":%" PRId32, style->fg);
        if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        first_attr = false;
    }
    if (style->bold) {
        if (!first_attr) {
            json = talloc_asprintf_append(json, ",");
            if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        json = talloc_asprintf_append(json, "\"bold\":true");
        if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        first_attr = false;
    }
    if (style->dim) {
        if (!first_attr) {
            json = talloc_asprintf_append(json, ",");
            if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        json = talloc_asprintf_append(json, "\"dim\":true");
        if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        first_attr = false;
    }
    if (style->reverse) {
        if (!first_attr) {
            json = talloc_asprintf_append(json, ",");
            if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        json = talloc_asprintf_append(json, "\"reverse\":true");
        if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    return json;
}

char *ik_serialize_build_span_json(char *json, span_t *span, TALLOC_CTX *tmp)
{
    char *escaped = ik_serialize_escape_text(tmp, span->text, span->text_len);
    json = talloc_asprintf_append(json, "{\"text\":\"%s\",\"style\":{", escaped);
    if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    json = ik_serialize_build_style_json(json, &span->style);
    json = talloc_asprintf_append(json, "}}");
    if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return json;
}

char *ik_serialize_build_line_json(char *json, line_t *line, TALLOC_CTX *tmp)
{
    json = talloc_asprintf_append(json, "{\"row\":%" PRId32 ",\"spans\":[", line->row);
    if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t s = 0; s < line->span_count; s++) {
        if (s > 0) {
            json = talloc_asprintf_append(json, ",");
            if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        json = ik_serialize_build_span_json(json, &line->spans[s], tmp);
    }

    json = talloc_asprintf_append(json, "]}");
    if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return json;
}

void ik_serialize_handle_escape_sequence(parse_state_t *state, const uint8_t *fb, size_t *i, size_t len)
{
    *i += 2;
    ik_serialize_handle_fg_color(state, fb, i, len);
    if (*i >= len || fb[*i - 1] == 'm') return;

    if (*i < len && fb[*i] == '0' && *i + 1 < len && fb[*i + 1] == 'm') {
        ik_serialize_handle_reset(state, i);
        return;
    }
    if (*i < len && fb[*i] == '1' && *i + 1 < len && fb[*i + 1] == 'm') {
        ik_serialize_handle_bold(state, i);
        return;
    }
    if (*i < len && fb[*i] == '2' && *i + 1 < len && fb[*i + 1] == 'm') {
        ik_serialize_handle_dim(state, i);
        return;
    }
    if (*i < len && fb[*i] == '7' && *i + 1 < len && fb[*i + 1] == 'm') {
        ik_serialize_handle_reverse(state, i);
        return;
    }

    while (*i < len && (fb[*i] < 0x40 || fb[*i] > 0x7E)) {
        (*i)++;
    }
    if (*i < len) (*i)++;
}

void ik_serialize_parse_framebuffer(parse_state_t *state, const uint8_t *fb, size_t len, int32_t rows)
{
    size_t i = 0;
    while (i < len) {
        if (i + 6 <= len && memcmp(&fb[i], "\x1b[?25l", 6) == 0) {
            i += 6;
            continue;
        }
        if (i + 3 <= len && memcmp(&fb[i], "\x1b[H", 3) == 0) {
            i += 3;
            continue;
        }

        if (fb[i] == '\x1b' && i + 1 < len && fb[i + 1] == '[') {
            ik_serialize_handle_escape_sequence(state, fb, &i, len);
            continue;
        }

        if (fb[i] == '\r' && i + 1 < len && fb[i + 1] == '\n') {
            if (state->current_span.text_len > 0 || state->lines[state->current_row].span_count == 0) {
                ik_serialize_flush_span(state);
            }
            state->current_row++;
            i += 2;
            continue;
        }

        ik_serialize_add_char(state, (char)fb[i]);
        i++;
    }

    if (state->current_span.text_len > 0 && state->current_row < rows) {
        ik_serialize_flush_span(state);
    }
}

void ik_serialize_ensure_empty_rows(line_t *lines, int32_t rows)
{
    for (int32_t r = 0; r < rows; r++) {
        if (lines[r].span_count == 0) {
            lines[r].spans[0].text = talloc_strdup(lines, "");
            if (lines[r].spans[0].text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            lines[r].spans[0].text_len = 0;
            lines[r].span_count = 1;
        }
    }
}

char *ik_serialize_build_json(TALLOC_CTX *ctx, line_t *lines, int32_t rows, int32_t cols,
                               int32_t cursor_row, int32_t cursor_col, bool cursor_visible,
                               TALLOC_CTX *tmp)
{
    char *json = talloc_asprintf(ctx,
        "{\"type\":\"framebuffer\",\"rows\":%" PRId32 ",\"cols\":%" PRId32 ","
        "\"cursor\":{\"row\":%" PRId32 ",\"col\":%" PRId32 ",\"visible\":%s},\"lines\":[",
        rows, cols, cursor_row, cursor_col, cursor_visible ? "true" : "false");
    if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (int32_t r = 0; r < rows; r++) {
        if (r > 0) {
            json = talloc_asprintf_append(json, ",");
            if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        json = ik_serialize_build_line_json(json, &lines[r], tmp);
    }

    json = talloc_asprintf_append(json, "]}");
    if (json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return json;
}

res_t ik_serialize_framebuffer(TALLOC_CTX *ctx,
                                const uint8_t *framebuffer,
                                size_t framebuffer_len,
                                int32_t rows,
                                int32_t cols,
                                int32_t cursor_row,
                                int32_t cursor_col,
                                bool cursor_visible)
{
    if (framebuffer == NULL) {
        return ERR(ctx, INVALID_ARG, "framebuffer is NULL");
    }

    TALLOC_CTX *tmp = talloc_new(ctx);
    if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    line_t *lines = talloc_zero_array(tmp, line_t, (uint32_t)rows);
    if (lines == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (int32_t i = 0; i < rows; i++) {
        lines[i].row = i;
        lines[i].span_cap = 4;
        lines[i].spans = talloc_zero_array(lines, span_t, 4);
        if (lines[i].spans == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    parse_state_t state = {
        .lines = lines,
        .current_row = 0,
        .current_style = {0},
        .current_span = {0},
        .tmp = tmp
    };
    state.current_span.text_cap = 256;
    state.current_span.text = talloc_zero_array(tmp, char, 256);
    if (state.current_span.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_serialize_parse_framebuffer(&state, framebuffer, framebuffer_len, rows);
    ik_serialize_ensure_empty_rows(lines, rows);

    char *json = ik_serialize_build_json(ctx, lines, rows, cols, cursor_row, cursor_col, cursor_visible, tmp);
    talloc_free(tmp);
    return OK(json);
}
