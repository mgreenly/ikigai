/**
 * @file commands_fork.c
 * @brief Fork command handler implementation
 */

#include "commands.h"
#include "commands_basic.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/message.h"
#include "event_render.h"
#include "logger.h"
#include "openai/client.h"
#include "panic.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "repl.h"
#include "repl_callbacks.h"
#include "repl_event_handlers.h"
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

/**
 * Parse /fork command arguments for --model flag and prompt
 *
 * Supports both orderings:
 * - /fork --model gpt-5 "prompt"
 * - /fork "prompt" --model gpt-5
 *
 * @param ctx Parent context for talloc allocations
 * @param input Command arguments string
 * @param model Output: model specification (NULL if no --model flag)
 * @param prompt Output: prompt string (NULL if no prompt)
 * @return OK on success, ERR on malformed input
 */
static res_t cmd_fork_parse_args(void *ctx, const char *input, char **model, char **prompt)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(model != NULL);    // LCOV_EXCL_BR_LINE
    assert(prompt != NULL);   // LCOV_EXCL_BR_LINE

    *model = NULL;
    *prompt = NULL;

    if (input == NULL || input[0] == '\0') {     // LCOV_EXCL_BR_LINE
        return OK(NULL);  // Empty args is valid (no model, no prompt)
    }

    const char *p = input;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t') {     // LCOV_EXCL_BR_LINE
        p++;
    }

    // Parse tokens in any order
    while (*p != '\0') {     // LCOV_EXCL_BR_LINE
        if (strncmp(p, "--model", 7) == 0 && (p[7] == ' ' || p[7] == '\t')) {
            // Found --model flag
            p += 7;
            // Skip whitespace after --model
            while (*p == ' ' || *p == '\t') {     // LCOV_EXCL_BR_LINE
                p++;
            }
            if (*p == '\0') {
                return ERR(ctx, INVALID_ARG, "--model requires an argument");
            }
            // Extract model spec (until next space or quote)
            const char *model_start = p;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '"') {     // LCOV_EXCL_BR_LINE
                p++;
            }
            size_t model_len = (size_t)(p - model_start);
            if (model_len == 0) {
                return ERR(ctx, INVALID_ARG, "--model requires an argument");
            }
            *model = talloc_strndup(ctx, model_start, model_len);
            if (*model == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
        } else if (*p == '"') {
            // Found quoted prompt
            p++;  // Skip opening quote
            const char *prompt_start = p;
            // Find closing quote
            while (*p != '\0' && *p != '"') {     // LCOV_EXCL_BR_LINE
                p++;
            }
            if (*p != '"') {
                return ERR(ctx, INVALID_ARG, "Unterminated quoted string");
            }
            size_t prompt_len = (size_t)(p - prompt_start);
            *prompt = talloc_strndup(ctx, prompt_start, prompt_len);
            if (*prompt == NULL) {  // LCOV_EXCL_BR_LINE
                PANIC("Out of memory");  // LCOV_EXCL_LINE
            }
            p++;  // Skip closing quote
        } else {
            // Unknown token - likely unquoted text
            return ERR(ctx, INVALID_ARG, "Error: Prompt must be quoted (usage: /fork \"prompt\") or use --model flag");
        }

        // Skip trailing whitespace
        while (*p == ' ' || *p == '\t') {     // LCOV_EXCL_BR_LINE
            p++;
        }
    }

    return OK(NULL);
}

/**
 * Apply model override to child agent
 *
 * Parses MODEL/THINKING specification and updates child's provider, model,
 * and thinking_level fields. Infers provider from model name.
 *
 * @param child Child agent to configure
 * @param model_spec Model specification (e.g., "gpt-5" or "gpt-5-mini/high")
 * @return OK on success, ERR on invalid model or provider
 */
