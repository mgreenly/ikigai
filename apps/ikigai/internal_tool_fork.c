/**
 * @file internal_tool_fork.c
 * @brief Fork tool handler implementation
 */

#include "apps/ikigai/internal_tool_fork.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool_wrapper.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"

// Fork handler: create child agent with prompt
char *ik_fork_handler(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, const char *args_json)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(args_json != NULL);  // LCOV_EXCL_BR_LINE

    ik_db_ctx_t *worker_db_ctx = agent->worker_db_ctx;

    // Parse arguments
    yyjson_doc *doc = yyjson_read_(args_json, strlen(args_json), 0);
    if (doc == NULL) {  // LCOV_EXCL_BR_LINE
        return ik_tool_wrap_failure(ctx, "Failed to parse fork arguments", "PARSE_ERROR");  // LCOV_EXCL_LINE
    }
    yyjson_val *root = yyjson_doc_get_root_(doc);

    yyjson_val *name_val = yyjson_obj_get_(root, "name");
    yyjson_val *prompt_val = yyjson_obj_get_(root, "prompt");

    if (name_val == NULL || !yyjson_is_str(name_val)) {  // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: name", "INVALID_ARG");
    }

    if (prompt_val == NULL || !yyjson_is_str(prompt_val)) {  // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return ik_tool_wrap_failure(ctx, "Missing required parameter: prompt", "INVALID_ARG");
    }

    char *child_name = talloc_strdup(ctx, yyjson_get_str_(name_val));
    char *prompt = talloc_strdup(ctx, yyjson_get_str_(prompt_val));
    yyjson_doc_free(doc);
    if (child_name == NULL || prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Create child agent using agent's shared context
    // Allocate on tool_thread_ctx since child will be stolen to proper parent in on_complete
    ik_agent_ctx_t *child = NULL;
    res_t res = ik_agent_create(agent->tool_thread_ctx, agent->shared, agent->uuid, &child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *err_msg = talloc_asprintf(ctx, "Failed to create child agent: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, err_msg, "AGENT_CREATE_FAILED");
    }

    // Set child name
    child->name = talloc_strdup(child, child_name);
    if (child->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Set fork_message_id (history inheritance point)
    int64_t fork_message_id = 0;
    res = ik_db_agent_get_last_message_id(worker_db_ctx, agent->uuid, &fork_message_id);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *err_msg = talloc_asprintf(ctx, "Failed to get fork message ID: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, err_msg, "DB_ERROR");
    }
    child->fork_message_id = fork_message_id;

    // Inherit parent configuration
    child->provider = talloc_strdup(child, agent->provider);
    if (child->provider == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    child->model = talloc_strdup(child, agent->model);
    if (child->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    child->thinking_level = agent->thinking_level;

    // Internal fork tool: child starts fresh with just the prompt.
    // Unlike /fork command (interactive), LLM-driven fork is delegation —
    // the child doesn't need the parent's conversation or scrollback.
    // (Copying would also break thinking: parent's text-only assistant
    // message lacks thinking blocks that the API requires.)

    // Insert child into database
    res = ik_db_agent_insert(worker_db_ctx, child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        char *err_msg = talloc_asprintf(ctx, "Failed to insert child agent: %s", error_message(res.err));
        talloc_free(res.err);
        return ik_tool_wrap_failure(ctx, err_msg, "DB_ERROR");
    }

    // Insert clear event so session restore won't walk parent history
    res = ik_db_message_insert(worker_db_ctx, agent->shared->session_id,
                               child->uuid, "clear", NULL, NULL);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        talloc_free(res.err);  // LCOV_EXCL_LINE — best effort
    }

    // Store child pointer in tool_deferred_data for on_complete
    agent->tool_deferred_data = child;

    // Set pending_prompt on child so main loop will start LLM request
    child->pending_prompt = talloc_strdup(child, prompt);
    if (child->pending_prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build success result with child UUID
    yyjson_mut_doc *result_doc = yyjson_mut_doc_new(NULL);
    if (result_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *result_root = yyjson_mut_obj(result_doc);
    if (result_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(result_doc, result_root);

    yyjson_mut_obj_add_str_(result_doc, result_root, "child_uuid", child->uuid);
    yyjson_mut_obj_add_str_(result_doc, result_root, "child_name", child_name);

    char *result_json = yyjson_mut_write(result_doc, 0, NULL);
    if (result_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    char *result_copy = talloc_strdup(ctx, result_json);
    free(result_json);
    yyjson_mut_doc_free(result_doc);

    return ik_tool_wrap_success(ctx, result_copy);
}

// Fork on_complete: steal child to repl, add to agents array
void ik_fork_on_complete(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (agent->tool_deferred_data == NULL) {  // LCOV_EXCL_BR_LINE
        return;
    }

    ik_agent_ctx_t *child = (ik_agent_ctx_t *)agent->tool_deferred_data;

    // Steal child from tool_thread_ctx to repl
    talloc_steal(repl, child);

    // Set repl backpointer
    child->repl = repl;

    // Add to agents array
    res_t res = ik_repl_add_agent(repl, child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        // Can't do much here - log would be ideal but we don't have logger access
        // Child is orphaned but still in DB
        talloc_free(res.err);
    }

    // Clear deferred data
    agent->tool_deferred_data = NULL;
}
