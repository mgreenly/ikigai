/**
 * @file event_render.c
 * @brief Universal event renderer implementation
 */

#include "event_render.h"

#include "ansi.h"
#include "panic.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

bool ik_event_renders_visible(const char *kind)
{
    if (kind == NULL) {
        return false;
    }

    // These kinds render visible content
    if (strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "system") == 0 ||
        strcmp(kind, "mark") == 0) {
        return true;
    }

    // These kinds do not render visible content
    // rewind: action is handled separately (truncation)
    // clear: action is handled separately (clear scrollback)
    return false;
}

// Helper: apply color styling to content based on color code
static char *apply_style(TALLOC_CTX *ctx, const char *content, uint8_t color)
{
    if (!ik_ansi_colors_enabled() || color == 0) {
        return talloc_strdup(ctx, content);
    }

    char color_seq[16];
    ik_ansi_fg_256(color_seq, sizeof(color_seq), color);
    return talloc_asprintf(ctx, "%s%s%s", color_seq, content, IK_ANSI_RESET);
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
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

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

// Helper: render content event (user, assistant, system, tool_call, tool_result)
static res_t render_content_event(ik_scrollback_t *scrollback, const char *content, uint8_t color)
{
    // Content can be NULL (e.g., empty system message)
    if (content == NULL || content[0] == '\0') {
        return OK(NULL);
    }

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Trim trailing whitespace
    char *trimmed = ik_scrollback_trim_trailing(tmp, content, strlen(content));

    // Skip if empty after trimming
    if (trimmed[0] == '\0') {
        talloc_free(tmp);
        return OK(NULL);
    }

    // Apply color styling
    char *styled = apply_style(tmp, trimmed, color);
    if (styled == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Append content
    res_t result = ik_scrollback_append_line_(scrollback, styled, strlen(styled));
    if (is_err(&result)) {
        talloc_free(tmp);
        return result;
    }

    // Append blank line for spacing
    result = ik_scrollback_append_line_(scrollback, "", 0);

    talloc_free(tmp);
    return result;
}

res_t ik_event_render(ik_scrollback_t *scrollback,
                      const char *kind,
                      const char *content,
                      const char *data_json)
{
    assert(scrollback != NULL); // LCOV_EXCL_BR_LINE

    // Validate kind parameter - return error instead of asserting
    // This allows callers to handle invalid kinds gracefully
    if (kind == NULL) { // LCOV_EXCL_START
        return ERR(scrollback, INVALID_ARG, "kind parameter cannot be NULL");
    } // LCOV_EXCL_STOP

    // Determine color based on kind
    uint8_t color = 0;
    if (strcmp(kind, "assistant") == 0) {
        color = IK_ANSI_GRAY_LIGHT;  // 249 - slightly subdued
    } else if (strcmp(kind, "tool_call") == 0 ||
               strcmp(kind, "tool_result") == 0 ||
               strcmp(kind, "system") == 0) {
        color = IK_ANSI_GRAY_SUBDUED;  // 242 - very subdued
    }
    // user, mark, rewind, clear: color = 0 (no color)

    // Handle each event kind
    if (strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "system") == 0 ||
        strcmp(kind, "tool_call") == 0 ||
        strcmp(kind, "tool_result") == 0) {
        return render_content_event(scrollback, content, color);
    }

    if (strcmp(kind, "mark") == 0) {
        return render_mark_event(scrollback, data_json);
    }

    if (strcmp(kind, "rewind") == 0 || strcmp(kind, "clear") == 0 || strcmp(kind, "agent_killed") == 0) {
        // These events don't render visual content
        // rewind: truncation is handled by replay logic
        // clear: clearing is handled by caller
        // agent_killed: metadata event for tracking killed agents
        return OK(NULL);
    }

    // Unknown kind - should not happen with valid database data
    return ERR(scrollback, INVALID_ARG, "Unknown event kind: %s", kind);
}
