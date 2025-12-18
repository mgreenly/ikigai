/**
 * @file commands_fork.c
 * @brief Fork command handler implementation
 */

#include "commands.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/message.h"
#include "event_render.h"
#include "logger.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "repl.h"
#include "repl_callbacks.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Parse quoted prompt from arguments
static char *parse_fork_prompt(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    if (args == NULL || args[0] == '\0') {     // LCOV_EXCL_BR_LINE
        return NULL;
    }

    // Check if starts with quote
    if (args[0] != '"') {
        char *err_msg = talloc_strdup(ctx, "Error: Prompt must be quoted (usage: /fork \"prompt\")");
        if (err_msg == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        return talloc_strdup(ctx, "");  // Return empty string to signal error
    }

    // Find closing quote
    const char *end_quote = strchr(args + 1, '"');
    if (end_quote == NULL) {
        char *err_msg = talloc_strdup(ctx, "Error: Unterminated quoted string");
        if (err_msg == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        return talloc_strdup(ctx, "");  // Return empty string to signal error
    }

    // Extract prompt (between quotes)
    size_t prompt_len = (size_t)(end_quote - (args + 1));
    char *prompt = talloc_strndup(ctx, args + 1, prompt_len);
    if (prompt == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }
    return prompt;
}

// Handle prompt-triggered LLM call after fork
static void handle_fork_prompt(void *ctx, ik_repl_ctx_t *repl, const char *prompt)
{
    // Create user message
    res_t res = ik_openai_msg_create(repl->current->conversation, "user", prompt);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return;  // Error already logged     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    ik_msg_t *user_msg = res.ok;

    // Add to conversation
    res = ik_openai_conversation_add_msg(repl->current->conversation, user_msg);
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
    ik_repl_transition_to_waiting_for_llm(repl);

    // Trigger LLM request
    res = ik_openai_multi_add_request(repl->current->multi, repl->shared->cfg, repl->current->conversation,
                                      ik_repl_streaming_callback, repl,
                                      ik_repl_http_completion_callback, repl, false,
                                      repl->shared->logger);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(res.err);     // LCOV_EXCL_LINE
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));     // LCOV_EXCL_LINE
        ik_repl_transition_to_idle(repl);     // LCOV_EXCL_LINE
        talloc_free(res.err);     // LCOV_EXCL_LINE
    } else {
        repl->current->curl_still_running = 1;     // LCOV_EXCL_LINE
    }
}

res_t cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args)
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

    // Parse prompt argument if present
    char *prompt = parse_fork_prompt(ctx, repl, args);
    if (prompt != NULL && prompt[0] == '\0') {
        // Error was shown to user by parse_fork_prompt
        return OK(NULL);
    }

    // Concurrency check (Q9)
    if (atomic_load(&repl->shared->fork_pending)) {
        char *err_msg = talloc_strdup(ctx, "Fork already in progress");
        if (err_msg == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
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

    // Insert parent-side fork event (only if session exists)
    if (repl->shared->session_id > 0) {
        char *parent_content = talloc_asprintf(ctx, "Forked child %.22s", child->uuid);
        if (parent_content == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        char *parent_data = talloc_asprintf(ctx,
            "{\"child_uuid\":\"%s\",\"fork_message_id\":%" PRId64 ",\"role\":\"parent\"}",
            child->uuid, fork_message_id);
        if (parent_data == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                    parent->uuid, "fork", parent_content, parent_data);
        talloc_free(parent_content);
        talloc_free(parent_data);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
            atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
            return res;     // LCOV_EXCL_LINE
        }

        // Insert child-side fork event
        char *child_content = talloc_asprintf(ctx, "Forked from %.22s", parent->uuid);
        if (child_content == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        char *child_data = talloc_asprintf(ctx,
            "{\"parent_uuid\":\"%s\",\"fork_message_id\":%" PRId64 ",\"role\":\"child\"}",
            parent->uuid, fork_message_id);
        if (child_data == NULL) {     // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }
        res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                    child->uuid, "fork", child_content, child_data);
        talloc_free(child_content);
        talloc_free(child_data);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
            atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    // Commit transaction
    res = ik_db_commit(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Switch to child (uses ik_repl_switch_agent for state save/restore)
    const char *parent_uuid = parent->uuid;
    res = ik_repl_switch_agent(repl, child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;  // LCOV_EXCL_LINE
    }
    atomic_store(&repl->shared->fork_pending, false);

    // Display confirmation
    char msg[64];
    int32_t written = snprintf(msg, sizeof(msg), "Forked from %.22s", parent_uuid);
    if (written < 0 || (size_t)written >= sizeof(msg)) {  // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");  // LCOV_EXCL_LINE
    }
    res = ik_scrollback_append_line(child->scrollback, msg, (size_t)written);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        return res;  // LCOV_EXCL_LINE
    }

    // If prompt provided, add as user message and trigger LLM
    if (prompt != NULL && prompt[0] != '\0') {     // LCOV_EXCL_BR_LINE
        handle_fork_prompt(ctx, repl, prompt);
    }

    return OK(NULL);
}
