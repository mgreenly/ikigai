// Resource-conflict-aware parallel tool call scheduler — execution
#include "apps/ikigai/tool_scheduler.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/format.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_executor.h"
#include "apps/ikigai/wrapper_pthread.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

// ---------------------------------------------------------------------------
// Internal: display helpers
// ---------------------------------------------------------------------------

// Build a display summary for a scheduler entry: "tool_name(first_string_arg)"
// or just "tool_name" if no string argument is found.
static const char *entry_summary(TALLOC_CTX *ctx, const ik_schedule_entry_t *e)
{
    const char *name = e->tool_call->name ? e->tool_call->name : "(unknown)";
    const char *args = e->tool_call->arguments;
    if (args == NULL || args[0] == '\0') return talloc_strdup(ctx, name);

    yyjson_doc *doc = yyjson_read(args, strlen(args), 0);
    if (doc == NULL) return talloc_strdup(ctx, name);

    yyjson_val *root = yyjson_doc_get_root(doc);
    const char *result = NULL;
    if (yyjson_is_obj(root)) {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(root, &iter);
        yyjson_val *key;
        while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
            yyjson_val *val = yyjson_obj_iter_get_val(key);
            if (yyjson_is_str(val)) {
                result = talloc_asprintf(ctx, "%s(%s)", name, yyjson_get_str(val));
                break;
            }
        }
    }
    yyjson_doc_free(doc);
    return result != NULL ? result : talloc_strdup(ctx, name);
}

static void append_with_blank(ik_tool_scheduler_t *sched, const char *line)
{
    if (!sched->agent || !sched->agent->scrollback) return;
    ik_scrollback_append_subdued_line(sched->agent->scrollback, line);
    ik_scrollback_append_line(sched->agent->scrollback, "", 0);
}

// Append icon-labeled status line (▶/◇/✗/⊘) to scrollback with blank line.
static void display_transition(ik_tool_scheduler_t *sched, int32_t idx,
                                const char *label, const char *extra)
{
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) return;

    const char *summary = entry_summary(tmp, &sched->entries[idx]);
    const char *line = (extra != NULL)
        ? talloc_asprintf(tmp, "%s: %s%s", label, summary, extra)
        : talloc_asprintf(tmp, "%s: %s", label, summary);

    if (line != NULL) append_with_blank(sched, line);
    talloc_free(tmp);
}

// Show "→ tool_name: args" input line with blank separator.
static void display_tool_input(ik_tool_scheduler_t *sched, int32_t idx)
{
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) return;

    const char *line = ik_format_tool_call(tmp, sched->entries[idx].tool_call);
    if (line != NULL) append_with_blank(sched, line);
    talloc_free(tmp);
}

// Show "← tool_name: result" output line with blank separator.
static void display_tool_output(ik_tool_scheduler_t *sched, int32_t idx)
{
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) return;

    ik_schedule_entry_t *e = &sched->entries[idx];
    const char *result_json = e->result != NULL ? e->result : "{}";
    const char *line = ik_format_tool_result(tmp, e->tool_call->name, result_json);
    if (line != NULL) append_with_blank(sched, line);
    talloc_free(tmp);
}

// ---------------------------------------------------------------------------
// Internal: grow entries array
// ---------------------------------------------------------------------------

