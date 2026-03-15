/**
 * @file internal_tool_pinspect.c
 * @brief pinspect internal tool handler — read background process output
 */

#include "apps/ikigai/internal_tools_bg.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/bg_ansi.h"
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/bg_process_io.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <talloc.h>

#include "shared/poison.h"

#define PINSPECT_MAX_LINES   200
#define PINSPECT_MAX_BYTES   (50 * 1024)
#define PINSPECT_DEFAULT_TAIL 50

/* Local helpers */

static bg_process_t *find_process(bg_manager_t *mgr, int32_t id)
{
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->processes[i]->id == id) return mgr->processes[i];
    }
    return NULL;
}

static const char *status_to_str(bg_status_t s)
{
    switch (s) {
        case BG_STATUS_STARTING:  return "starting";
        case BG_STATUS_RUNNING:   return "running";
        case BG_STATUS_EXITED:    return "exited";
        case BG_STATUS_KILLED:    return "killed";
        case BG_STATUS_TIMED_OUT: return "timed_out";
        case BG_STATUS_FAILED:    return "failed";
        default:                  return "unknown"; // LCOV_EXCL_LINE
    }
}

static int64_t age_seconds(const bg_process_t *proc)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t age = (int64_t)(now.tv_sec - proc->started_at.tv_sec);
    return age < 0 ? 0 : age;
}

static int64_t ttl_remaining(const bg_process_t *proc, int64_t age)
{
    if (proc->ttl_seconds < 0) return -1;
    bg_status_t s = proc->status;
    if (s != BG_STATUS_RUNNING && s != BG_STATUS_STARTING) return 0;
    int64_t rem = (int64_t)proc->ttl_seconds - age;
    return rem < 0 ? 0 : rem;
}

static char *json_from_mut_doc(TALLOC_CTX *ctx, yyjson_mut_doc *doc)
{
    char *json = yyjson_mut_write(doc, 0, NULL);
    if (json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    char *copy = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return copy;
}

/* ================================================================
 * pinspect handler
 * ================================================================ */

char *ik_bg_pinspect_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(agent != NULL);     // LCOV_EXCL_BR_LINE
    assert(args_json != NULL); // LCOV_EXCL_BR_LINE

    if (agent->bg_manager == NULL) {
        return ik_tool_wrap_failure(ctx, "Background process support not initialized",
                                    "NOT_INITIALIZED");
    }

    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) { // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse pinspect arguments", "PARSE_ERROR"); // LCOV_EXCL_LINE
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *id_val   = yyjson_obj_get_(root, "id");
    yyjson_val *mode_val = yyjson_obj_get_(root, "mode");
    yyjson_val *tail_val = yyjson_obj_get_(root, "tail_lines");
    yyjson_val *sl_val   = yyjson_obj_get_(root, "start_line");
    yyjson_val *el_val   = yyjson_obj_get_(root, "end_line");

    if (id_val == NULL || !yyjson_is_int(id_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: id", "INVALID_ARG");
    }

    int32_t id = (int32_t)yyjson_get_sint_(id_val);

    const char *mode_str = (mode_val && yyjson_is_str(mode_val))
                           ? yyjson_get_str_(mode_val) : "tail";
    int64_t tail_lines   = (tail_val && yyjson_is_int(tail_val))
                           ? yyjson_get_sint_(tail_val) : PINSPECT_DEFAULT_TAIL;
    int64_t start_line   = (sl_val && yyjson_is_int(sl_val))
                           ? yyjson_get_sint_(sl_val) : 1;
    int64_t end_line     = (el_val && yyjson_is_int(el_val))
                           ? yyjson_get_sint_(el_val) : 0;

    yyjson_doc_free(doc);

    bg_process_t *proc = find_process(agent->bg_manager, id);
    if (proc == NULL) {
        return ik_tool_wrap_failure(ctx, "Process not found", "NOT_FOUND");
    }

    /* Determine read mode and apply line cap */
    bg_read_mode_t read_mode;
    if (strcmp(mode_str, "since_last") == 0) {
        read_mode = BG_READ_SINCE_LAST;
    } else if (strcmp(mode_str, "lines") == 0) {
        read_mode = BG_READ_RANGE;
        if (end_line == 0) end_line = start_line + PINSPECT_MAX_LINES - 1;
        if (end_line - start_line + 1 > PINSPECT_MAX_LINES)
            end_line = start_line + PINSPECT_MAX_LINES - 1;
    } else {
        read_mode = BG_READ_TAIL;
        if (tail_lines > PINSPECT_MAX_LINES) tail_lines = PINSPECT_MAX_LINES;
        if (tail_lines < 1) tail_lines = 1;
    }

    int64_t total_lines = bg_line_index_count(proc->line_index);
    int64_t age         = age_seconds(proc);
    int64_t ttl_rem     = ttl_remaining(proc, age);

    uint8_t *raw_buf = NULL;
    size_t   raw_len = 0;
    res_t res = bg_process_read_output(proc, ctx, read_mode,
                                       tail_lines, start_line, end_line,
                                       &raw_buf, &raw_len);
    if (is_err(&res)) {
        char *msg = talloc_asprintf(ctx, "Failed to read output: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, msg, "READ_FAILED");
    }

    /* Strip ANSI and apply size cap */
    const char *stripped = "";
    size_t      stripped_len = 0;
    if (raw_buf != NULL && raw_len > 0) {
        char *s = bg_ansi_strip(ctx, (const char *)raw_buf, raw_len);
        stripped_len = strlen(s);
        if (stripped_len > PINSPECT_MAX_BYTES) {
            s[PINSPECT_MAX_BYTES] = '\0';
            stripped_len = PINSPECT_MAX_BYTES;
        }
        stripped = s;
    }
    (void)stripped_len;

    /* Build response */
    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);
    if (!rroot) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);

    yyjson_mut_obj_add_int_(rdoc, rroot, "id",                   (int64_t)proc->id);
    yyjson_mut_obj_add_str_(rdoc, rroot, "status",               status_to_str(proc->status));
    yyjson_mut_obj_add_int_(rdoc, rroot, "age_seconds",          age);
    yyjson_mut_obj_add_int_(rdoc, rroot, "ttl_remaining_seconds", ttl_rem);
    yyjson_mut_obj_add_int_(rdoc, rroot, "total_lines",          total_lines);
    yyjson_mut_obj_add_int_(rdoc, rroot, "total_bytes",          proc->total_bytes);
    yyjson_mut_obj_add_str_(rdoc, rroot, "output",               stripped);

    return ik_tool_wrap_success(ctx, json_from_mut_doc(ctx, rdoc));
}
