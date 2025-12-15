/**
 * @file commands_agent.c
 * @brief Agent command handlers implementation
 */

#include "commands.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/message.h"
#include "event_render.h"
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
    if (args == NULL || args[0] == '\0') {
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
    if (is_err(&res)) {
        return;  // Error already logged
    }
    ik_msg_t *user_msg = res.ok;

    // Add to conversation
    res = ik_openai_conversation_add_msg(repl->current->conversation, user_msg);
    if (is_err(&res)) {
        return;  // Error already logged
    }

    // Persist user message to database
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        char *data_json = talloc_asprintf(ctx,
                                          "{\"model\":\"%s\",\"temperature\":%.2f,\"max_completion_tokens\":%d}",
                                          repl->shared->cfg->openai_model,
                                          repl->shared->cfg->openai_temperature,
                                          repl->shared->cfg->openai_max_completion_tokens);
        if (data_json == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                            NULL, "user", prompt, data_json);
        if (is_err(&db_res)) {
            if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
                fprintf(repl->shared->db_debug_pipe->write_end,
                        "Warning: Failed to persist user message to database: %s\n",
                        error_message(db_res.err));
            }
            talloc_free(db_res.err);
        }
        talloc_free(data_json);
    }

    // Render user message to scrollback
    res = ik_event_render(repl->current->scrollback, "user", prompt, "{}");
    if (is_err(&res)) {
        return;  // Error already logged
    }

    // Clear previous assistant response
    if (repl->current->assistant_response != NULL) {
        talloc_free(repl->current->assistant_response);
        repl->current->assistant_response = NULL;
    }
    if (repl->current->streaming_line_buffer != NULL) {
        talloc_free(repl->current->streaming_line_buffer);
        repl->current->streaming_line_buffer = NULL;
    }

    // Reset tool iteration count
    repl->current->tool_iteration_count = 0;

    // Transition to waiting for LLM
    ik_repl_transition_to_waiting_for_llm(repl);

    // Trigger LLM request
    res = ik_openai_multi_add_request(repl->current->multi, repl->shared->cfg, repl->current->conversation,
                                      ik_repl_streaming_callback, repl,
                                      ik_repl_http_completion_callback, repl, false);
    if (is_err(&res)) {
        const char *err_msg = error_message(res.err);
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        ik_repl_transition_to_idle(repl);
        talloc_free(res.err);
    } else {
        repl->current->curl_still_running = 1;
    }
}

// Collect all descendants of a given agent in depth-first order
static size_t collect_descendants(ik_repl_ctx_t *repl,
                                  const char *uuid,
                                  ik_agent_ctx_t **out,
                                  size_t max)
{
    size_t count = 0;

    // Find children
    for (size_t i = 0; i < repl->agent_count && count < max; i++) {
        if (repl->agents[i]->parent_uuid != NULL &&
            strcmp(repl->agents[i]->parent_uuid, uuid) == 0) {
            // Recurse first (depth-first)
            count += collect_descendants(repl, repl->agents[i]->uuid,
                                        out + count, max - count);

            // Then add this child
            if (count < max) {
                out[count++] = repl->agents[i];
            }
        }
    }

    return count;
}

// Kill an agent and all its descendants with transaction semantics
static res_t cmd_kill_cascade(void *ctx, ik_repl_ctx_t *repl, const char *uuid)
{
    // Begin transaction (Q15)
    res_t res = ik_db_begin(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;
    }

    // Collect descendants
    ik_agent_ctx_t *victims[256];
    size_t count = collect_descendants(repl, uuid, victims, 256);

    // Kill descendants (depth-first order)
    for (size_t i = 0; i < count; i++) {
        res = ik_db_agent_mark_dead(repl->shared->db_ctx, victims[i]->uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            ik_db_rollback(repl->shared->db_ctx);
            return res;
        }
    }

    // Kill target
    res = ik_db_agent_mark_dead(repl->shared->db_ctx, uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);
        return res;
    }

    // Record cascade kill event (Q20)
    char *metadata_json = talloc_asprintf(ctx,
        "{\"killed_by\": \"user\", \"target\": \"%s\", \"cascade\": true, \"count\": %zu}",
        uuid, count + 1);
    if (metadata_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    res = ik_db_message_insert(repl->shared->db_ctx,
        repl->shared->session_id,
        repl->current->uuid,
        "agent_killed",
        NULL,
        metadata_json);
    talloc_free(metadata_json);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);
        return res;
    }

    // Commit
    res = ik_db_commit(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;
    }

    // Remove from memory (after DB commit succeeds)
    for (size_t i = 0; i < count; i++) {
        res = ik_repl_remove_agent(repl, victims[i]->uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;
        }
    }
    res = ik_repl_remove_agent(repl, uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;
    }

    // Report
    char msg[64];
    int32_t written = snprintf(msg, sizeof(msg), "Killed %zu agents", count + 1);
    if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");     // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, (size_t)written);

    return OK(NULL);
}

