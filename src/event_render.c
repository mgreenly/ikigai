/**
 * @file event_render.c
 * @brief Universal event renderer implementation
 */

#include "event_render.h"

#include "ansi.h"
#include "output_style.h"
#include "panic.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "tmp_ctx.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
bool ik_event_renders_visible(const char *kind)
{
    if (kind == NULL) {
        return false;
    }

    // These kinds render visible content
    if (strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "mark") == 0 ||
        strcmp(kind, "command") == 0 ||
        strcmp(kind, "fork") == 0 ||
        strcmp(kind, "usage") == 0) {
        return true;
    }

    // These kinds do not render visible content
    // system: stored for LLM context but not shown in UI
    // rewind: action is handled separately (truncation)
    // clear: action is handled separately (clear scrollback)
    return false;
}

// Helper: apply color styling to content based on color code
// Applies color per-line so each line is self-contained for scrollback
static char *apply_style(TALLOC_CTX *ctx, const char *content, uint8_t color)
{
    if (!ik_ansi_colors_enabled() || color == 0) {
        return talloc_strdup(ctx, content);
    }

    char color_seq[16];
    ik_ansi_fg_256(color_seq, sizeof(color_seq), color);

    // Check if content has newlines - if not, use simple approach
    if (strchr(content, '\n') == NULL) {
        return talloc_asprintf(ctx, "%s%s%s", color_seq, content, IK_ANSI_RESET);
    }

    // Multi-line: apply color to each line individually
    char *result = talloc_strdup(ctx, "");
    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const char *line_start = content;
    const char *newline;

    while ((newline = strchr(line_start, '\n')) != NULL) {
        size_t line_len = (size_t)(newline - line_start);
        char *line = talloc_strndup(ctx, line_start, line_len);
        if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        char *new_result = talloc_asprintf(ctx, "%s%s%s%s\n",
                                           result, color_seq, line, IK_ANSI_RESET);
        if (new_result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        talloc_free(result);
        talloc_free(line);
        result = new_result;
        line_start = newline + 1;
    }

    // Handle final line (after last newline, if any content remains)
    if (*line_start != '\0') {
        char *new_result = talloc_asprintf(ctx, "%s%s%s%s",
                                           result, color_seq, line_start, IK_ANSI_RESET);
        if (new_result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        talloc_free(result);
        result = new_result;
    }

    return result;
}

// Helper: extract label from data_json
static char *extract_label_from_json(TALLOC_CTX *ctx, const char *data_json)
{
    if (data_json == NULL) {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
    if (doc == NULL) {
        return NULL;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *label_val = yyjson_obj_get_(root, "label");

    char *label = NULL;
    if (label_val != NULL && yyjson_is_str(label_val)) {
        const char *label_str = yyjson_get_str_(label_val);
        if (label_str != NULL && label_str[0] != '\0') {
            label = talloc_strdup(ctx, label_str);
            if (label == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    yyjson_doc_free(doc);
    return label;
}

// Helper: render mark event
static res_t render_mark_event(ik_scrollback_t *scrollback, const char *data_json)
{
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Extract label from data_json
    char *label = extract_label_from_json(tmp, data_json);

    // Format as "/mark LABEL" or "/mark" if no label
    char *text;
    if (label != NULL) {
        text = talloc_asprintf(tmp, "/mark %s", label);
    } else {
        text = talloc_strdup(tmp, "/mark");
    }
    if (text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Append mark text
    res_t result = ik_scrollback_append_line_(scrollback, text, strlen(text));
    if (is_err(&result)) {
        talloc_free(tmp);
        return result;
    }

    // Append blank line for spacing
    result = ik_scrollback_append_line_(scrollback, "", 0);

    talloc_free(tmp);
    return result;
}

// Helper: render command event
static res_t render_command_event(ik_scrollback_t *scrollback, const char *content, const char *data_json)
{
    TALLOC_CTX *tmp = tmp_ctx_create();

    // Extract echo from data_json
    char *echo = NULL;
    if (data_json != NULL) {
        yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
        if (doc != NULL) {
            yyjson_val *root = yyjson_doc_get_root_(doc);
            yyjson_val *echo_val = yyjson_obj_get_(root, "echo");
            if (echo_val != NULL && yyjson_is_str(echo_val)) {
                const char *echo_str = yyjson_get_str_(echo_val);
                if (echo_str != NULL && echo_str[0] != '\0') {
                    echo = talloc_strdup(tmp, echo_str);
                    if (echo == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                }
            }
            yyjson_doc_free(doc);
        }
    }

    // Render echo in gray if present
    if (echo != NULL) {
        int32_t color_code = ik_output_color(IK_OUTPUT_SLASH_CMD);
        uint8_t color = (color_code >= 0) ? (uint8_t)color_code : 0;     // LCOV_EXCL_BR_LINE
        char *styled_echo = apply_style(tmp, echo, color);
        if (styled_echo == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        res_t result = ik_scrollback_append_line_(scrollback, styled_echo, strlen(styled_echo));
        if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
            talloc_free(tmp);     // LCOV_EXCL_LINE
            return result;     // LCOV_EXCL_LINE
        }

        // Append blank line after echo
        result = ik_scrollback_append_line_(scrollback, "", 0);
        if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
            talloc_free(tmp);     // LCOV_EXCL_LINE
            return result;     // LCOV_EXCL_LINE
        }
    }

    // Render output in subdued yellow if present
    if (content != NULL && content[0] != '\0') {
        // Trim trailing whitespace
        char *trimmed = ik_scrollback_trim_trailing(tmp, content, strlen(content));

        if (trimmed[0] != '\0') {
            int32_t color_code = ik_output_color(IK_OUTPUT_SLASH_OUTPUT);
            uint8_t color = (color_code >= 0) ? (uint8_t)color_code : 0;     // LCOV_EXCL_BR_LINE
            char *styled_output = apply_style(tmp, trimmed, color);
            if (styled_output == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            res_t result = ik_scrollback_append_line_(scrollback, styled_output, strlen(styled_output));
            if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
                talloc_free(tmp);     // LCOV_EXCL_LINE
                return result;     // LCOV_EXCL_LINE
            }

            // Append blank line after output
            result = ik_scrollback_append_line_(scrollback, "", 0);
            if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
                talloc_free(tmp);     // LCOV_EXCL_LINE
                return result;     // LCOV_EXCL_LINE
            }
        }
    }

    talloc_free(tmp);
    return OK(NULL);
}

// Helper: render token usage line from data_json
static res_t render_token_usage(ik_scrollback_t *scrollback, const char *data_json)
{
    if (data_json == NULL) {
        return OK(NULL);
    }

    yyjson_doc *doc = yyjson_read_(data_json, strlen(data_json), 0);
    if (doc == NULL) {
        return OK(NULL);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    yyjson_val *in_val = yyjson_obj_get_(root, "input_tokens");
    yyjson_val *out_val = yyjson_obj_get_(root, "output_tokens");
    yyjson_val *think_val = yyjson_obj_get_(root, "thinking_tokens");

    int32_t in_tok = (in_val && yyjson_is_int(in_val)) ? (int32_t)yyjson_get_int(in_val) : 0;
    int32_t out_tok = (out_val && yyjson_is_int(out_val)) ? (int32_t)yyjson_get_int(out_val) : 0; // LCOV_EXCL_BR_LINE
    int32_t think_tok = (think_val && yyjson_is_int(think_val)) ? (int32_t)yyjson_get_int(think_val) : 0; // LCOV_EXCL_BR_LINE

    yyjson_doc_free(doc);

    int32_t total = in_tok + out_tok + think_tok;
    if (total == 0) {
        return OK(NULL);
    }

    TALLOC_CTX *tmp = tmp_ctx_create();

    char token_line[128];
    if (think_tok > 0) {
        snprintf(token_line, sizeof(token_line),
                 "Tokens: %d in + %d out + %d thinking = %d",
                 in_tok, out_tok, think_tok, total);
    } else {
        snprintf(token_line, sizeof(token_line),
                 "Tokens: %d in + %d out = %d",
                 in_tok, out_tok, in_tok + out_tok);
    }

    // Apply subdued color
    char *styled = apply_style(tmp, token_line, IK_ANSI_GRAY_SUBDUED);
    if (styled == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t result = ik_scrollback_append_line_(scrollback, styled, strlen(styled));
    talloc_free(tmp);
    return result;
}

// Helper: render content event (user, assistant, system, tool_call, tool_result)
static res_t render_content_event(ik_scrollback_t *scrollback, const char *content, uint8_t color, const char *prefix)
{
    // Content can be NULL (e.g., empty system message)
    if (content == NULL || content[0] == '\0') {
        return OK(NULL);
    }

    TALLOC_CTX *tmp = tmp_ctx_create();

    // Prepend prefix if provided
    char *with_prefix = NULL;
    if (prefix != NULL) {
        with_prefix = talloc_asprintf(tmp, "%s %s", prefix, content);
        if (with_prefix == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        content = with_prefix;
    }

    // Trim trailing whitespace
    char *trimmed = ik_scrollback_trim_trailing(tmp, content, strlen(content));

    // Skip if empty after trimming
    if (trimmed[0] == '\0') {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return OK(NULL);     // LCOV_EXCL_LINE
    }

    // Apply color styling
    char *styled = apply_style(tmp, trimmed, color);
    if (styled == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Append content
    res_t result = ik_scrollback_append_line_(scrollback, styled, strlen(styled));
    if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return result;     // LCOV_EXCL_LINE
    }

    // Append blank line for spacing
    result = ik_scrollback_append_line_(scrollback, "", 0);

    talloc_free(tmp);
    return result;
}

res_t ik_event_render(ik_scrollback_t *scrollback,
                      const char *kind,
                      const char *content,
                      const char *data_json,
                      bool interrupted)
{
    assert(scrollback != NULL); // LCOV_EXCL_BR_LINE

    // Validate kind parameter - return error instead of asserting
    // This allows callers to handle invalid kinds gracefully
    if (kind == NULL) { // LCOV_EXCL_START
        return ERR(scrollback, INVALID_ARG, "kind parameter cannot be NULL");
    } // LCOV_EXCL_STOP

    // Determine color and prefix based on kind using centralized output style system
    // If interrupted, override with cancelled style for user/tool_call/tool_result
    uint8_t color = 0;
    const char *prefix = NULL;

    if (interrupted && (strcmp(kind, "user") == 0 || strcmp(kind, "tool_call") == 0 || strcmp(kind, "tool_result") == 0)) {
        // Use cancelled style for interrupted messages
        int32_t color_code = ik_output_color(IK_OUTPUT_CANCELLED);
        color = (color_code >= 0) ? (uint8_t)color_code : 0;     // LCOV_EXCL_BR_LINE
        prefix = ik_output_prefix(IK_OUTPUT_CANCELLED);
    } else if (strcmp(kind, "assistant") == 0) {
        color = IK_ANSI_GRAY_LIGHT;  // 249 - slightly subdued
        prefix = ik_output_prefix(IK_OUTPUT_MODEL_TEXT);
    } else if (strcmp(kind, "user") == 0) {
        prefix = ik_output_prefix(IK_OUTPUT_USER_INPUT);
    } else if (strcmp(kind, "tool_call") == 0) {
        int32_t color_code = ik_output_color(IK_OUTPUT_TOOL_REQUEST);
        color = (color_code >= 0) ? (uint8_t)color_code : 0;     // LCOV_EXCL_BR_LINE
    } else if (strcmp(kind, "tool_result") == 0) {
        int32_t color_code = ik_output_color(IK_OUTPUT_TOOL_RESPONSE);
        color = (color_code >= 0) ? (uint8_t)color_code : 0;     // LCOV_EXCL_BR_LINE
    } else if (strcmp(kind, "fork") == 0) {
        int32_t color_code = ik_output_color(IK_OUTPUT_SLASH_OUTPUT);
        color = (color_code >= 0) ? (uint8_t)color_code : 0;     // LCOV_EXCL_BR_LINE
    }
    // mark, rewind, clear, command: handled separately

    // Handle each event kind
    if (strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "user") == 0 ||
        strcmp(kind, "tool_call") == 0 ||
        strcmp(kind, "tool_result") == 0 ||
        strcmp(kind, "fork") == 0) {
        return render_content_event(scrollback, content, color, prefix);
    }

    if (strcmp(kind, "command") == 0) {
        return render_command_event(scrollback, content, data_json);
    }

    if (strcmp(kind, "mark") == 0) {
        return render_mark_event(scrollback, data_json);
    }

    if (strcmp(kind, "usage") == 0) {
        // Usage events: render token usage + blank line
        res_t result = render_token_usage(scrollback, data_json);
        if (is_err(&result)) return result;
        return ik_scrollback_append_line_(scrollback, "", 0);
    }

    if (strcmp(kind, "system") == 0 ||
        strcmp(kind, "rewind") == 0 ||
        strcmp(kind, "clear") == 0 ||
        strcmp(kind, "agent_killed") == 0) {
        // These events don't render visual content
        // system: stored for LLM context but not shown in UI
        // rewind: truncation is handled by replay logic
        // clear: clearing is handled by caller
        // agent_killed: metadata event for tracking killed agents
        return OK(NULL);
    }

    // Unknown kind - should not happen with valid database data
    return ERR(scrollback, INVALID_ARG, "Unknown event kind: %s", kind);
}
