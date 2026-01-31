/**
 * @file commands_fork.c
 * @brief Fork command handler implementation
 */

#include "commands.h"
#include "commands_basic.h"
#include "commands_fork_args.h"
#include "commands_fork_helpers.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/message.h"
#include "event_render.h"
#include "logger.h"
#include "message.h"
#include "panic.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "repl.h"
#include "repl_callbacks.h"
#include "repl_event_handlers.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "shared.h"
#include "wrapper.h"
#include "wrapper_internal.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>
#include <time.h>
#include <unistd.h>


#include "poison.h"
// Helper: Copy parent's pinned_paths to child
static void copy_pinned_paths(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    if (parent->pinned_count == 0) {
        return;
    }

    child->pinned_paths = talloc_zero_array(child, char *, (unsigned int)parent->pinned_count);
    if (child->pinned_paths == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < parent->pinned_count; i++) {
        child->pinned_paths[i] = talloc_strdup(child, parent->pinned_paths[i]);
        if (child->pinned_paths[i] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    child->pinned_count = parent->pinned_count;
}

// Helper: Copy parent's toolset_filter to child
static void copy_toolset_filter(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    if (parent->toolset_count == 0) {
        return;
    }

    child->toolset_filter = talloc_zero_array(child, char *, (unsigned int)parent->toolset_count);
    if (child->toolset_filter == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < parent->toolset_count; i++) {
        child->toolset_filter[i] = talloc_strdup(child, parent->toolset_filter[i]);
        if (child->toolset_filter[i] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
    child->toolset_count = parent->toolset_count;
}

// Handle prompt-triggered LLM call after fork
static void handle_fork_prompt(void *ctx, ik_repl_ctx_t *repl, const char *prompt)
{
    // Create user message
    ik_message_t *user_msg = ik_message_create_text(repl->current, IK_ROLE_USER, prompt);

    // Add to conversation
    res_t res = ik_agent_add_message(repl->current, user_msg);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return;  // Error already logged     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Persist user message to database
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {     // LCOV_EXCL_BR_LINE
        char *data_json = talloc_asprintf(ctx,     // LCOV_EXCL_LINE
                                          "{\"model\":\"%s\",\"temperature\":%.2f,\"max_completion_tokens\":%d}",     // LCOV_EXCL_LINE
                                          repl->shared->cfg->openai_model,     // LCOV_EXCL_LINE
                                          repl->shared->cfg->openai_temperature,     // LCOV_EXCL_LINE
                                          repl->shared->cfg->openai_max_completion_tokens);     // LCOV_EXCL_LINE
        if (data_json == NULL) {  // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,     // LCOV_EXCL_LINE
                                            repl->current->uuid, "user", prompt, data_json);     // LCOV_EXCL_LINE
        if (is_err(&db_res)) {     // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
            yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
            yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_warning");     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "operation", "fork_prompt_persist");     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));     // LCOV_EXCL_LINE
            ik_logger_warn_json(repl->shared->logger, log_doc);     // LCOV_EXCL_LINE
            talloc_free(db_res.err);     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE
        talloc_free(data_json);     // LCOV_EXCL_LINE
    }

    // Render user message to scrollback
    res = ik_event_render(repl->current->scrollback, "user", prompt, "{}");
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return;  // Error already logged     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Clear previous assistant response
    if (repl->current->assistant_response != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(repl->current->assistant_response);     // LCOV_EXCL_LINE
        repl->current->assistant_response = NULL;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    if (repl->current->streaming_line_buffer != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(repl->current->streaming_line_buffer);     // LCOV_EXCL_LINE
        repl->current->streaming_line_buffer = NULL;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Reset tool iteration count
    repl->current->tool_iteration_count = 0;

    // Transition to waiting for LLM
    ik_agent_transition_to_waiting_for_llm(repl->current);

    // Get or create provider (lazy initialization)
    ik_provider_t *provider = NULL;
    res = ik_agent_get_provider_(repl->current, (void **)&provider);
    if (is_err(&res)) {
        const char *err_msg = error_message(res.err);
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        ik_agent_transition_to_idle(repl->current);
        talloc_free(res.err);
        return;
    }

    // Build normalized request from conversation
    ik_request_t *req = NULL;
    res = ik_request_build_from_conversation_(repl->current, repl->current, repl->current->shared->tool_registry,
                                              (void **)&req);
    if (is_err(&res)) {
        const char *err_msg = error_message(res.err);
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        ik_agent_transition_to_idle(repl->current);
        talloc_free(res.err);
        return;
    }

    // Start async stream (returns immediately)
    res = provider->vt->start_stream(provider->ctx, req,
                                     ik_repl_stream_callback, repl->current,
                                     ik_repl_completion_callback, repl->current);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(res.err);     // LCOV_EXCL_LINE
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));     // LCOV_EXCL_LINE
        ik_agent_transition_to_idle(repl->current);     // LCOV_EXCL_LINE
        talloc_free(res.err);     // LCOV_EXCL_LINE
    } else {
        repl->current->curl_still_running = 1;     // LCOV_EXCL_LINE
    }
}

res_t ik_cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Sync barrier: wait for running tools to complete
    if (ik_agent_has_running_tools(repl->current)) {     // LCOV_EXCL_BR_LINE
        const char *wait_msg = "Waiting for tools to complete...";     // LCOV_EXCL_LINE
        ik_scrollback_append_line(repl->current->scrollback, wait_msg, strlen(wait_msg));     // LCOV_EXCL_LINE

        // Wait for tool completion (polling pattern - event loop handles progress)
        while (ik_agent_has_running_tools(repl->current)) {     // LCOV_EXCL_BR_LINE  // LCOV_EXCL_LINE
            // Tool thread will set tool_thread_running to false when complete
            // In a unit test context, this loop won't execute because we control
            // the tool_thread_running flag manually
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};  // 10ms     // LCOV_EXCL_LINE
            nanosleep(&ts, NULL);     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Parse arguments for --model flag and prompt
    char *model_spec = NULL;
    char *prompt = NULL;
    res_t parse_res = cmd_fork_parse_args(ctx, args, &model_spec, &prompt);
    if (is_err(&parse_res)) {
        const char *err_msg = error_message(parse_res.err);
        char *styled_msg = ik_scrollback_format_warning(ctx, err_msg);
        ik_scrollback_append_line(repl->current->scrollback, styled_msg, strlen(styled_msg));
        talloc_free(parse_res.err);
        return OK(NULL);  // Error shown to user
    }

    // Concurrency check (Q9)
    if (atomic_load(&repl->shared->fork_pending)) {
        char *err_msg = ik_scrollback_format_warning(ctx, "Fork already in progress");
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        return OK(NULL);
    }
    atomic_store(&repl->shared->fork_pending, true);

    // Begin transaction (Q14)
    res_t res = ik_db_begin(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Get parent's last message ID (fork point) before creating child
    ik_agent_ctx_t *parent = repl->current;
    int64_t fork_message_id = 0;
    res = ik_db_agent_get_last_message_id(repl->shared->db_ctx, parent->uuid, &fork_message_id);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Create child agent
    ik_agent_ctx_t *child = NULL;
    res = ik_agent_create(repl, repl->shared, parent->uuid, &child);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Set repl backpointer on child agent
    child->repl = repl;

    // Set fork_message_id on child (history inheritance point)
    child->fork_message_id = fork_message_id;

    // Configure child's provider/model/thinking (either inherit or override)
    if (model_spec != NULL) {
        // Apply model override
        res = cmd_fork_apply_override(child, model_spec);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
            atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
            const char *err_msg = error_message(res.err);     // LCOV_EXCL_LINE
            ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));     // LCOV_EXCL_LINE
            talloc_free(res.err);     // LCOV_EXCL_LINE
            return OK(NULL);  // Error shown to user     // LCOV_EXCL_LINE
        }
    } else {
        // Inherit parent's configuration
        res = cmd_fork_inherit_config(child, parent);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
            atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    // Copy parent's conversation to child (history inheritance)
    res = ik_agent_copy_conversation(child, parent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Copy parent's scrollback to child (visual history inheritance)
    res = ik_scrollback_copy_from(child->scrollback, parent->scrollback);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Copy parent's pinned_paths to child (in-memory optimization)
    copy_pinned_paths(child, parent);

    // Copy parent's toolset_filter to child (in-memory optimization)
    copy_toolset_filter(child, parent);

    // Insert into registry
    res = ik_db_agent_insert(repl->shared->db_ctx, child);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Add to array
    res = ik_repl_add_agent(repl, child);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Insert fork events into database
    res = insert_fork_events(ctx, repl, parent, child, fork_message_id);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Commit transaction
    res = ik_db_commit(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Switch to child (uses ik_repl_switch_agent for state save/restore)
    res = ik_repl_switch_agent(repl, child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;  // LCOV_EXCL_LINE
    }
    atomic_store(&repl->shared->fork_pending, false);

    // Display confirmation with model information
    char *feedback = build_fork_feedback(ctx, child, model_spec != NULL);
    if (feedback == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    res = ik_scrollback_append_line(child->scrollback, feedback, strlen(feedback));
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        return res;  // LCOV_EXCL_LINE
    }

    // Warn if model doesn't support thinking but thinking level is set
    if (child->thinking_level != IK_THINKING_NONE && child->model != NULL) {
        bool supports_thinking = false;
        ik_model_supports_thinking(child->model, &supports_thinking);
        if (!supports_thinking) {
            char *warning = talloc_asprintf(ctx, "Warning: Model '%s' does not support thinking/reasoning",
                                            child->model);
            if (warning == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
            ik_scrollback_append_line(child->scrollback, warning, strlen(warning));
        }
    }

    // If prompt provided, add as user message and trigger LLM
    if (prompt != NULL && prompt[0] != '\0') {     // LCOV_EXCL_BR_LINE
        handle_fork_prompt(ctx, repl, prompt);
    }

    return OK(NULL);
}