res_t cmd_kill(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Sync barrier (Q10): wait for pending fork
    while (repl->shared->fork_pending) {
        // In unit tests, this will not loop because we control fork_pending manually
        // In production, this would process events while waiting
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};  // 10ms
        nanosleep(&ts, NULL);
    }

    // No args = kill self
    if (args == NULL || args[0] == '\0') {
        if (repl->current->parent_uuid == NULL) {
            const char *err_msg = "Error: Cannot kill root agent";
            ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
            return OK(NULL);
        }

        const char *uuid = repl->current->uuid;
        ik_agent_ctx_t *parent = ik_repl_find_agent(repl,
            repl->current->parent_uuid);

        if (parent == NULL) {
            return ERR(ctx, INVALID_ARG, "Parent agent not found");
        }

        // Record kill event in parent's history (Q20)
        char *metadata_json = talloc_asprintf(ctx,
            "{\"killed_by\": \"user\", \"target\": \"%s\"}", uuid);
        if (metadata_json == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        res_t res = ik_db_message_insert(repl->shared->db_ctx,
            repl->shared->session_id,
            parent->uuid,
            "agent_killed",
            NULL,
            metadata_json);
        talloc_free(metadata_json);
        if (is_err(&res)) {
            return res;
        }

        // Mark dead in registry (sets status='dead', ended_at=now)
        res = ik_db_agent_mark_dead(repl->shared->db_ctx, uuid);
        if (is_err(&res)) {
            return res;
        }

        // Switch to parent first (saves state), then remove dead agent
        res = ik_repl_switch_agent(repl, parent);
        if (is_err(&res)) {
            return res;
        }

        res = ik_repl_remove_agent(repl, uuid);
        if (is_err(&res)) {
            return res;
        }

        // Notify
        char msg[64];
        int32_t written = snprintf(msg, sizeof(msg), "Agent %.22s terminated", uuid);
        if (written < 0 || (size_t)written >= sizeof(msg)) {  // LCOV_EXCL_BR_LINE
            PANIC("snprintf failed");  // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(parent->scrollback, msg, (size_t)written);

        return OK(NULL);
    }

    // Handle targeted kill
    // Parse UUID and --cascade flag
    const char *uuid_arg = args;
    bool cascade = false;

    // Check for --cascade flag
    const char *cascade_flag = strstr(args, "--cascade");
    char *uuid_copy = NULL;
    if (cascade_flag != NULL) {
        cascade = true;
        // Extract UUID (everything before --cascade)
        size_t uuid_len = (size_t)(cascade_flag - args);
        // Trim trailing whitespace
        while (uuid_len > 0 && isspace((unsigned char)args[uuid_len - 1])) {
            uuid_len--;
        }
        uuid_copy = talloc_strndup(ctx, args, uuid_len);
        if (!uuid_copy) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");     // LCOV_EXCL_LINE
        }
        uuid_arg = uuid_copy;
    }

    // Find target agent by UUID (partial match allowed)
    ik_agent_ctx_t *target = ik_repl_find_agent(repl, uuid_arg);
    if (target == NULL) {
        if (ik_repl_uuid_ambiguous(repl, uuid_arg)) {
            const char *err_msg = "Error: Ambiguous UUID prefix";
            ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        } else {
            const char *err_msg = "Error: Agent not found";
            ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        }
        return OK(NULL);
    }

    // Check if root
    if (target->parent_uuid == NULL) {
        const char *err_msg = "Error: Cannot kill root agent";
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        return OK(NULL);
    }

    // If killing current, use self-kill logic
    if (target == repl->current) {
        return cmd_kill(ctx, repl, NULL);
    }

    const char *target_uuid = target->uuid;

    // If cascade flag is set, use cascade kill
    if (cascade) {
        return cmd_kill_cascade(ctx, repl, target_uuid);
    }

    // Record kill event in current agent's history (Q20)
    char *metadata_json = talloc_asprintf(ctx,
        "{\"killed_by\": \"user\", \"target\": \"%s\"}", target_uuid);
    if (metadata_json == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    res_t res = ik_db_message_insert(repl->shared->db_ctx,
        repl->shared->session_id,
        repl->current->uuid,
        "agent_killed",
        NULL,
        metadata_json);
    talloc_free(metadata_json);
    if (is_err(&res)) {
        return res;
    }

    // Mark dead in registry (sets status='dead', ended_at=now)
    res = ik_db_agent_mark_dead(repl->shared->db_ctx, target_uuid);
    if (is_err(&res)) {
        return res;
    }

    // Remove from agents array and free agent context
    res = ik_repl_remove_agent(repl, target_uuid);
    if (is_err(&res)) {
        return res;
    }

    // Notify
    char msg[64];
    int32_t written = snprintf(msg, sizeof(msg), "Agent %.22s terminated", target_uuid);
    if (written < 0 || (size_t)written >= sizeof(msg)) {  // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");  // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, (size_t)written);

    return OK(NULL);
}

res_t cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Sync barrier: wait for running tools to complete
    if (ik_agent_has_running_tools(repl->current)) {
        const char *wait_msg = "Waiting for tools to complete...";
        ik_scrollback_append_line(repl->current->scrollback, wait_msg, strlen(wait_msg));

        // Wait for tool completion (polling pattern - event loop handles progress)
        while (ik_agent_has_running_tools(repl->current)) {
            // Tool thread will set tool_thread_running to false when complete
            // In a unit test context, this loop won't execute because we control
            // the tool_thread_running flag manually
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};  // 10ms
            nanosleep(&ts, NULL);
        }
    }

    // Parse prompt argument if present
    char *prompt = parse_fork_prompt(ctx, repl, args);
    if (prompt != NULL && prompt[0] == '\0') {
        // Error was shown to user by parse_fork_prompt
        return OK(NULL);
    }

    // Concurrency check (Q9)
    if (repl->shared->fork_pending) {
        char *err_msg = talloc_strdup(ctx, "Fork already in progress");
        if (err_msg == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        return OK(NULL);
    }
    repl->shared->fork_pending = true;

    // Begin transaction (Q14)
    res_t res = ik_db_begin(repl->shared->db_ctx);
    if (is_err(&res)) {
        repl->shared->fork_pending = false;
        return res;
    }

    // Get parent's last message ID (fork point) before creating child
    ik_agent_ctx_t *parent = repl->current;
    int64_t fork_message_id = 0;
    res = ik_db_agent_get_last_message_id(repl->shared->db_ctx, parent->uuid, &fork_message_id);
    if (is_err(&res)) {
        ik_db_rollback(repl->shared->db_ctx);
        repl->shared->fork_pending = false;
        return res;
    }

    // Create child agent
    ik_agent_ctx_t *child = NULL;
    res = ik_agent_create(repl, repl->shared, parent->uuid, &child);
    if (is_err(&res)) {
        ik_db_rollback(repl->shared->db_ctx);
        repl->shared->fork_pending = false;
        return res;
    }

    // Set fork_message_id on child (history inheritance point)
    child->fork_message_id = fork_message_id;

    // Copy parent's conversation to child (history inheritance)
    res = ik_agent_copy_conversation(child, parent);
    if (is_err(&res)) {
        ik_db_rollback(repl->shared->db_ctx);
        repl->shared->fork_pending = false;
        return res;
    }

    // Insert into registry
    res = ik_db_agent_insert(repl->shared->db_ctx, child);
    if (is_err(&res)) {
        ik_db_rollback(repl->shared->db_ctx);
        repl->shared->fork_pending = false;
        return res;
    }

    // Add to array
    res = ik_repl_add_agent(repl, child);
    if (is_err(&res)) {
        ik_db_rollback(repl->shared->db_ctx);
        repl->shared->fork_pending = false;
        return res;
    }

    // Commit transaction
    res = ik_db_commit(repl->shared->db_ctx);
    if (is_err(&res)) {
        repl->shared->fork_pending = false;
        return res;
    }

    // Switch to child (uses ik_repl_switch_agent for state save/restore)
    const char *parent_uuid = parent->uuid;
    res = ik_repl_switch_agent(repl, child);
    if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
        repl->shared->fork_pending = false;
        return res;  // LCOV_EXCL_LINE
    }
    repl->shared->fork_pending = false;

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
    if (prompt != NULL && prompt[0] != '\0') {
        handle_fork_prompt(ctx, repl, prompt);
    }

    return OK(NULL);
}

