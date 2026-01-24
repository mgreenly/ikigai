// REPL tool execution helper
#include "repl.h"

#include "agent.h"
#include "db/message.h"
#include "event_render.h"
#include "format.h"
#include "logger.h"
#include "message.h"
#include "panic.h"
#include "shared.h"
#include "tool.h"
#include "tool_executor.h"
#include "tool_registry.h"
#include "wrapper.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Worker thread arguments - strings owned by ctx, safe for thread use.
typedef struct {
    TALLOC_CTX *ctx;
    const char *tool_name;
    const char *arguments;
    ik_agent_ctx_t *agent;
    ik_tool_registry_t *registry;
    ik_paths_t *paths;
} tool_thread_args_t;

// Worker thread - executes tool and signals completion via mutex.
static void *tool_thread_worker(void *arg)
{
    tool_thread_args_t *args = (tool_thread_args_t *)arg;

    char *result_json = ik_tool_execute_from_registry(args->ctx, args->registry, args->paths,
                                                      args->agent->uuid, args->tool_name, args->arguments);
    args->agent->tool_thread_result = result_json;
    pthread_mutex_lock_(&args->agent->tool_thread_mutex);
    args->agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&args->agent->tool_thread_mutex);

    return NULL;
}

// Build tool_call data_json for database with thinking/redacted blocks.
static char *ik_build_tool_call_data_json(TALLOC_CTX *ctx,
                                          const ik_tool_call_t *tc,
                                          const char *thinking_text,
                                          const char *thinking_signature,
                                          const char *redacted_data)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);
    if (!yyjson_mut_obj_add_str(doc, root, "tool_call_id", tc->id)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "tool_name", tc->name)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "tool_args", tc->arguments)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (thinking_text != NULL) {
        yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
        if (thinking_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(doc, thinking_obj, "text", thinking_text)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (thinking_signature != NULL) {
            if (!yyjson_mut_obj_add_str(doc, thinking_obj, "signature", thinking_signature)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
        if (!yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    if (redacted_data != NULL) {
        yyjson_mut_val *redacted_obj = yyjson_mut_obj(doc);
        if (redacted_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_str(doc, redacted_obj, "data", redacted_data)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (!yyjson_mut_obj_add_val(doc, root, "redacted_thinking", redacted_obj)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    char *json = yyjson_mut_write(doc, 0, NULL);
    char *data_json = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return data_json;
}

// Build tool_result data_json for database.
static char *ik_build_tool_result_data_json(TALLOC_CTX *ctx,
                                            const char *tool_call_id,
                                            const char *tool_name,
                                            const char *result_json)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);
    if (!yyjson_mut_obj_add_str(doc, root, "tool_call_id", tool_call_id)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "name", tool_name)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    if (!yyjson_mut_obj_add_str(doc, root, "output", result_json)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    bool success = false;
    yyjson_doc *result_doc = yyjson_read(result_json, strlen(result_json), 0);
    if (result_doc != NULL) {
        yyjson_val *result_root = yyjson_doc_get_root_(result_doc);
        yyjson_val *success_val = yyjson_obj_get_(result_root, "tool_success");
        if (success_val != NULL) {
            success = yyjson_get_bool(success_val);
        }
        yyjson_doc_free(result_doc);
    }

    if (!yyjson_mut_obj_add_bool(doc, root, "success", success)) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *json = yyjson_mut_write(doc, 0, NULL);
    char *data_json = talloc_strdup(ctx, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return data_json;
}

// Execute pending tool call and add messages to conversation (synchronous).
void ik_repl_execute_pending_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);               // LCOV_EXCL_BR_LINE
    assert(repl->current->pending_tool_call != NULL);  // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = repl->current->pending_tool_call;
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_message_t *tc_msg = ik_message_create_tool_call(repl->current, tc->id, tc->name, tc->arguments);
    res_t result = ik_agent_add_message(repl->current, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_call");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "summary", summary);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }
    char *result_json = ik_tool_execute_from_registry(repl, repl->shared->tool_registry, repl->shared->paths,
                                                      repl->current->uuid, tc->name, tc->arguments);
    ik_message_t *result_msg = ik_message_create_tool_result(repl->current, tc->id, result_json, false);
    result = ik_agent_add_message(repl->current, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_result");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "result", result_json);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }
    const char *formatted_call = ik_format_tool_call(repl, tc);
    ik_event_render(repl->current->scrollback, "tool_call", formatted_call, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->current->scrollback, "tool_result", formatted_result, "{}");
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        char *tool_call_data_json = ik_build_tool_call_data_json(repl, tc, NULL, NULL, NULL);
        char *tool_result_data_json = ik_build_tool_result_data_json(repl, tc->id, tc->name, result_json);
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              repl->current->uuid, "tool_call", formatted_call, tool_call_data_json);
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              repl->current->uuid, "tool_result", formatted_result, tool_result_data_json);
        talloc_free(tool_call_data_json);
        talloc_free(tool_result_data_json);
    }
    talloc_free(summary);
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = NULL;
}

// Start async tool execution - spawns thread, returns immediately.
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);                    // LCOV_EXCL_BR_LINE
    assert(agent->pending_tool_call != NULL); // LCOV_EXCL_BR_LINE
    assert(!agent->tool_thread_running);      // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = agent->pending_tool_call;
    agent->tool_thread_ctx = talloc_new(agent);
    if (agent->tool_thread_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    tool_thread_args_t *args = talloc(agent->tool_thread_ctx, tool_thread_args_t);
    if (args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    args->ctx = agent->tool_thread_ctx;
    args->tool_name = talloc_strdup(agent->tool_thread_ctx, tc->name);
    args->arguments = talloc_strdup(agent->tool_thread_ctx, tc->arguments);
    args->agent = agent;
    args->registry = agent->shared->tool_registry;
    args->paths = agent->shared->paths;
    if (args->tool_name == NULL || args->arguments == NULL) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_complete = false;
    agent->tool_thread_running = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    int ret = pthread_create_(&agent->tool_thread, NULL, tool_thread_worker, args);
    if (ret != 0) { // LCOV_EXCL_BR_LINE
        pthread_mutex_lock_(&agent->tool_thread_mutex); // LCOV_EXCL_LINE
        agent->tool_thread_running = false; // LCOV_EXCL_LINE
        pthread_mutex_unlock_(&agent->tool_thread_mutex); // LCOV_EXCL_LINE
        PANIC("Failed to create tool thread"); // LCOV_EXCL_LINE
    }
    ik_agent_transition_to_executing_tool(agent);
}

void ik_repl_start_tool_execution(ik_repl_ctx_t *repl)
{
    ik_agent_start_tool_execution(repl->current);
}

// Complete async tool execution - harvest result and update state.
void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);                  // LCOV_EXCL_BR_LINE
    assert(agent->tool_thread_running);     // LCOV_EXCL_BR_LINE
    assert(agent->tool_thread_complete);    // LCOV_EXCL_BR_LINE

    pthread_join_(agent->tool_thread, NULL);
    ik_tool_call_t *tc = agent->pending_tool_call;
    char *result_json = talloc_steal(agent, agent->tool_thread_result);
    char *summary = talloc_asprintf(agent, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_message_t *tc_msg = ik_message_create_tool_call_with_thinking(agent, agent->pending_thinking_text,
                                                                     agent->pending_thinking_signature,
                                                                     agent->pending_redacted_data,
                                                                     tc->id, tc->name, tc->arguments);
    const char *formatted_call = ik_format_tool_call(agent, tc);
    const char *formatted_result = ik_format_tool_result(agent, tc->name, result_json);
    if (agent->shared->db_ctx != NULL && agent->shared->session_id > 0) {
        char *tool_call_data_json = ik_build_tool_call_data_json(agent, tc, agent->pending_thinking_text,
                                                                 agent->pending_thinking_signature,
                                                                 agent->pending_redacted_data);
        char *tool_result_data_json = ik_build_tool_result_data_json(agent, tc->id, tc->name, result_json);
        ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                              agent->uuid, "tool_call", formatted_call, tool_call_data_json);
        ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                              agent->uuid, "tool_result", formatted_result, tool_result_data_json);
        talloc_free(tool_call_data_json);
        talloc_free(tool_result_data_json);
    }
    if (agent->pending_thinking_text != NULL) {
        talloc_free(agent->pending_thinking_text);
        agent->pending_thinking_text = NULL;
    }
    if (agent->pending_thinking_signature != NULL) {
        talloc_free(agent->pending_thinking_signature);
        agent->pending_thinking_signature = NULL;
    }
    if (agent->pending_redacted_data != NULL) {
        talloc_free(agent->pending_redacted_data);
        agent->pending_redacted_data = NULL;
    }
    res_t result = ik_agent_add_message(agent, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_call");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "summary", summary);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }
    ik_message_t *result_msg = ik_message_create_tool_result(agent, tc->id, result_json, false);
    result = ik_agent_add_message(agent, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_result");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "result", result_json);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }
    ik_event_render(agent->scrollback, "tool_call", formatted_call, "{}");
    ik_event_render(agent->scrollback, "tool_result", formatted_result, "{}");
    talloc_free(summary);
    talloc_free(agent->pending_tool_call);
    agent->pending_tool_call = NULL;
    talloc_free(agent->tool_thread_ctx);
    agent->tool_thread_ctx = NULL;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_result = NULL;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    ik_agent_transition_from_executing_tool(agent);
}

void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl)
{
    ik_agent_complete_tool_execution(repl->current);
}