static void ensure_capacity(ik_tool_scheduler_t *sched)
{
    if (sched->count < sched->capacity) return;

    int32_t new_cap = sched->capacity == 0 ? 8 : sched->capacity * 2;
    sched->entries = talloc_realloc(sched, sched->entries, ik_schedule_entry_t,
                                    (unsigned int)new_cap);
    if (sched->entries == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    sched->capacity = new_cap;
}

// ---------------------------------------------------------------------------
// Internal: error cascading
// ---------------------------------------------------------------------------

// Cascade skips from entry[skipped_index] to all entries that depend on it.
// Recursively processes newly-skipped entries.
static void cascade_skips(ik_tool_scheduler_t *sched, int32_t skipped_index)
{
    const char *cause_name = sched->entries[skipped_index].tool_call->name;
    for (int32_t i = skipped_index + 1; i < sched->count; i++) {
        ik_schedule_entry_t *e = &sched->entries[i];
        if (ik_schedule_is_terminal(e->status)) continue;
        for (int32_t k = 0; k < e->blocked_by_count; k++) {
            if (e->blocked_by[k] == skipped_index) {
                e->status = IK_SCHEDULE_SKIPPED;
                TALLOC_CTX *tmp = talloc_new(NULL);
                if (tmp != NULL) {
                    const char *extra = talloc_asprintf(tmp,
                        " \xe2\x80\x94 prerequisite failed: %s", cause_name);
                    display_transition(sched, i, "⊘ Skipped", extra);
                    talloc_free(tmp);
                }
                cascade_skips(sched, i);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Internal: worker thread
// ---------------------------------------------------------------------------

typedef struct {
    TALLOC_CTX          *ctx;
    char                *tool_name;
    char                *arguments;
    ik_tool_scheduler_t *scheduler;
    int32_t              entry_index;
    ik_tool_registry_t  *registry;
    ik_paths_t          *paths;
    const char          *agent_uuid;
} sched_worker_args_t;

// Worker thread: execute one tool call and signal completion via per-entry mutex.
static void *sched_worker(void *arg)
{
    sched_worker_args_t *a = (sched_worker_args_t *)arg;
    ik_schedule_entry_t *e = &a->scheduler->entries[a->entry_index];

    char *result = ik_tool_execute_from_registry(
        a->ctx, a->registry, a->paths,
        a->agent_uuid, a->tool_name, a->arguments,
        &e->child_pid);

    if (result == NULL) {
        result = talloc_strdup(a->ctx,
                               "{\"type\":\"error\",\"error\":\"null result\"}");
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Steal result into per-entry thread context (outlives worker args)
    e->result = talloc_steal(e->thread_ctx, result);

    pthread_mutex_lock_(&e->mutex);
    e->thread_complete = true;
    pthread_mutex_unlock_(&e->mutex);

    return NULL;
}

// ---------------------------------------------------------------------------
// Internal: spawn a worker for entry[index]
// ---------------------------------------------------------------------------

static void start_entry(ik_tool_scheduler_t *sched, int32_t index)
{
    ik_schedule_entry_t *e  = &sched->entries[index];

    if (sched->replay_mode) {
        e->status = IK_SCHEDULE_RUNNING;
        display_transition(sched, index, "▶ Running", NULL);
        return;
    }

    ik_agent_ctx_t      *ag = sched->agent;

    e->thread_ctx = talloc_new(sched);
    if (e->thread_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    sched_worker_args_t *args = talloc_zero(e->thread_ctx, sched_worker_args_t);
    if (args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    args->ctx         = e->thread_ctx;
    args->tool_name   = talloc_strdup(e->thread_ctx, e->tool_call->name);
    args->arguments   = talloc_strdup(e->thread_ctx, e->tool_call->arguments);
    args->scheduler   = sched;
    args->entry_index = index;
    args->registry    = ag->shared ? ag->shared->tool_registry : NULL;
    args->paths       = ag->shared ? ag->shared->paths : NULL;
    args->agent_uuid  = ag->uuid;

    if (args->tool_name == NULL || args->arguments == NULL) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    pthread_mutex_lock_(&e->mutex);
    e->thread_complete = false;
    e->thread_started  = true;
    e->status          = IK_SCHEDULE_RUNNING;
    pthread_mutex_unlock_(&e->mutex);

    display_transition(sched, index, "▶ Running", NULL);

    int ret = pthread_create_(&e->thread, NULL, sched_worker, args);
    if (ret != 0) { // LCOV_EXCL_BR_LINE
        pthread_mutex_lock_(&e->mutex); // LCOV_EXCL_LINE
        e->thread_started = false;      // LCOV_EXCL_LINE
        e->status = IK_SCHEDULE_QUEUED; // LCOV_EXCL_LINE
        pthread_mutex_unlock_(&e->mutex); // LCOV_EXCL_LINE
        PANIC("pthread_create failed");  // LCOV_EXCL_LINE
    }
}

// ---------------------------------------------------------------------------
// Public: promote queued entries
// ---------------------------------------------------------------------------

void ik_tool_scheduler_promote(ik_tool_scheduler_t *sched)
{
    assert(sched != NULL); // LCOV_EXCL_BR_LINE

    for (int32_t i = 0; i < sched->count; i++) {
        ik_schedule_entry_t *e = &sched->entries[i];
        if (e->status != IK_SCHEDULE_QUEUED) continue;

        // All blockers must be terminal before we can proceed
        bool all_done = true;
        for (int32_t k = 0; k < e->blocked_by_count; k++) {
            if (!ik_schedule_is_terminal(sched->entries[e->blocked_by[k]].status)) {
                all_done = false;
                break;
            }
        }
        if (!all_done) continue;

        // If any blocker errored/skipped, cascade skip instead of running
        bool should_skip = false;
        for (int32_t k = 0; k < e->blocked_by_count; k++) {
            ik_schedule_status_t bs = sched->entries[e->blocked_by[k]].status;
            if (bs == IK_SCHEDULE_ERRORED || bs == IK_SCHEDULE_SKIPPED) {
                should_skip = true;
                break;
            }
        }
        if (should_skip) {
            const char *prereq_name = "prerequisite";
            for (int32_t k = 0; k < e->blocked_by_count; k++) {
                ik_schedule_status_t bs = sched->entries[e->blocked_by[k]].status;
                if (bs == IK_SCHEDULE_ERRORED || bs == IK_SCHEDULE_SKIPPED) {
                    prereq_name = sched->entries[e->blocked_by[k]].tool_call->name;
                    break;
                }
            }
            e->status = IK_SCHEDULE_SKIPPED;
            TALLOC_CTX *tmp = talloc_new(NULL);
            if (tmp != NULL) {
                const char *extra = talloc_asprintf(tmp,
                    " \xe2\x80\x94 prerequisite failed: %s", prereq_name);
                display_transition(sched, i, "⊘ Skipped", extra);
                talloc_free(tmp);
            }
            cascade_skips(sched, i);
            continue;
        }

        start_entry(sched, i);
    }
}

// ---------------------------------------------------------------------------
// Public: add a tool call
// ---------------------------------------------------------------------------

res_t ik_tool_scheduler_add(ik_tool_scheduler_t *sched, ik_tool_call_t *tool_call)
{
    assert(sched != NULL);     // LCOV_EXCL_BR_LINE
    assert(tool_call != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&sched->mutex);
    ensure_capacity(sched);

    int32_t idx            = sched->count;
    ik_schedule_entry_t *e = &sched->entries[idx];
    memset(e, 0, sizeof(*e));

    // Take ownership of the tool call
    e->tool_call = tool_call;
    talloc_steal(sched, tool_call);

    // Classify resource access (path strings allocated on sched context)
    e->access = ik_tool_scheduler_classify(sched, tool_call->name,
                                            tool_call->arguments);

    // Initialise per-entry mutex
    int mret = pthread_mutex_init_(&e->mutex, NULL);
    if (mret != 0) { // LCOV_EXCL_BR_LINE
        pthread_mutex_unlock_(&sched->mutex); // LCOV_EXCL_LINE
        return ERR(sched, INVALID_ARG, "pthread_mutex_init failed"); // LCOV_EXCL_LINE
    }

    // Detect conflicts with non-terminal predecessors; record blockers
    e->blocked_by       = NULL;
    e->blocked_by_count = 0;

    for (int32_t i = 0; i < idx; i++) {
        ik_schedule_entry_t *prev = &sched->entries[i];
        if (ik_schedule_is_terminal(prev->status)) continue;
        if (!ik_tool_scheduler_conflicts(&e->access, &prev->access)) continue;

        e->blocked_by = talloc_realloc(sched, e->blocked_by, int32_t,
                                        (unsigned int)(e->blocked_by_count + 1));
        if (e->blocked_by == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        e->blocked_by[e->blocked_by_count] = i;
        e->blocked_by_count++;
    }

    e->status = IK_SCHEDULE_QUEUED;
    sched->count++;
    pthread_mutex_unlock_(&sched->mutex);

    return OK(NULL);
}

void ik_tool_scheduler_begin(ik_tool_scheduler_t *sched)
{
    for (int32_t i = 0; i < sched->count; i++) {
        display_tool_input(sched, i);
        if (sched->entries[i].blocked_by_count > 0) {
            TALLOC_CTX *tmp = talloc_new(NULL);
            if (tmp != NULL) {
                int32_t bi = sched->entries[i].blocked_by[0];
                const char *bs = entry_summary(tmp, &sched->entries[bi]);
                const char *ex = talloc_asprintf(tmp, " \xe2\x80\x94 waiting on %s", bs);
                display_transition(sched, i, "◇ Blocked", ex);
                talloc_free(tmp);
            }
        }
    }
    ik_tool_scheduler_promote(sched);
}

// ---------------------------------------------------------------------------
// Public: on_complete / on_error
// ---------------------------------------------------------------------------

void ik_tool_scheduler_on_complete(ik_tool_scheduler_t *sched, int32_t index,
                                    char *result)
{
    assert(sched != NULL);                     // LCOV_EXCL_BR_LINE
    assert(index >= 0 && index < sched->count); // LCOV_EXCL_BR_LINE

    ik_schedule_entry_t *e = &sched->entries[index];
    e->result = result;
    e->status = IK_SCHEDULE_COMPLETED;
    display_tool_output(sched, index);
    ik_tool_scheduler_promote(sched);
}

void ik_tool_scheduler_on_error(ik_tool_scheduler_t *sched, int32_t index,
                                 const char *error_msg)
{
    assert(sched != NULL);                     // LCOV_EXCL_BR_LINE
    assert(index >= 0 && index < sched->count); // LCOV_EXCL_BR_LINE

    ik_schedule_entry_t *e = &sched->entries[index];
    e->error = talloc_strdup(e->thread_ctx ? e->thread_ctx : (TALLOC_CTX *)sched,
                              error_msg != NULL ? error_msg : "");
    e->status = IK_SCHEDULE_ERRORED;
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp != NULL) {
        const char *msg = error_msg != NULL ? error_msg : "unknown error";
        const char *extra = talloc_asprintf(tmp, " \xe2\x80\x94 %s", msg);
        display_transition(sched, index, "✗ Errored", extra);
        talloc_free(tmp);
    }
    cascade_skips(sched, index);
    ik_tool_scheduler_promote(sched);
}

// ---------------------------------------------------------------------------
// Public: poll
// ---------------------------------------------------------------------------

void ik_tool_scheduler_poll(ik_tool_scheduler_t *sched)
{
    assert(sched != NULL); // LCOV_EXCL_BR_LINE

    for (int32_t i = 0; i < sched->count; i++) {
        ik_schedule_entry_t *e = &sched->entries[i];
        if (e->status != IK_SCHEDULE_RUNNING) continue;

        pthread_mutex_lock_(&e->mutex);
        bool done = e->thread_complete;
        pthread_mutex_unlock_(&e->mutex);

        if (!done) continue;

        // Join the finished thread and process its result
        pthread_join_(e->thread, NULL);
        e->thread_started = false;

        if (e->result != NULL) {
            ik_tool_scheduler_on_complete(sched, i, e->result);
        } else {
            ik_tool_scheduler_on_error(sched, i,
                e->error != NULL ? e->error : "tool returned no result");
        }
    }
}