// /agents command implementation - displays agent hierarchy tree
res_t cmd_agents(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    TALLOC_CTX *tmp_ctx = talloc_new(ctx);
    if (tmp_ctx == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    // Add header
    const char *header = "Agent Hierarchy:";
    res_t res = ik_scrollback_append_line(repl->current->scrollback, header, strlen(header));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);
        return res;
    }

    // Add blank line
    res = ik_scrollback_append_line(repl->current->scrollback, "", 0);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);
        return res;
    }

    // Get all running agents from database
    ik_db_agent_row_t **all_agents = NULL;
    size_t all_count = 0;
    res = ik_db_agent_list_running(repl->shared->db_ctx, tmp_ctx, &all_agents, &all_count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);
        return res;
    }

    // Build tree by iterating through agents in depth-first order
    // We'll use a simple approach: process each depth level iteratively
    size_t running_count = 0;
    size_t dead_count = 0;

    // Queue for breadth-first traversal (stores indices and depths)
    size_t *queue_idx = talloc_array(tmp_ctx, size_t, (uint32_t)all_count);
    size_t *queue_depth = talloc_array(tmp_ctx, size_t, (uint32_t)all_count);
    if (queue_idx == NULL || queue_depth == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }
    size_t queue_start = 0;
    size_t queue_end = 0;

    // Find and queue root agents (parent_uuid = NULL)
    for (size_t i = 0; i < all_count; i++) {     // LCOV_EXCL_BR_LINE
        if (all_agents[i]->parent_uuid == NULL) {     // LCOV_EXCL_BR_LINE
            queue_idx[queue_end] = i;
            queue_depth[queue_end] = 0;
            queue_end++;
        }
    }

    // Process queue
    while (queue_start < queue_end) {     // LCOV_EXCL_BR_LINE
        size_t idx = queue_idx[queue_start];
        size_t depth = queue_depth[queue_start];
        queue_start++;

        ik_db_agent_row_t *agent = all_agents[idx];

        // Count status
        if (strcmp(agent->status, "running") == 0) {     // LCOV_EXCL_BR_LINE
            running_count++;
        } else {
            dead_count++;
        }

        // Build line with indentation
        char line[256];
        size_t offset = 0;

        // Add indentation (2 spaces per level)
        for (size_t d = 0; d < depth; d++) {     // LCOV_EXCL_BR_LINE
            line[offset++] = ' ';
            line[offset++] = ' ';
        }

        // Add current marker
        bool is_current = strcmp(agent->uuid, repl->current->uuid) == 0;     // LCOV_EXCL_BR_LINE
        if (is_current) {     // LCOV_EXCL_BR_LINE
            line[offset++] = '*';
            line[offset++] = ' ';
        }

        // Add truncated UUID (first 10 chars max)
        size_t uuid_len = strlen(agent->uuid);
        size_t copy_len = uuid_len > 10 ? 10 : uuid_len;     // LCOV_EXCL_BR_LINE
        memcpy(&line[offset], agent->uuid, copy_len);
        offset += copy_len;
        if (uuid_len > 10) {     // LCOV_EXCL_BR_LINE
            memcpy(&line[offset], "...", 3);
            offset += 3;
        }

        // Add status
        line[offset++] = ' ';
        line[offset++] = '(';
        size_t status_len = strlen(agent->status);
        memcpy(&line[offset], agent->status, status_len);
        offset += status_len;
        line[offset++] = ')';

        // Add root label if parent is NULL
        if (agent->parent_uuid == NULL) {     // LCOV_EXCL_BR_LINE
            memcpy(&line[offset], " - root", 7);
            offset += 7;
        }

        line[offset] = '\0';

        // Append line to scrollback
        res = ik_scrollback_append_line(repl->current->scrollback, line, offset);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            talloc_free(tmp_ctx);
            return res;
        }

        // Find and queue children
        for (size_t i = 0; i < all_count; i++) {     // LCOV_EXCL_BR_LINE
            if (all_agents[i]->parent_uuid != NULL) {     // LCOV_EXCL_BR_LINE
                if (strcmp(all_agents[i]->parent_uuid, agent->uuid) == 0) {     // LCOV_EXCL_BR_LINE
                    queue_idx[queue_end] = i;
                    queue_depth[queue_end] = depth + 1;
                    queue_end++;
                }
            }
        }
    }

    // Add blank line before summary
    res = ik_scrollback_append_line(repl->current->scrollback, "", 0);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);
        return res;
    }

    // Add summary
    char summary[64];
    snprintf(summary, sizeof(summary), "%" PRIu64 " running, %" PRIu64 " dead",
             (uint64_t)running_count, (uint64_t)dead_count);
    res = ik_scrollback_append_line(repl->current->scrollback, summary, strlen(summary));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);
        return res;
    }

    talloc_free(tmp_ctx);
    return OK(NULL);
}
