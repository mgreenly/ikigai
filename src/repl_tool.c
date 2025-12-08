// REPL tool execution helper
#include "repl.h"

#include "db/message.h"
#include "event_render.h"
#include "format.h"
#include "panic.h"
#include "shared.h"
#include "tool.h"
#include "wrapper.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>

// Arguments passed to the worker thread.
// All strings are copied into tool_thread_ctx so the thread owns them.
// The repl pointer is used to write results back - this is safe because
// main thread only reads these fields after seeing tool_thread_complete=true.
typedef struct {
    TALLOC_CTX *ctx;           // Memory context for allocations (owned by main thread)
    const char *tool_name;     // Copied into ctx, safe for thread to use
    const char *arguments;     // Copied into ctx, safe for thread to use
    ik_repl_ctx_t *repl;       // For writing result and signaling completion
} tool_thread_args_t;

// Worker thread function - runs tool dispatch in background.
//
// Thread safety model:
// - Worker WRITES to repl->tool_thread_result (no mutex - see D1 above)
// - Worker WRITES to repl->tool_thread_complete UNDER MUTEX
// - Main thread only READS result AFTER seeing complete=true
// - The mutex on the flag provides the memory barrier
static void *tool_thread_worker(void *arg)
{
    tool_thread_args_t *args = (tool_thread_args_t *)arg;

    // Execute tool - this is the potentially slow operation.
    // All allocations go into args->ctx which main thread will free.
    res_t result = ik_tool_dispatch(args->ctx, args->tool_name, args->arguments);

    // Store result directly in repl context.
    // Safe without mutex: main thread won't read until complete=true,
    // and setting complete=true (below) provides the memory barrier.
    args->repl->tool_thread_result = result.ok;

    // Signal completion under mutex.
    // The mutex ensures main thread sees result before or after this,
    // never during. Combined with the read-after-complete pattern,
    // this guarantees main thread sees the final result value.
    pthread_mutex_lock_(&args->repl->tool_thread_mutex);
    args->repl->tool_thread_complete = true;
    pthread_mutex_unlock_(&args->repl->tool_thread_mutex);

    return NULL;
}

// Helper: Execute pending tool call and add messages to conversation
// Exposed to reduce complexity in handle_request_success
void ik_repl_execute_pending_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);               // LCOV_EXCL_BR_LINE
    assert(repl->pending_tool_call != NULL);  // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = repl->pending_tool_call;

    // 1. Add tool_call message to conversation (canonical format)
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_msg_t *tc_msg = ik_openai_msg_create_tool_call(
        repl->conversation, tc->id, "function", tc->name, tc->arguments, summary);
    res_t result = ik_openai_conversation_add_msg(repl->conversation, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_call is added
    if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                "<< TOOL_CALL: %s\n",
                summary);
        fflush(repl->shared->openai_debug_pipe->write_end);
    }

    // 2. Execute tool
    res_t tool_res = ik_tool_dispatch(repl, tc->name, tc->arguments);
    if (is_err(&tool_res)) PANIC("tool dispatch failed"); // LCOV_EXCL_BR_LINE
    char *result_json = tool_res.ok;

    // 3. Add tool result message to conversation
    ik_msg_t *result_msg = ik_openai_msg_create_tool_result(
        repl->conversation, tc->id, result_json);
    result = ik_openai_conversation_add_msg(repl->conversation, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_result is added
    if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                "<< TOOL_RESULT: %s\n",
                result_json);
        fflush(repl->shared->openai_debug_pipe->write_end);
    }

    // 4. Display tool call and result in scrollback via event renderer
    const char *formatted_call = ik_format_tool_call(repl, tc);
    ik_event_render(repl->scrollback, "tool_call", formatted_call, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->scrollback, "tool_result", formatted_result, "{}");

    // 5. Persist to database
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              "tool_call", formatted_call, tc_msg->data_json);
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              "tool_result", formatted_result, result_msg->data_json);
    }

    // 6. Clear pending tool call
    talloc_free(summary);
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = NULL;
}

