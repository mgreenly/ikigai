// Resource-conflict-aware parallel tool call scheduler — core
#include "apps/ikigai/tool_scheduler.h"

#include "apps/ikigai/tool.h"
#include "apps/ikigai/wrapper_pthread.h"
#include "shared/panic.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

// ---------------------------------------------------------------------------
// Internal: path overlap check
// ---------------------------------------------------------------------------

// Return true if any string in set_a appears in set_b.
static bool paths_overlap(char **a, int32_t na, char **b, int32_t nb)
{
    for (int32_t i = 0; i < na; i++) {
        for (int32_t j = 0; j < nb; j++) {
            if (a[i] != NULL && b[j] != NULL && strcmp(a[i], b[j]) == 0) {
                return true;
            }
        }
    }
    return false;
}

// Does a write (wmode/wpaths/wcount) conflict with an access
// (amode/apaths/acount)?  Reads-vs-reads is never a conflict; caller
// ensures this is only called when wmode represents a write side.
static bool rw_conflict(ik_access_mode_t wmode, char **wpaths, int32_t wcount,
                         ik_access_mode_t amode, char **apaths, int32_t acount)
{
    if (wmode == IK_ACCESS_NONE) return false;
    if (amode == IK_ACCESS_NONE) return false;
    if (wmode == IK_ACCESS_WILDCARD) return true;
    if (amode == IK_ACCESS_WILDCARD) return true;
    // Both PATHS: conflict only when a specific path overlaps.
    return paths_overlap(wpaths, wcount, apaths, acount);
}

// ---------------------------------------------------------------------------
// Public: conflict detection
// ---------------------------------------------------------------------------

