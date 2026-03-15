/**
 * @file internal_tool_pstart.c
 * @brief pstart internal tool handler — start a background process
 */

#include "apps/ikigai/internal_tools_bg.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* Local helpers (subset shared with internal_tools_bg.c) */

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
 * pstart handler
 * ================================================================ */

char *ik_bg_pstart_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
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
        return ik_tool_wrap_failure(ctx, "Failed to parse pstart arguments", "PARSE_ERROR"); // LCOV_EXCL_LINE
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *cmd_val   = yyjson_obj_get_(root, "command");
    yyjson_val *label_val = yyjson_obj_get_(root, "label");
    yyjson_val *ttl_val   = yyjson_obj_get_(root, "ttl_seconds");

    if (cmd_val == NULL || !yyjson_is_str(cmd_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: command", "INVALID_ARG");
    }
    if (label_val == NULL || !yyjson_is_str(label_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: label", "INVALID_ARG");
    }
    if (ttl_val == NULL || !yyjson_is_int(ttl_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: ttl_seconds", "INVALID_ARG");
    }

    const char *command     = yyjson_get_str_(cmd_val);
    const char *label       = yyjson_get_str_(label_val);
    int64_t     ttl_raw     = yyjson_get_sint_(ttl_val);
    int32_t     ttl_seconds = (int32_t)ttl_raw;

    char *command_copy = talloc_strdup(ctx, command);
    char *label_copy   = talloc_strdup(ctx, label);
    yyjson_doc_free(doc);
    if (!command_copy || !label_copy) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const char *agent_uuid = agent->uuid ? agent->uuid : "unknown";
    const char *base_dir   = "run/bg-processes";
    if (agent->shared && agent->shared->paths) {
        const char *runtime = ik_paths_get_runtime_dir(agent->shared->paths);
        base_dir = talloc_asprintf(ctx, "%s/bg-processes", runtime);
        if (!base_dir) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    ik_db_ctx_t  *db   = agent->worker_db_ctx;
    bg_process_t *proc = NULL;
    res_t res = bg_process_start(agent->bg_manager, db, base_dir,
                                 command_copy, label_copy, agent_uuid,
                                 ttl_seconds, &proc);
    if (is_err(&res)) {
        char *msg = talloc_asprintf(ctx, "Failed to start process: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, msg, "START_FAILED");
    }

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);
    if (!rroot) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);

    yyjson_mut_obj_add_int_(rdoc, rroot, "id",  (int64_t)proc->id);
    yyjson_mut_obj_add_int_(rdoc, rroot, "pid", (int64_t)proc->pid);
    yyjson_mut_obj_add_str_(rdoc, rroot, "status", status_to_str(proc->status));

    return ik_tool_wrap_success(ctx, json_from_mut_doc(ctx, rdoc));
}
