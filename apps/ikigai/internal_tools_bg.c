/**
 * @file internal_tools_bg.c
 * @brief Background process internal tool registration and pwrite/pkill/ps handlers
 *
 * pstart is in internal_tool_pstart.c, pread in internal_tool_pread.c.
 * This file contains schemas, registration, helpers shared with pwrite/pkill/ps,
 * and the pwrite, pkill, and ps handler implementations.
 */

#include "apps/ikigai/internal_tools_bg.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/bg_process_io.h"
#include "apps/ikigai/tool_registry.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/error.h"
#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <talloc.h>

#include "shared/poison.h"

/* ================================================================
 * Static helpers
 * ================================================================ */

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
 * pwrite handler
 * ================================================================ */

char *ik_bg_pwrite_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
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
        return ik_tool_wrap_failure(ctx, "Failed to parse pwrite arguments", "PARSE_ERROR"); // LCOV_EXCL_LINE
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *id_val    = yyjson_obj_get_(root, "id");
    yyjson_val *input_val = yyjson_obj_get_(root, "input");
    yyjson_val *close_val = yyjson_obj_get_(root, "close_stdin");

    if (id_val == NULL || !yyjson_is_int(id_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: id", "INVALID_ARG");
    }
    if (input_val == NULL || !yyjson_is_str(input_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: input", "INVALID_ARG");
    }

    int32_t     id         = (int32_t)yyjson_get_sint_(id_val);
    const char *input      = yyjson_get_str_(input_val);
    bool        do_close   = (close_val && yyjson_is_bool(close_val))
                             && yyjson_get_bool(close_val);
    size_t      input_len  = strlen(input);
    char       *input_copy = talloc_strndup(ctx, input, input_len);
    yyjson_doc_free(doc);
    if (!input_copy) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    bg_process_t *proc = find_process(agent->bg_manager, id);
    if (proc == NULL) {
        return ik_tool_wrap_failure(ctx, "Process not found", "NOT_FOUND");
    }

    res_t res = bg_process_write_stdin(proc, input_copy, input_len, true);
    if (is_err(&res)) {
        char *msg = talloc_asprintf(ctx, "Failed to write stdin: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, msg, "WRITE_FAILED");
    }

    if (do_close) {
        res = bg_process_close_stdin(proc);
        if (is_err(&res)) {
            char *msg = talloc_asprintf(ctx, "Failed to close stdin: %s",
                                        error_message(res.err));
            talloc_free(res.err);
            return ik_tool_wrap_failure(ctx, msg, "CLOSE_FAILED");
        }
    }

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);
    if (!rroot) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);

    yyjson_mut_obj_add_int_(rdoc, rroot, "id", (int64_t)proc->id);
    yyjson_mut_obj_add_str_(rdoc, rroot, "status", status_to_str(proc->status));
    yyjson_mut_obj_add_bool_(rdoc, rroot, "stdin_closed", !proc->stdin_open);

    return ik_tool_wrap_success(ctx, json_from_mut_doc(ctx, rdoc));
}

/* ================================================================
 * pkill handler
 * ================================================================ */

char *ik_bg_pkill_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
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
        return ik_tool_wrap_failure(ctx, "Failed to parse pkill arguments", "PARSE_ERROR"); // LCOV_EXCL_LINE
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *id_val = yyjson_obj_get_(root, "id");
    if (id_val == NULL || !yyjson_is_int(id_val)) {
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: id", "INVALID_ARG");
    }

    int32_t id = (int32_t)yyjson_get_sint_(id_val);
    yyjson_doc_free(doc);

    bg_process_t *proc = find_process(agent->bg_manager, id);
    if (proc == NULL) {
        return ik_tool_wrap_failure(ctx, "Process not found", "NOT_FOUND");
    }

    res_t res = bg_process_kill(proc);
    if (is_err(&res)) {
        char *msg = talloc_asprintf(ctx, "Failed to kill process: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, msg, "KILL_FAILED");
    }

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_val *rroot = yyjson_mut_obj(rdoc);
    if (!rroot) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, rroot);

    yyjson_mut_obj_add_int_(rdoc, rroot, "id", (int64_t)proc->id);
    yyjson_mut_obj_add_str_(rdoc, rroot, "status", status_to_str(proc->status));

    return ik_tool_wrap_success(ctx, json_from_mut_doc(ctx, rdoc));
}

/* ================================================================
 * ps handler
 * ================================================================ */

char *ik_bg_ps_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL); // LCOV_EXCL_BR_LINE
    (void)args_json;

    if (agent->bg_manager == NULL) {
        return ik_tool_wrap_failure(ctx, "Background process support not initialized",
                                    "NOT_INITIALIZED");
    }

    bg_manager_t *mgr = agent->bg_manager;

    yyjson_mut_doc *rdoc = yyjson_mut_doc_new(NULL);
    if (!rdoc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_val *arr = yyjson_mut_arr_(rdoc);
    if (!arr) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(rdoc, arr);

    for (int i = 0; i < mgr->count; i++) {
        bg_process_t *proc = mgr->processes[i];
        int64_t age     = age_seconds(proc);
        int64_t ttl_rem = ttl_remaining(proc, age);
        int64_t lines   = bg_line_index_count(proc->line_index);

        yyjson_mut_val *obj = yyjson_mut_obj_(rdoc);
        if (!obj) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_int_(rdoc, obj, "id",  (int64_t)proc->id);
        yyjson_mut_obj_add_int_(rdoc, obj, "pid", (int64_t)proc->pid);
        yyjson_mut_obj_add_str_(rdoc, obj, "label",   proc->label   ? proc->label   : "");
        yyjson_mut_obj_add_str_(rdoc, obj, "status",  status_to_str(proc->status));
        yyjson_mut_obj_add_int_(rdoc, obj, "age_seconds", age);
        yyjson_mut_obj_add_int_(rdoc, obj, "ttl_remaining_seconds", ttl_rem);
        yyjson_mut_obj_add_int_(rdoc, obj, "total_lines", lines);
        yyjson_mut_obj_add_int_(rdoc, obj, "total_bytes", proc->total_bytes);
        yyjson_mut_obj_add_str_(rdoc, obj, "command", proc->command ? proc->command : "");

        yyjson_mut_arr_add_val_(arr, obj);
    }

    return ik_tool_wrap_success(ctx, json_from_mut_doc(ctx, rdoc));
}