bool ik_tool_scheduler_conflicts(const ik_access_t *a, const ik_access_t *b)
{
    assert(a != NULL); // LCOV_EXCL_BR_LINE
    assert(b != NULL); // LCOV_EXCL_BR_LINE

    // a's writes conflict with b's reads
    if (rw_conflict(a->write_mode, a->write_paths, a->write_path_count,
                    b->read_mode,  b->read_paths,  b->read_path_count)) {
        return true;
    }
    // b's writes conflict with a's reads
    if (rw_conflict(b->write_mode, b->write_paths, b->write_path_count,
                    a->read_mode,  a->read_paths,  a->read_path_count)) {
        return true;
    }
    // a's writes conflict with b's writes (same path or wildcard)
    if (rw_conflict(a->write_mode, a->write_paths, a->write_path_count,
                    b->write_mode, b->write_paths, b->write_path_count)) {
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public: access classification
// ---------------------------------------------------------------------------

// Build a single-path PATHS access array on ctx.
static char **make_single_path(TALLOC_CTX *ctx, const char *path)
{
    char **arr = talloc_array(ctx, char *, 1);
    if (arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    arr[0] = talloc_strdup(ctx, path != NULL ? path : "");
    if (arr[0] == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return arr;
}

// Build a mem-namespaced path (prefix "mem:") on ctx.
static char *mem_path(TALLOC_CTX *ctx, const char *raw)
{
    char *p = talloc_asprintf(ctx, "mem:%s", raw != NULL ? raw : "");
    if (p == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return p;
}

ik_access_t ik_tool_scheduler_classify(TALLOC_CTX *ctx, const char *tool_name,
                                        const char *args_json)
{
    assert(tool_name != NULL); // LCOV_EXCL_BR_LINE

    ik_access_t acc = {
        .read_mode        = IK_ACCESS_NONE,
        .write_mode       = IK_ACCESS_NONE,
        .read_paths       = NULL,
        .read_path_count  = 0,
        .write_paths      = NULL,
        .write_path_count = 0,
    };

    if (strcmp(tool_name, "file_read") == 0) {
        char *fp = ik_tool_arg_get_string(ctx, args_json, "file_path");
        acc.read_mode       = IK_ACCESS_PATHS;
        acc.read_paths      = make_single_path(ctx, fp);
        acc.read_path_count = 1;

    } else if (strcmp(tool_name, "file_edit") == 0) {
        char *fp = ik_tool_arg_get_string(ctx, args_json, "file_path");
        acc.read_mode        = IK_ACCESS_PATHS;
        acc.read_paths       = make_single_path(ctx, fp);
        acc.read_path_count  = 1;
        acc.write_mode       = IK_ACCESS_PATHS;
        acc.write_paths      = make_single_path(ctx, fp);
        acc.write_path_count = 1;

    } else if (strcmp(tool_name, "file_write") == 0) {
        char *fp = ik_tool_arg_get_string(ctx, args_json, "file_path");
        acc.write_mode       = IK_ACCESS_PATHS;
        acc.write_paths      = make_single_path(ctx, fp);
        acc.write_path_count = 1;

    } else if (strcmp(tool_name, "glob")  == 0 ||
               strcmp(tool_name, "grep")  == 0 ||
               strcmp(tool_name, "list")  == 0) {
        acc.read_mode = IK_ACCESS_WILDCARD;

    } else if (strcmp(tool_name, "bash") == 0) {
        acc.read_mode  = IK_ACCESS_WILDCARD;
        acc.write_mode = IK_ACCESS_WILDCARD;

    } else if (strcmp(tool_name, "web_fetch")  == 0 ||
               strcmp(tool_name, "web_search") == 0) {
        // Pure remote I/O: no local resource access — acc stays NONE/NONE

    } else if (strcmp(tool_name, "mem") == 0) {
        char *action = ik_tool_arg_get_string(ctx, args_json, "action");
        char *path   = ik_tool_arg_get_string(ctx, args_json, "path");
        if (action != NULL &&
            (strcmp(action, "get")          == 0 ||
             strcmp(action, "list")         == 0 ||
             strcmp(action, "revisions")    == 0 ||
             strcmp(action, "revision_get") == 0)) {
            char *mp = mem_path(ctx, path);
            acc.read_mode       = IK_ACCESS_PATHS;
            acc.read_paths      = make_single_path(ctx, mp);
            acc.read_path_count = 1;
        } else {
            // create / update / delete (or unknown action)
            char *mp = mem_path(ctx, path);
            acc.write_mode       = IK_ACCESS_PATHS;
            acc.write_paths      = make_single_path(ctx, mp);
            acc.write_path_count = 1;
        }

    } else {
        // Unknown / internal tools: treat as full barrier (WILDCARD/WILDCARD)
        acc.read_mode  = IK_ACCESS_WILDCARD;
        acc.write_mode = IK_ACCESS_WILDCARD;
    }

    return acc;
}

// ---------------------------------------------------------------------------
// Public: create / destroy
// ---------------------------------------------------------------------------

ik_tool_scheduler_t *ik_tool_scheduler_create(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    assert(agent != NULL); // LCOV_EXCL_BR_LINE

    ik_tool_scheduler_t *sched = talloc_zero(ctx, ik_tool_scheduler_t);
    if (sched == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    sched->agent           = agent;
    sched->stream_complete = false;
    sched->count           = 0;
    sched->capacity        = 0;
    sched->entries         = NULL;

    int ret = pthread_mutex_init_(&sched->mutex, NULL);
    if (ret != 0) PANIC("pthread_mutex_init failed"); // LCOV_EXCL_BR_LINE

    return sched;
}

void ik_tool_scheduler_destroy(ik_tool_scheduler_t *sched)
{
    if (sched == NULL) return;
    // Destroy per-entry mutexes; join any threads still running
    for (int32_t i = 0; i < sched->count; i++) {
        ik_schedule_entry_t *e = &sched->entries[i];
        if (e->thread_started) {
            pthread_join_(e->thread, NULL);
        }
        pthread_mutex_destroy_(&e->mutex);
    }
    pthread_mutex_destroy_(&sched->mutex);
    talloc_free(sched);
}

// ---------------------------------------------------------------------------
// Public: terminal check
// ---------------------------------------------------------------------------

bool ik_tool_scheduler_all_terminal(const ik_tool_scheduler_t *sched)
{
    assert(sched != NULL); // LCOV_EXCL_BR_LINE

    if (!sched->stream_complete) return false;
    if (sched->count == 0) return false;

    for (int32_t i = 0; i < sched->count; i++) {
        if (!ik_schedule_is_terminal(sched->entries[i].status)) return false;
    }
    return true;
}
