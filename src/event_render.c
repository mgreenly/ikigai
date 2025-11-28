/**
 * @file event_render.c
 * @brief Universal event renderer implementation
 */

#include "event_render.h"

#include "panic.h"
#include "scrollback.h"
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

    // Append to scrollback (use wrapper for testability)
    res_t result = ik_scrollback_append_line_(scrollback, text, strlen(text));

    talloc_free(tmp);
    return result;
}

// Helper: render content event (user, assistant, system)
static res_t render_content_event(ik_scrollback_t *scrollback, const char *content)
{
    // Content can be NULL (e.g., empty system message)
    if (content == NULL || content[0] == '\0') {
        return OK(NULL);
    }

    return ik_scrollback_append_line_(scrollback, content, strlen(content));
}

res_t ik_event_render(ik_scrollback_t *scrollback,
                      const char *kind,
                      const char *content,
                      const char *data_json)
{
    assert(scrollback != NULL); // LCOV_EXCL_BR_LINE
    assert(kind != NULL);       // LCOV_EXCL_BR_LINE

    // Handle each event kind
    if (strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "system") == 0) {
        return render_content_event(scrollback, content);
    }

    if (strcmp(kind, "mark") == 0) {
        return render_mark_event(scrollback, data_json);
    }

    if (strcmp(kind, "rewind") == 0 || strcmp(kind, "clear") == 0) {
        // These events don't render visual content
        // rewind: truncation is handled by replay logic
        // clear: clearing is handled by caller
        return OK(NULL);
    }

    // Unknown kind - should not happen with valid database data
    return ERR(scrollback, INVALID_ARG, "Unknown event kind: %s", kind);
}