/* ================================================================
 * JSON schemas for background process tools
 * ================================================================ */

static const char *PSTART_SCHEMA =
    "{\"name\":\"pstart\","
    "\"description\":\"Start a long-running process in the background. The process runs in a terminal (PTY) and all output is preserved. You must specify a TTL.\","
    "\"parameters\":{"
    "\"type\":\"object\","
    "\"properties\":{"
    "\"command\":{\"type\":\"string\",\"description\":\"Shell command to run\"},"
    "\"label\":{\"type\":\"string\",\"description\":\"Short human-readable description (e.g. 'unit tests', 'dev server')\"},"
    "\"ttl_seconds\":{\"type\":\"integer\",\"description\":\"Time-to-live in seconds. The process is killed when TTL expires. Use -1 for no limit.\"}"
    "},"
    "\"required\":[\"command\",\"label\",\"ttl_seconds\"]"
    "}}";

static const char *PREAD_SCHEMA =
    "{\"name\":\"pread\","
    "\"description\":\"Check the status and read output of a background process.\","
    "\"parameters\":{"
    "\"type\":\"object\","
    "\"properties\":{"
    "\"id\":{\"type\":\"integer\",\"description\":\"Process ID from pstart\"},"
    "\"mode\":{\"type\":\"string\",\"enum\":[\"tail\",\"since_last\",\"lines\"],"
    "\"description\":\"Output mode. 'tail' = last N lines, 'since_last' = new output since last check, 'lines' = specific range. Default: tail\"},"
    "\"tail_lines\":{\"type\":\"integer\",\"description\":\"Number of lines for tail mode. Default: 50\"},"
    "\"start_line\":{\"type\":\"integer\",\"description\":\"Start line for lines mode (1-indexed)\"},"
    "\"end_line\":{\"type\":\"integer\",\"description\":\"End line for lines mode (1-indexed, inclusive)\"}"
    "},"
    "\"required\":[\"id\"]"
    "}}";

static const char *PWRITE_SCHEMA =
    "{\"name\":\"pwrite\","
    "\"description\":\"Write to a background process's stdin. Appends a newline by default.\","
    "\"parameters\":{"
    "\"type\":\"object\","
    "\"properties\":{"
    "\"id\":{\"type\":\"integer\",\"description\":\"Process ID from pstart\"},"
    "\"input\":{\"type\":\"string\",\"description\":\"Text to send to the process\"},"
    "\"close_stdin\":{\"type\":\"boolean\","
    "\"description\":\"If true, close stdin after sending (signal EOF). Default: false\"}"
    "},"
    "\"required\":[\"id\",\"input\"]"
    "}}";

static const char *PKILL_SCHEMA =
    "{\"name\":\"pkill\","
    "\"description\":\"Terminate a background process. Sends SIGTERM, escalates to SIGKILL after 5 seconds.\","
    "\"parameters\":{"
    "\"type\":\"object\","
    "\"properties\":{"
    "\"id\":{\"type\":\"integer\",\"description\":\"Process ID from pstart\"}"
    "},"
    "\"required\":[\"id\"]"
    "}}";

static const char *PS_SCHEMA =
    "{\"name\":\"ps\","
    "\"description\":\"List all background processes owned by the current agent.\","
    "\"parameters\":{"
    "\"type\":\"object\","
    "\"properties\":{},"
    "\"required\":[]"
    "}}";

/* ================================================================
 * Registration
 * ================================================================ */

void ik_bg_tools_register(ik_tool_registry_t *registry)
{
    assert(registry != NULL); // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (!tmp) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_alc alc = ik_make_talloc_allocator(registry);

    #define PARSE_SCHEMA(var, schema_str)                                               \
        char *var##_buf = talloc_strdup(tmp, (schema_str));                             \
        if (!var##_buf) PANIC("Out of memory");             /* LCOV_EXCL_BR_LINE */     \
        yyjson_doc *var##_doc = yyjson_read_opts(var##_buf,                             \
            strlen(var##_buf), 0, &alc, NULL);                                          \
        if (!var##_doc) PANIC("Failed to parse " #var " schema") /* LCOV_EXCL_BR_LINE */

    PARSE_SCHEMA(pstart,   PSTART_SCHEMA);
    PARSE_SCHEMA(pread,    PREAD_SCHEMA);
    PARSE_SCHEMA(pwrite,   PWRITE_SCHEMA);
    PARSE_SCHEMA(pkill,    PKILL_SCHEMA);
    PARSE_SCHEMA(ps,       PS_SCHEMA);

    #undef PARSE_SCHEMA

    #define REG(name)                                                                   \
        do {                                                                            \
            res_t _r = ik_tool_registry_add_internal(registry, #name,                  \
                            name##_doc, ik_bg_##name##_handler, NULL);                 \
            if (is_err(&_r)) PANIC("Failed to register " #name " tool"); /* LCOV_EXCL_BR_LINE */ \
        } while (0)

    REG(pstart);
    REG(pread);
    REG(pwrite);
    REG(pkill);
    REG(ps);

    #undef REG

    talloc_free(tmp);
}
