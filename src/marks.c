#include "marks.h"

#include "event_render.h"
#include "openai/client.h"
#include "panic.h"
#include "repl.h"
#include "agent.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

/**
 * Generate ISO 8601 timestamp for current time
 *
 * @param parent  Talloc context
 * @return        Result containing ISO 8601 timestamp string (e.g., "2025-01-15T10:30:45Z")
 */
static res_t get_iso8601_timestamp(void *parent)
{
    time_t now = time(NULL);
    struct tm *tm_utc = gmtime_(&now);
    if (tm_utc == NULL) {
        return ERR(parent, IO, "gmtime failed to convert timestamp");
    }

    char *timestamp = talloc_array(parent, char, 32);
    if (timestamp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t len = strftime_(timestamp, 32, "%Y-%m-%dT%H:%M:%SZ", tm_utc);
    if (len == 0) {
        talloc_free(timestamp);
        return ERR(parent, IO, "strftime failed to format timestamp");
    }

    return OK(timestamp);
}

res_t ik_mark_create(ik_repl_ctx_t *repl, const char *label)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    // label can be NULL for unlabeled marks

    // TODO(DI): This function violates DI principles - it reaches into repl internals
    // instead of taking explicit dependencies as parameters. See marks.h for details.

    // Create new mark
    ik_mark_t *mark = talloc_zero(repl, ik_mark_t);
    if (mark == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Record current conversation position
    mark->message_index = repl->current->conversation->message_count;

    // Copy label if provided
    if (label != NULL) {
        mark->label = talloc_strdup(mark, label);
        if (mark->label == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    } else {
        mark->label = NULL;
    }

    // Generate timestamp - allocate on repl context so error survives if mark is freed
    res_t ts_res = get_iso8601_timestamp(repl);
    if (is_err(&ts_res)) {
        talloc_free(mark);
        return ts_res;
    }
    // Steal timestamp to mark context
    mark->timestamp = talloc_steal(mark, ts_res.ok);

    // Add mark to marks array
    size_t new_count = repl->current->mark_count + 1;
    ik_mark_t **new_marks = talloc_realloc(repl, repl->current->marks, ik_mark_t *, (unsigned int)new_count);
    if (new_marks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    new_marks[repl->current->mark_count] = mark;
    repl->current->marks = new_marks;
    repl->current->mark_count = new_count;

    // Reparent mark to marks array
    talloc_steal(repl->current->marks, mark);

    // Render mark event to scrollback (identical to replay)
    char *data_json;
    if (label != NULL) {
        data_json = talloc_asprintf(repl, "{\"label\":\"%s\"}", label);
    } else {
        data_json = talloc_strdup(repl, "{}");
    }
    if (data_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t result = ik_event_render(repl->current->scrollback, "mark", NULL, data_json);
    talloc_free(data_json);
    if (is_err(&result)) return result;  /* LCOV_EXCL_BR_LINE */

    return OK(NULL);
}

res_t ik_mark_find(ik_repl_ctx_t *repl, const char *label, ik_mark_t **mark_out)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(mark_out != NULL);  /* LCOV_EXCL_BR_LINE */
    // label can be NULL to find most recent mark

    // TODO(DI): This function violates DI principles - it reaches into repl internals.
    // See marks.h for details.

    if (repl->current->mark_count == 0) {
        return ERR(repl, INVALID_ARG, "No marks found");
    }

    // If no label specified, return most recent mark
    if (label == NULL) {
        *mark_out = repl->current->marks[repl->current->mark_count - 1];
        return OK(*mark_out);
    }

    // Search for mark with matching label (from most recent to oldest)
    for (size_t i = repl->current->mark_count; i > 0; i--) {
        ik_mark_t *mark = repl->current->marks[i - 1];
        if (mark->label != NULL && strcmp(mark->label, label) == 0) {
            *mark_out = mark;
            return OK(*mark_out);
        }
    }

    return ERR(repl, INVALID_ARG, "Mark not found: %s", label);
}

res_t ik_mark_rewind_to_mark(ik_repl_ctx_t *repl, ik_mark_t *target_mark)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(target_mark != NULL);  /* LCOV_EXCL_BR_LINE */

    // TODO(DI): This function violates DI principles - it reaches into repl internals.
    // See marks.h for details.

    res_t result;

    // Truncate conversation to mark position
    // Free messages after the mark position
    for (size_t i = target_mark->message_index; i < repl->current->conversation->message_count; i++) {
        talloc_free(repl->current->conversation->messages[i]);
    }
    repl->current->conversation->message_count = target_mark->message_index;

    // Remove marks after the target position (but keep the target mark itself)
    size_t target_index = 0;
    bool found = false;
    for (size_t i = 0; i < repl->current->mark_count; i++) {  /* LCOV_EXCL_BR_LINE */
        if (repl->current->marks[i] == target_mark) {
            target_index = i;
            found = true;
            break;
        }
    }
    assert(found);  /* LCOV_EXCL_BR_LINE */
    (void)found;  // Used only in assert (compiled out in release builds)

    // Free marks after target (keep target mark)
    for (size_t i = target_index + 1; i < repl->current->mark_count; i++) {
        talloc_free(repl->current->marks[i]);
    }
    repl->current->mark_count = target_index + 1;

    // Rebuild scrollback from remaining conversation
    ik_scrollback_clear(repl->current->scrollback);

    // Render system message first (if configured)
    if (repl->shared->cfg != NULL && repl->shared->cfg->openai_system_message != NULL) {
        result = ik_event_render(repl->current->scrollback, "system", repl->shared->cfg->openai_system_message, "{}");
        if (is_err(&result)) return result;  /* LCOV_EXCL_BR_LINE */
    }

    // Render conversation messages using event renderer (no role prefixes)
    for (size_t i = 0; i < repl->current->conversation->message_count; i++) {
        ik_msg_t *msg = repl->current->conversation->messages[i];
        result = ik_event_render(repl->current->scrollback, msg->kind, msg->content, "{}");
        if (is_err(&result)) return result;  /* LCOV_EXCL_BR_LINE */
    }

    // Re-add mark indicators for remaining marks (including the target mark)
    for (size_t i = 0; i < repl->current->mark_count; i++) {
        ik_mark_t *mark = repl->current->marks[i];
        char *data_json;
        if (mark->label != NULL) {
            data_json = talloc_asprintf(NULL, "{\"label\":\"%s\"}", mark->label);
        } else {
            data_json = talloc_strdup(NULL, "{}");
        }
        if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        result = ik_event_render(repl->current->scrollback, "mark", NULL, data_json);
        talloc_free(data_json);
        if (is_err(&result)) return result;  /* LCOV_EXCL_BR_LINE */
    }

    // Rewind events don't render anything visible to scrollback
    // (The visual state is simply: conversation + marks after rewind)

    return OK(NULL);
}

res_t ik_mark_rewind_to(ik_repl_ctx_t *repl, const char *label)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    // label can be NULL to rewind to most recent mark

    // TODO(DI): This function violates DI principles - see marks.h for details.

    // Find the target mark
    ik_mark_t *target_mark;
    res_t result = ik_mark_find(repl, label, &target_mark);
    if (is_err(&result)) return result;

    // Rewind to the found mark
    return ik_mark_rewind_to_mark(repl, target_mark);
}
