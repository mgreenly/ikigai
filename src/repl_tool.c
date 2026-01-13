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
#include "tool_external.h"
#include "tool_registry.h"
#include "tool_wrapper.h"
#include "wrapper.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>
#include "vendor/yyjson/yyjson.h"

// Arguments passed to the worker thread.
// All strings are copied into tool_thread_ctx so the thread owns them.
// The agent pointer is used to write results back - this is safe because
// main thread only reads these fields after seeing tool_thread_complete=true.
typedef struct {
    TALLOC_CTX *ctx;           // Memory context for allocations (owned by main thread)
    const char *tool_name;     // Copied into ctx, safe for thread to use
    const char *arguments;     // Copied into ctx, safe for thread to use
    ik_agent_ctx_t *agent;     // Direct reference to target agent
    ik_tool_registry_t *registry; // Tool registry for lookup
} tool_thread_args_t;

// Worker thread function - runs tool dispatch in background.
//
// Thread safety model:
// - Worker WRITES to agent->tool_thread_result (no mutex - see D1 above)
// - Worker WRITES to agent->tool_thread_complete UNDER MUTEX
// - Main thread only READS result AFTER seeing complete=true
// - The mutex on the flag provides the memory barrier
static void *tool_thread_worker(void *arg)
{
    tool_thread_args_t *args = (tool_thread_args_t *)arg;
    char *result_json = NULL;

    // Check if registry is available
    if (args->registry == NULL) {
        result_json = ik_tool_wrap_failure(args->ctx, "Tool registry not initialized", "registry_unavailable");
        if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    } else {
        // Look up tool in registry
        ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(args->registry, args->tool_name);
        if (entry == NULL) {
            result_json = ik_tool_wrap_failure(args->ctx, "Tool not found in registry", "tool_not_found");
            if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            // Execute external tool
            char *raw_result = NULL;
            res_t res = ik_tool_external_exec(args->ctx, entry->path, args->arguments, &raw_result);

            if (is_err(&res)) {
                // Tool execution failed (timeout, crash, etc.)
                result_json = ik_tool_wrap_failure(args->ctx, res.err->msg, "execution_failed");
                if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            } else {
                // Tool executed successfully - wrap the result
                result_json = ik_tool_wrap_success(args->ctx, raw_result);
                if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    // Store result directly in agent context.
    // Safe without mutex: main thread won't read until complete=true,
    // and setting complete=true (below) provides the memory barrier.
    args->agent->tool_thread_result = result_json;

    // Signal completion under mutex.
    // The mutex ensures main thread sees result before or after this,
    // never during. Combined with the read-after-complete pattern,
    // this guarantees main thread sees the final result value.
    pthread_mutex_lock_(&args->agent->tool_thread_mutex);
    args->agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&args->agent->tool_thread_mutex);

    return NULL;
}

// Helper: Execute pending tool call and add messages to conversation
// Exposed to reduce complexity in handle_request_success
void ik_repl_execute_pending_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);               // LCOV_EXCL_BR_LINE
    assert(repl->current->pending_tool_call != NULL);  // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = repl->current->pending_tool_call;

    // 1. Add tool_call message to conversation (canonical format)
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_message_t *tc_msg = ik_message_create_tool_call(repl->current, tc->id, tc->name, tc->arguments);
    res_t result = ik_agent_add_message(repl->current, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_call is added
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_call");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "summary", summary);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }

    // 2. Execute tool via registry lookup
    char *result_json = NULL;
    if (repl->shared->tool_registry == NULL) {
        result_json = ik_tool_wrap_failure(repl, "Tool registry not initialized", "registry_unavailable");
        if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    } else {
        ik_tool_registry_entry_t *entry = ik_tool_registry_lookup(repl->shared->tool_registry, tc->name);
        if (entry == NULL) {
            result_json = ik_tool_wrap_failure(repl, "Tool not found in registry", "tool_not_found");
            if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            char *raw_result = NULL;
            res_t res = ik_tool_external_exec(repl, entry->path, tc->arguments, &raw_result);
            if (is_err(&res)) {
                result_json = ik_tool_wrap_failure(repl, res.err->msg, "execution_failed");
                if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            } else {
                result_json = ik_tool_wrap_success(repl, raw_result);
                if (result_json == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    // 3. Add tool result message to conversation
    ik_message_t *result_msg = ik_message_create_tool_result(repl->current, tc->id, result_json, false);
    result = ik_agent_add_message(repl->current, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_result is added
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_result");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "result", result_json);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }

    // 4. Display tool call and result in scrollback via event renderer
    const char *formatted_call = ik_format_tool_call(repl, tc);
    ik_event_render(repl->current->scrollback, "tool_call", formatted_call, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->current->scrollback, "tool_result", formatted_result, "{}");

    // 5. Persist to database
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              repl->current->uuid, "tool_call", formatted_call, "{}");
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              repl->current->uuid, "tool_result", formatted_result, "{}");
    }

    // 6. Clear pending tool call
    talloc_free(summary);
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = NULL;
}

// Start async tool execution - spawns thread and returns immediately.
// Called from handle_request_success when LLM returns a tool call.
//
// After this returns:
// - State is EXECUTING_TOOL
// - Thread is running (or we PANICed)
// - Event loop resumes, spinner animates
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);                    // LCOV_EXCL_BR_LINE
    assert(agent->pending_tool_call != NULL); // LCOV_EXCL_BR_LINE
    assert(!agent->tool_thread_running);      // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = agent->pending_tool_call;

    // Create memory context for thread allocations.
    // Owned by main thread (child of agent), freed after completion.
    // Thread allocates into this but doesn't free it.
    agent->tool_thread_ctx = talloc_new(agent);
    if (agent->tool_thread_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Create thread arguments in the thread context.
    // Copy strings so thread has its own copies (original may be freed).
    tool_thread_args_t *args = talloc(agent->tool_thread_ctx, tool_thread_args_t);
    if (args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    args->ctx = agent->tool_thread_ctx;
    args->tool_name = talloc_strdup(agent->tool_thread_ctx, tc->name);
    args->arguments = talloc_strdup(agent->tool_thread_ctx, tc->arguments);
    args->agent = agent;
    args->registry = agent->shared->tool_registry;

    if (args->tool_name == NULL || args->arguments == NULL) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Set flags BEFORE spawning thread to avoid race condition.
    // If thread runs faster than main thread, we must have flags set first.
    // If spawn fails, we reset flags and PANIC.
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_complete = false;
    agent->tool_thread_running = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // Spawn thread - if this fails, reset flags and PANIC.
    int ret = pthread_create_(&agent->tool_thread, NULL, tool_thread_worker, args);
    if (ret != 0) { // LCOV_EXCL_BR_LINE
        // Thread creation failure is rare (resource exhaustion).
        // Reset flags before PANIC to maintain consistency.
        pthread_mutex_lock_(&agent->tool_thread_mutex); // LCOV_EXCL_LINE
        agent->tool_thread_running = false; // LCOV_EXCL_LINE
        pthread_mutex_unlock_(&agent->tool_thread_mutex); // LCOV_EXCL_LINE
        PANIC("Failed to create tool thread"); // LCOV_EXCL_LINE
    }

    // Transition to EXECUTING_TOOL state.
    // Spinner stays visible, input stays hidden.
    ik_agent_transition_to_executing_tool(agent);
}

// Wrapper for backward compatibility - calls new agent-based function
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl)
{
    ik_agent_start_tool_execution(repl->current);
}

// Complete async tool execution - harvest result after thread finishes.
// Called from event loop when tool_thread_complete is true.
//
// Preconditions:
// - tool_thread_running == true
// - tool_thread_complete == true (thread is done)
//
// After this returns:
// - Messages added to conversation
// - Scrollback updated
// - Thread context freed
// - State back to WAITING_FOR_LLM
void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);                  // LCOV_EXCL_BR_LINE
    assert(agent->tool_thread_running);     // LCOV_EXCL_BR_LINE
    assert(agent->tool_thread_complete);    // LCOV_EXCL_BR_LINE

    // Join thread - it's already done, so this returns immediately.
    // We still call join to clean up thread resources.
    pthread_join_(agent->tool_thread, NULL);

    ik_tool_call_t *tc = agent->pending_tool_call;

    // Steal result from thread context before freeing it.
    // talloc_steal moves ownership to agent so it survives context free.
    char *result_json = talloc_steal(agent, agent->tool_thread_result);

    // 1. Add tool_call message to conversation
    char *summary = talloc_asprintf(agent, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_message_t *tc_msg = ik_message_create_tool_call_with_thinking(
        agent,
        agent->pending_thinking_text,
        agent->pending_thinking_signature,
        agent->pending_redacted_data,
        tc->id, tc->name, tc->arguments);

    // 2. Format for display (needed before clearing thinking for database)
    const char *formatted_call = ik_format_tool_call(agent, tc);
    const char *formatted_result = ik_format_tool_result(agent, tc->name, result_json);

    // 3. Persist to database (before clearing thinking fields)
    if (agent->shared->db_ctx != NULL && agent->shared->session_id > 0) {
        // Build data_json with tool call details and thinking (inline - no static function)
        yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
        if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *root = yyjson_mut_obj(doc);
        if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_set_root(doc, root);

        // Tool call fields
        yyjson_mut_obj_add_str(doc, root, "tool_call_id", tc->id);  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(doc, root, "tool_name", tc->name);  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(doc, root, "tool_args", tc->arguments);  // LCOV_EXCL_BR_LINE

        // Thinking block (if present)
        if (agent->pending_thinking_text != NULL) {
            yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
            if (thinking_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_str(doc, thinking_obj, "text", agent->pending_thinking_text);  // LCOV_EXCL_BR_LINE
            if (agent->pending_thinking_signature != NULL) {
                yyjson_mut_obj_add_str(doc, thinking_obj, "signature", agent->pending_thinking_signature);  // LCOV_EXCL_BR_LINE
            }
            yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj);
        }

        // Redacted thinking (if present)
        if (agent->pending_redacted_data != NULL) {
            yyjson_mut_val *redacted_obj = yyjson_mut_obj(doc);
            if (redacted_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_str(doc, redacted_obj, "data", agent->pending_redacted_data);  // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_val(doc, root, "redacted_thinking", redacted_obj);
        }

        char *json = yyjson_mut_write(doc, 0, NULL);
        char *data_json = talloc_strdup(agent, json);
        free(json);
        yyjson_mut_doc_free(doc);
        if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                              agent->uuid, "tool_call", formatted_call, data_json);
        ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                              agent->uuid, "tool_result", formatted_result, "{}");
        talloc_free(data_json);
    }

    // Clear pending thinking after use
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

    // Debug output when tool_call is added
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_call");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "summary", summary);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }

    // 4. Add tool result message to conversation
    ik_message_t *result_msg = ik_message_create_tool_result(agent, tc->id, result_json, false);
    result = ik_agent_add_message(agent, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_result is added
    {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "tool_result");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "result", result_json);  // LCOV_EXCL_LINE
        ik_log_debug_json(log_doc);  // LCOV_EXCL_LINE
    }

    // 5. Display in scrollback via event renderer
    ik_event_render(agent->scrollback, "tool_call", formatted_call, "{}");
    ik_event_render(agent->scrollback, "tool_result", formatted_result, "{}");

    // 6. Clean up
    talloc_free(summary);
    talloc_free(agent->pending_tool_call);
    agent->pending_tool_call = NULL;

    // Free thread context (includes args struct and copied strings).
    // result_json was stolen out, so it survives.
    talloc_free(agent->tool_thread_ctx);
    agent->tool_thread_ctx = NULL;

    // Reset thread state for next tool call
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_result = NULL;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // Transition back to WAITING_FOR_LLM.
    // Caller will check if tool loop should continue.
    ik_agent_transition_from_executing_tool(agent);
}

// Wrapper for backward compatibility - calls new agent-based function
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl)
{
    ik_agent_complete_tool_execution(repl->current);
}
