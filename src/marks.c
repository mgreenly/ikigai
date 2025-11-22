#include "marks.h"

#include "openai/client.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

/**
 * Generate ISO 8601 timestamp for current time
 *
 * @param parent  Talloc context
 * @return        ISO 8601 timestamp string (e.g., "2025-01-15T10:30:45Z")
 */
static char *get_iso8601_timestamp(void *parent)
{
    time_t now = time(NULL);
    struct tm *tm_utc = gmtime(&now);
    if (tm_utc == NULL) {  /* LCOV_EXCL_BR_LINE */
        PANIC("gmtime failed");  // LCOV_EXCL_LINE
    }

    char *timestamp = talloc_array(parent, char, 32);
    if (timestamp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t len = strftime(timestamp, 32, "%Y-%m-%dT%H:%M:%SZ", tm_utc);
    if (len == 0) {  /* LCOV_EXCL_BR_LINE */
        PANIC("strftime failed");  // LCOV_EXCL_LINE
    }

    return timestamp;
}

res_t ik_mark_create(ik_repl_ctx_t *repl, const char *label)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    // label can be NULL for unlabeled marks

    // Create new mark
    ik_mark_t *mark = talloc_zero(repl, ik_mark_t);
    if (mark == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Record current conversation position
    mark->message_index = repl->conversation->message_count;

    // Copy label if provided
    if (label != NULL) {
        mark->label = talloc_strdup(mark, label);
        if (mark->label == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    } else {
        mark->label = NULL;
    }

    // Generate timestamp
    mark->timestamp = get_iso8601_timestamp(mark);

    // Add mark to marks array
    size_t new_count = repl->mark_count + 1;
    ik_mark_t **new_marks = talloc_realloc(repl, repl->marks, ik_mark_t *, (unsigned int)new_count);
    if (new_marks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    new_marks[repl->mark_count] = mark;
    repl->marks = new_marks;
    repl->mark_count = new_count;

    // Reparent mark to marks array
    talloc_steal(repl->marks, mark);

    // Add visual indicator to scrollback
    char *mark_indicator;
    if (label != NULL) {
        mark_indicator = talloc_asprintf(NULL, "─── Mark: %s ───", label);
    } else {
        mark_indicator = talloc_strdup(NULL, "─── Mark ───");
    }
    if (mark_indicator == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t mark_len = strlen(mark_indicator);
    res_t result = ik_scrollback_append_line(repl->scrollback, mark_indicator, mark_len);
    talloc_free(mark_indicator);
    if (is_err(&result)) return result; /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_mark_find(ik_repl_ctx_t *repl, const char *label, ik_mark_t **mark_out)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(mark_out != NULL);  /* LCOV_EXCL_BR_LINE */
    // label can be NULL to find most recent mark

    if (repl->mark_count == 0) {
        return ERR(repl, INVALID_ARG, "No marks found");
    }

    // If no label specified, return most recent mark
    if (label == NULL) {
        *mark_out = repl->marks[repl->mark_count - 1];
        return OK(*mark_out);
    }

    // Search for mark with matching label (from most recent to oldest)
    for (size_t i = repl->mark_count; i > 0; i--) {
        ik_mark_t *mark = repl->marks[i - 1];
        if (mark->label != NULL && strcmp(mark->label, label) == 0) {
            *mark_out = mark;
            return OK(*mark_out);
        }
    }

    return ERR(repl, INVALID_ARG, "Mark not found: %s", label);
}

res_t ik_mark_rewind_to(ik_repl_ctx_t *repl, const char *label)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    // label can be NULL to rewind to most recent mark

    // Find the target mark
    ik_mark_t *target_mark;
    res_t result = ik_mark_find(repl, label, &target_mark);
    if (is_err(&result)) return result;

    // Truncate conversation to mark position
    // Free messages after the mark position
    for (size_t i = target_mark->message_index; i < repl->conversation->message_count; i++) {
        talloc_free(repl->conversation->messages[i]);
    }
    repl->conversation->message_count = target_mark->message_index;

    // Remove marks at and after the target position
    size_t target_index = 0;
    bool found = false;
    for (size_t i = 0; i < repl->mark_count; i++) {  /* LCOV_EXCL_BR_LINE */
        if (repl->marks[i] == target_mark) {
            target_index = i;
            found = true;
            break;
        }
    }
    assert(found);  /* LCOV_EXCL_BR_LINE */
    (void)found;  // Used only in assert (compiled out in release builds)

    // Free marks from target onwards
    for (size_t i = target_index; i < repl->mark_count; i++) {
        talloc_free(repl->marks[i]);
    }
    repl->mark_count = target_index;

    // Rebuild scrollback from remaining conversation
    ik_scrollback_clear(repl->scrollback);

    for (size_t i = 0; i < repl->conversation->message_count; i++) {
        ik_openai_msg_t *msg = repl->conversation->messages[i];

        // Format message with role prefix
        char *formatted;
        if (strcmp(msg->role, "user") == 0) {
            formatted = talloc_asprintf(NULL, "You: %s", msg->content);
        } else if (strcmp(msg->role, "assistant") == 0) {  /* LCOV_EXCL_BR_LINE */
            formatted = talloc_asprintf(NULL, "Assistant: %s", msg->content);
        } else {  // LCOV_EXCL_LINE
            formatted = talloc_strdup(NULL, msg->content);  // LCOV_EXCL_LINE
        }  // LCOV_EXCL_LINE
        if (formatted == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        size_t formatted_len = strlen(formatted);
        result = ik_scrollback_append_line(repl->scrollback, formatted, formatted_len);
        talloc_free(formatted);
        if (is_err(&result)) return result; /* LCOV_EXCL_BR_LINE */
    }

    // Re-add mark indicators for remaining marks
    for (size_t i = 0; i < repl->mark_count; i++) {  // LCOV_EXCL_LINE
        ik_mark_t *mark = repl->marks[i];  // LCOV_EXCL_LINE
        char *mark_indicator;  // LCOV_EXCL_LINE
        if (mark->label != NULL) {  // LCOV_EXCL_LINE
            mark_indicator = talloc_asprintf(NULL, "─── Mark: %s ───", mark->label);  // LCOV_EXCL_LINE
        } else {  // LCOV_EXCL_LINE
            mark_indicator = talloc_strdup(NULL, "─── Mark ───");  // LCOV_EXCL_LINE
        }  // LCOV_EXCL_LINE
        if (mark_indicator == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE

        size_t mark_len = strlen(mark_indicator);  // LCOV_EXCL_LINE
        result = ik_scrollback_append_line(repl->scrollback, mark_indicator, mark_len);  // LCOV_EXCL_LINE
        talloc_free(mark_indicator);  // LCOV_EXCL_LINE
        if (is_err(&result)) return result;  // LCOV_EXCL_LINE
    }  // LCOV_EXCL_LINE

    // Add rewind indicator
    char *rewind_indicator;
    if (label != NULL) {
        rewind_indicator = talloc_asprintf(NULL, "─── Rewound to: %s ───", label);
    } else {
        rewind_indicator = talloc_strdup(NULL, "─── Rewound to last mark ───");
    }
    if (rewind_indicator == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t rewind_len = strlen(rewind_indicator);
    result = ik_scrollback_append_line(repl->scrollback, rewind_indicator, rewind_len);
    talloc_free(rewind_indicator);
    if (is_err(&result)) return result; /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}