// Start async tool execution - spawns thread and returns immediately.
// Called from handle_request_success when LLM returns a tool call.
//
// After this returns:
// - State is EXECUTING_TOOL
// - Thread is running (or we PANICed)
// - Event loop resumes, spinner animates
void ik_repl_start_tool_execution(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);                    // LCOV_EXCL_BR_LINE
    assert(repl->pending_tool_call != NULL); // LCOV_EXCL_BR_LINE
    assert(!repl->tool_thread_running);      // LCOV_EXCL_BR_LINE

    ik_tool_call_t *tc = repl->pending_tool_call;

    // Create memory context for thread allocations.
    // Owned by main thread (child of repl), freed after completion.
    // Thread allocates into this but doesn't free it.
    repl->tool_thread_ctx = talloc_new(repl);
    if (repl->tool_thread_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Create thread arguments in the thread context.
    // Copy strings so thread has its own copies (original may be freed).
    tool_thread_args_t *args = talloc(repl->tool_thread_ctx, tool_thread_args_t);
    if (args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    args->ctx = repl->tool_thread_ctx;
    args->tool_name = talloc_strdup(repl->tool_thread_ctx, tc->name);
    args->arguments = talloc_strdup(repl->tool_thread_ctx, tc->arguments);
    args->repl = repl;

    if (args->tool_name == NULL || args->arguments == NULL) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Set flags BEFORE spawning thread to avoid race condition.
    // If thread runs faster than main thread, we must have flags set first.
    // If spawn fails, we reset flags and PANIC.
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    repl->tool_thread_complete = false;
    repl->tool_thread_running = true;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Spawn thread - if this fails, reset flags and PANIC.
    int ret = pthread_create_(&repl->tool_thread, NULL, tool_thread_worker, args);
    if (ret != 0) { // LCOV_EXCL_BR_LINE
        // Thread creation failure is rare (resource exhaustion).
        // Reset flags before PANIC to maintain consistency.
        pthread_mutex_lock_(&repl->tool_thread_mutex); // LCOV_EXCL_LINE
        repl->tool_thread_running = false; // LCOV_EXCL_LINE
        pthread_mutex_unlock_(&repl->tool_thread_mutex); // LCOV_EXCL_LINE
        PANIC("Failed to create tool thread"); // LCOV_EXCL_LINE
    }

    // Transition to EXECUTING_TOOL state.
    // Spinner stays visible, input stays hidden.
    ik_repl_transition_to_executing_tool(repl);
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
void ik_repl_complete_tool_execution(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);                  // LCOV_EXCL_BR_LINE
    assert(repl->tool_thread_running);     // LCOV_EXCL_BR_LINE
    assert(repl->tool_thread_complete);    // LCOV_EXCL_BR_LINE

    // Join thread - it's already done, so this returns immediately.
    // We still call join to clean up thread resources.
    pthread_join_(repl->tool_thread, NULL);

    ik_tool_call_t *tc = repl->pending_tool_call;

    // Steal result from thread context before freeing it.
    // talloc_steal moves ownership to repl so it survives context free.
    char *result_json = talloc_steal(repl, repl->tool_thread_result);

    // 1. Add tool_call message to conversation
    char *summary = talloc_asprintf(repl, "%s(%s)", tc->name, tc->arguments);
    if (summary == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_msg_t *tc_msg = ik_openai_msg_create_tool_call(
        repl->conversation, tc->id, "function", tc->name, tc->arguments, summary);
    res_t result = ik_openai_conversation_add_msg(repl->conversation, tc_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_call is added
    if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                "<< TOOL_CALL: %s\n",
                summary);
        fflush(repl->shared->openai_debug_pipe->write_end);
    }

    // 2. Add tool result message to conversation
    ik_msg_t *result_msg = ik_openai_msg_create_tool_result(
        repl->conversation, tc->id, result_json);
    result = ik_openai_conversation_add_msg(repl->conversation, result_msg);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Debug output when tool_result is added
    if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                "<< TOOL_RESULT: %s\n",
                result_json);
        fflush(repl->shared->openai_debug_pipe->write_end);
    }

    // 3. Display in scrollback via event renderer
    const char *formatted_call = ik_format_tool_call(repl, tc);
    ik_event_render(repl->scrollback, "tool_call", formatted_call, "{}");
    const char *formatted_result = ik_format_tool_result(repl, tc->name, result_json);
    ik_event_render(repl->scrollback, "tool_result", formatted_result, "{}");

    // 4. Persist to database
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              "tool_call", formatted_call, tc_msg->data_json);
        ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                              "tool_result", formatted_result, result_msg->data_json);
    }

    // 5. Clean up
    talloc_free(summary);
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = NULL;

    // Free thread context (includes args struct and copied strings).
    // result_json was stolen out, so it survives.
    talloc_free(repl->tool_thread_ctx);
    repl->tool_thread_ctx = NULL;

    // Reset thread state for next tool call
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;
    repl->tool_thread_result = NULL;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Transition back to WAITING_FOR_LLM.
    // Caller will check if tool loop should continue.
    ik_repl_transition_from_executing_tool(repl);
}