static res_t cmd_fork_apply_override(ik_agent_ctx_t *child, const char *model_spec)
{
    assert(child != NULL);      // LCOV_EXCL_BR_LINE
    assert(model_spec != NULL); // LCOV_EXCL_BR_LINE

    // Parse MODEL/THINKING syntax (reuse existing parser)
    char *model_name = NULL;
    char *thinking_str = NULL;
    res_t parse_res = cmd_model_parse(child, model_spec, &model_name, &thinking_str);
    if (is_err(&parse_res)) {
        return parse_res;
    }

    // Infer provider from model name
    const char *provider = ik_infer_provider(model_name);
    if (provider == NULL) {
        return ERR(child, INVALID_ARG, "Unknown model '%s'", model_name);
    }

    // Set provider and model
    if (child->provider != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(child->provider);     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    child->provider = talloc_strdup(child, provider);
    if (child->provider == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    if (child->model != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(child->model);     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    child->model = talloc_strdup(child, model_name);
    if (child->model == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    // Parse thinking level if specified, otherwise keep parent's level (already inherited)
    if (thinking_str != NULL) {
        ik_thinking_level_t thinking_level;
        if (strcmp(thinking_str, "none") == 0) {
            thinking_level = IK_THINKING_NONE;
        } else if (strcmp(thinking_str, "low") == 0) {
            thinking_level = IK_THINKING_LOW;
        } else if (strcmp(thinking_str, "med") == 0) {
            thinking_level = IK_THINKING_MED;
        } else if (strcmp(thinking_str, "high") == 0) {
            thinking_level = IK_THINKING_HIGH;
        } else {
            return ERR(child, INVALID_ARG, "Invalid thinking level '%s' (must be: none, low, med, high)", thinking_str);
        }
        child->thinking_level = thinking_level;
    }

    return OK(NULL);
}

/**
 * Copy parent's provider config to child agent
 *
 * Inherits provider, model, and thinking_level from parent to child.
 * Used when no --model override is specified.
 *
 * @param child Child agent to configure
 * @param parent Parent agent to copy from
 * @return OK on success, ERR on memory allocation failure
 */
static res_t cmd_fork_inherit_config(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    assert(child != NULL);  // LCOV_EXCL_BR_LINE
    assert(parent != NULL); // LCOV_EXCL_BR_LINE

    // Copy provider
    if (parent->provider != NULL) {
        if (child->provider != NULL) {     // LCOV_EXCL_BR_LINE
            talloc_free(child->provider);     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE
        child->provider = talloc_strdup(child, parent->provider);
        if (child->provider == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
    }

    // Copy model
    if (parent->model != NULL) {
        if (child->model != NULL) {     // LCOV_EXCL_BR_LINE
            talloc_free(child->model);     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE
        child->model = talloc_strdup(child, parent->model);
        if (child->model == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
    }

    // Copy thinking level
    child->thinking_level = parent->thinking_level;

    return OK(NULL);
}

// Handle prompt-triggered LLM call after fork
static void handle_fork_prompt(void *ctx, ik_repl_ctx_t *repl, const char *prompt)
{
    // Create user message
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", prompt);

    // Add to conversation
    res_t res = ik_openai_conversation_add_msg(repl->current->conversation, user_msg);
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
    res = ik_agent_get_provider(repl->current, &provider);
    if (is_err(&res)) {
        const char *err_msg = error_message(res.err);
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        ik_agent_transition_to_idle(repl->current);
        talloc_free(res.err);
        return;
    }

    // Build normalized request from conversation
    ik_request_t *req = NULL;
    res = ik_request_build_from_conversation(repl->current, repl->current, &req);
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
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        talloc_free(parse_res.err);
        return OK(NULL);  // Error shown to user
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
                                            "{\"child_uuid\":\"%s\",\"fork_message_id\":%" PRId64
                                            ",\"role\":\"parent\"}",
                                            child->uuid,
                                            fork_message_id);
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
                                           parent->uuid,
                                           fork_message_id);
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
    res = ik_repl_switch_agent(repl, child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        atomic_store(&repl->shared->fork_pending, false);     // LCOV_EXCL_LINE
        return res;  // LCOV_EXCL_LINE
    }
    atomic_store(&repl->shared->fork_pending, false);

    // Display confirmation with model information
    char *feedback = NULL;
    if (model_spec != NULL) {
        // Override: show what child is using
        const char *thinking_level_str = NULL;
        switch (child->thinking_level) {
            case IK_THINKING_NONE: thinking_level_str = "none"; break;
            case IK_THINKING_LOW:  thinking_level_str = "low";  break;
            case IK_THINKING_MED:  thinking_level_str = "medium"; break;
            case IK_THINKING_HIGH: thinking_level_str = "high"; break;
        }
        feedback = talloc_asprintf(ctx, "Forked child with %s/%s/%s",
                                   child->provider, child->model, thinking_level_str);
    } else {
        // Inheritance: show that child inherited parent's config
        const char *thinking_level_str = NULL;
        switch (child->thinking_level) {
            case IK_THINKING_NONE: thinking_level_str = "none"; break;
            case IK_THINKING_LOW:  thinking_level_str = "low";  break;
            case IK_THINKING_MED:  thinking_level_str = "medium"; break;
            case IK_THINKING_HIGH: thinking_level_str = "high"; break;
        }
        feedback = talloc_asprintf(ctx, "Forked child with parent's model (%s/%s/%s)",
                                   child->provider, child->model, thinking_level_str);
    }
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
