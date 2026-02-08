/**
 * @file internal_tool_wait.c
 * @brief Wait tool handler implementation
 */

#include "apps/ikigai/internal_tool_wait.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_wait_core.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"

// Wait handler: wait for messages from other agents
char *ik_wait_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    ik_db_ctx_t *worker_db_ctx = agent->worker_db_ctx;

    // Parse arguments
    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse wait arguments", "PARSE_ERROR");
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *timeout_val = yyjson_obj_get_(root, "timeout");
    if (timeout_val == NULL || !yyjson_is_num(timeout_val)) {  // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: timeout", "INVALID_ARG");
    }

    int32_t timeout_sec = (int32_t)yyjson_get_num(timeout_val);

    // Check for from_agents (fan-in mode)
    yyjson_val *from_agents_val = yyjson_obj_get_(root, "from_agents");
    bool is_fanin = (from_agents_val != NULL && yyjson_is_arr(from_agents_val));

    ik_wait_result_t wait_result = {0};

    if (is_fanin) {
        // Fan-in mode
        size_t target_count = yyjson_arr_size(from_agents_val);
        char **target_uuids = talloc_zero_array(ctx, char *, (unsigned int)target_count);
        if (target_uuids == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        for (size_t i = 0; i < target_count; i++) {
            yyjson_val *uuid_val = yyjson_arr_get(from_agents_val, i);
            if (!yyjson_is_str(uuid_val)) {  // LCOV_EXCL_BR_LINE
                yyjson_doc_free(doc);
                return ik_tool_wrap_failure(ctx, "from_agents must contain strings", "INVALID_ARG");
            }
            target_uuids[i] = talloc_strdup(ctx, yyjson_get_str_(uuid_val));
            if (target_uuids[i] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        yyjson_doc_free(doc);

        wait_result.is_fanin = true;
        ik_wait_core_fanin(ctx, worker_db_ctx, agent->shared->session_id,
                           agent->uuid, timeout_sec, target_uuids, target_count,
                           &agent->interrupt_requested, &wait_result);
    } else {
        // Next message mode
        yyjson_doc_free(doc);
        ik_wait_core_next_message(ctx, worker_db_ctx, agent->shared->session_id,
                                   agent->uuid, timeout_sec,
                                   &agent->interrupt_requested, &wait_result);
    }

    // Build result JSON
    yyjson_mut_doc *result_doc = yyjson_mut_doc_new(NULL);
    if (result_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *result_root = yyjson_mut_obj(result_doc);
    if (result_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(result_doc, result_root);

    if (wait_result.is_fanin) {
        // Fan-in results
        yyjson_mut_val *results_arr = yyjson_mut_arr(result_doc);
        if (results_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        for (size_t i = 0; i < wait_result.entry_count; i++) {
            yyjson_mut_val *entry = yyjson_mut_obj(result_doc);
            if (entry == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            yyjson_mut_obj_add_str_(result_doc, entry, "agent_uuid", wait_result.entries[i].agent_uuid);
            yyjson_mut_obj_add_str_(result_doc, entry, "agent_name", wait_result.entries[i].agent_name);
            yyjson_mut_obj_add_str_(result_doc, entry, "status", wait_result.entries[i].status);
            if (wait_result.entries[i].message != NULL) {
                yyjson_mut_obj_add_str_(result_doc, entry, "message", wait_result.entries[i].message);
            }

            yyjson_mut_arr_append(results_arr, entry);
        }

        yyjson_mut_obj_add_val(result_doc, result_root, "results", results_arr);
    } else {
        // Next message mode
        if (wait_result.from_uuid != NULL) {
            yyjson_mut_obj_add_str_(result_doc, result_root, "from", wait_result.from_uuid);
            yyjson_mut_obj_add_str_(result_doc, result_root, "message", wait_result.message);
        } else {
            yyjson_mut_obj_add_str_(result_doc, result_root, "status", "timeout");
        }
    }

    char *result_json = yyjson_mut_write(result_doc, 0, NULL);
    if (result_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *result_copy = talloc_strdup(ctx, result_json);
    free(result_json);
    yyjson_mut_doc_free(result_doc);

    return ik_tool_wrap_success(ctx, result_copy);
}
