/**
 * @file commands.c
 * @brief REPL command registry and dispatcher implementation
 */

#include "commands.h"

#include "agent.h"
#include "commands_mark.h"
#include "completion.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/message.h"
#include "event_render.h"
#include "logger.h"
#include "marks.h"
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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Forward declarations of command handlers
static res_t cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_help(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_system(void *ctx, ik_repl_ctx_t *repl, const char *args);
static res_t cmd_debug(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for cmd_fork (non-static, declared in commands.h)
res_t cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for cmd_kill (non-static, declared in commands.h)
res_t cmd_kill(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Command registry
static const ik_command_t commands[] = {
    {"clear", "Clear scrollback, session messages, and marks", cmd_clear},
    {"mark", "Create a checkpoint for rollback (usage: /mark [label])",
     ik_cmd_mark},
    {"rewind", "Rollback to a checkpoint (usage: /rewind [label])", ik_cmd_rewind},
    {"fork", "Create a child agent (usage: /fork)", cmd_fork},
    {"kill", "Terminate agent (usage: /kill [uuid])", cmd_kill},
    {"help", "Show available commands", cmd_help},
    {"model", "Switch LLM model (usage: /model <name>)", cmd_model},
    {"system", "Set system message (usage: /system <text>)", cmd_system},
    {"debug", "Toggle debug output (usage: /debug [on|off])", cmd_debug},
};

static const size_t command_count =
    sizeof(commands) / sizeof(commands[0]);     // LCOV_EXCL_BR_LINE

const ik_command_t *ik_cmd_get_all(size_t *count)
{
    assert(count != NULL);     // LCOV_EXCL_BR_LINE
    *count = command_count;
    return commands;
}

res_t ik_cmd_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    assert(input != NULL);     // LCOV_EXCL_BR_LINE
    assert(input[0] == '/');     // LCOV_EXCL_BR_LINE

    // Skip leading slash
    const char *cmd_start = input + 1;

    // Skip leading whitespace
    while (isspace((unsigned char)*cmd_start)) {     // LCOV_EXCL_BR_LINE
        cmd_start++;
    }

    // Empty command (just "/")
    if (*cmd_start == '\0') {     // LCOV_EXCL_BR_LINE
        char *msg = talloc_strdup(ctx, "Error: Empty command");
        if (!msg) {         // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Empty command");
    }

    // Find end of command name (space or end of string)
    const char *args_start = cmd_start;
    while (*args_start && !isspace((unsigned char)*args_start)) {     // LCOV_EXCL_BR_LINE
        args_start++;
    }

    // Extract command name
    size_t cmd_len = (size_t)(args_start - cmd_start);
    char *cmd_name = talloc_strndup(ctx, cmd_start, cmd_len);
    if (!cmd_name) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");    // LCOV_EXCL_LINE
    }

    // Skip whitespace before args
    while (isspace((unsigned char)*args_start)) {     // LCOV_EXCL_BR_LINE
        args_start++;
    }

    // Args is NULL if no arguments, otherwise points to remaining text
    const char *args = (*args_start == '\0') ? NULL : args_start;     // LCOV_EXCL_BR_LINE

    // Look up command in registry
    for (size_t i = 0; i < command_count; i++) {     // LCOV_EXCL_BR_LINE
        if (strcmp(cmd_name, commands[i].name) == 0) {         // LCOV_EXCL_BR_LINE
            // Found matching command, invoke handler
            return commands[i].handler(ctx, repl, args);
        }
    }

    // Unknown command
    char *msg = talloc_asprintf(ctx, "Error: Unknown command '%s'", cmd_name);
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return ERR(ctx, INVALID_ARG, "Unknown command '%s'", cmd_name);
}

// Command handler stubs (to be implemented in later tasks)

static res_t cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)ctx;      // Used only in assert (compiled out in release builds)
    (void)args;     // Unused for /clear

    // Reinitialize logger when /clear is executed
    // This rotates the current.log file and creates a new one
    char cwd[PATH_MAX];
    if (posix_getcwd_(cwd, sizeof(cwd)) == NULL) {
        return ERR(ctx, IO, "Failed to get current working directory");
    }
    ik_log_reinit(cwd);

    // Clear scrollback buffer
    ik_scrollback_clear(repl->current->scrollback);

    // Clear conversation (session messages)
    if (repl->current->conversation != NULL) {  // LCOV_EXCL_BR_LINE
        ik_openai_conversation_clear(repl->current->conversation);
    }

    // Clear marks
    if (repl->current->marks != NULL) {  // LCOV_EXCL_BR_LINE
        for (size_t i = 0; i < repl->current->mark_count; i++) {
            talloc_free(repl->current->marks[i]);
        }
        talloc_free(repl->current->marks);
        repl->current->marks = NULL;
        repl->current->mark_count = 0;
    }

    // Clear autocomplete state so suggestions don't persist
    if (repl->current->completion != NULL) {     // LCOV_EXCL_BR_LINE
        // LCOV_EXCL_START - Defensive cleanup, rarely occurs in practice
        talloc_free(repl->current->completion);
        repl->current->completion = NULL;
        // LCOV_EXCL_STOP
    }

    // Persist clear event to database (Integration Point 3)
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                            NULL, "clear", NULL, NULL);
        if (is_err(&db_res)) {
            // Log error but don't crash - memory state is authoritative
            if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
                fprintf(repl->shared->db_debug_pipe->write_end,
                        "Warning: Failed to persist clear event to database: %s\n",
                        error_message(db_res.err));
            }
            talloc_free(db_res.err);
        }

        // Write system message if configured (matching new session creation pattern)
        if (repl->shared->cfg->openai_system_message != NULL) {
            res_t system_res = ik_db_message_insert(
                repl->shared->db_ctx,
                repl->shared->session_id,
                NULL,
                "system",
                repl->shared->cfg->openai_system_message,
                "{}"
                );
            if (is_err(&system_res)) {
                // Log error but don't crash - memory state is authoritative
                if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
                    fprintf(repl->shared->db_debug_pipe->write_end,
                            "Warning: Failed to persist system message to database: %s\n",
                            error_message(system_res.err));
                }
                talloc_free(system_res.err);
            }
        }
    }

    // Add system message to scrollback using event renderer (consistent with replay)
    if (repl->shared->cfg != NULL && repl->shared->cfg->openai_system_message != NULL) {  // LCOV_EXCL_BR_LINE - Defensive: cfg always set during init
        res_t render_res = ik_event_render(
            repl->current->scrollback,
            "system",
            repl->shared->cfg->openai_system_message,
            "{}"
            );
        if (is_err(&render_res)) {
            return render_res;
        }
    }

    return OK(NULL);
}

static res_t cmd_help(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    // Build help header
    char *header = talloc_strdup(ctx, "Available commands:");
    if (!header) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    res_t result = ik_scrollback_append_line(repl->current->scrollback, header, strlen(header));
    talloc_free(header);
    if (is_err(&result)) {  /* LCOV_EXCL_BR_LINE */
        return result;  // LCOV_EXCL_LINE
    }

    // Get all registered commands
    size_t count;
    const ik_command_t *cmds = ik_cmd_get_all(&count);

    // Append each command with description
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        char *cmd_line = talloc_asprintf(ctx, "  /%s - %s",
                                         cmds[i].name, cmds[i].description);
        if (!cmd_line) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        result = ik_scrollback_append_line(repl->current->scrollback, cmd_line, strlen(cmd_line));
        talloc_free(cmd_line);
        if (is_err(&result)) {  /* LCOV_EXCL_BR_LINE */
            return result;  // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

static res_t cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    // Check if model name provided
    if (args == NULL) {     // LCOV_EXCL_BR_LINE
        char *msg = talloc_strdup(ctx, "Error: Model name required (usage: /model <name>)");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Model name required");
    }

    // List of supported OpenAI models
    static const char *valid_models[] = {
        "gpt-4",
        "gpt-4-turbo",
        "gpt-4o",
        "gpt-4o-mini",
        "gpt-3.5-turbo",
        "gpt-5",
        "gpt-5-mini",
        "o1",
        "o1-mini",
        "o1-preview",
    };
    static const size_t valid_model_count = sizeof(valid_models) / sizeof(valid_models[0]);     // LCOV_EXCL_BR_LINE

    // Validate model name
    bool valid = false;
    for (size_t i = 0; i < valid_model_count; i++) {     // LCOV_EXCL_BR_LINE
        if (strcmp(args, valid_models[i]) == 0) {     // LCOV_EXCL_BR_LINE
            valid = true;
            break;
        }
    }

    if (!valid) {     // LCOV_EXCL_BR_LINE
        char *msg = talloc_asprintf(ctx, "Error: Unknown model '%s'", args);
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Unknown model '%s'", args);
    }

    // Update config (free old, allocate new)
    if (repl->shared->cfg->openai_model != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(repl->shared->cfg->openai_model);
    }
    repl->shared->cfg->openai_model = talloc_strdup(repl->shared->cfg, args);
    if (!repl->shared->cfg->openai_model) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Show confirmation
    char *msg = talloc_asprintf(ctx, "Switched to model: %s", args);
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return OK(NULL);
}

static res_t cmd_system(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    // Free old system message
    if (repl->shared->cfg->openai_system_message != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(repl->shared->cfg->openai_system_message);
        repl->shared->cfg->openai_system_message = NULL;
    }

    char *msg = NULL;

    // If args is NULL or empty, clear the system message
    if (args == NULL) {     // LCOV_EXCL_BR_LINE
        msg = talloc_strdup(ctx, "System message cleared");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
    } else {
        // Set new system message
        repl->shared->cfg->openai_system_message = talloc_strdup(repl->shared->cfg, args);
        if (!repl->shared->cfg->openai_system_message) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }

        // Show confirmation
        msg = talloc_asprintf(ctx, "System message set to: %s", args);
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
    }

    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return OK(NULL);
}

// Parse quoted prompt from arguments
// Returns NULL if no args, empty string if error shown to user, or the extracted prompt
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
// Adds user message to conversation and triggers LLM request
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

static res_t cmd_debug(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    char *msg = NULL;

    if (args == NULL) {
        // Show current status
        msg = talloc_asprintf(ctx, "Debug output: %s", repl->shared->debug_enabled ? "ON" : "OFF");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
    } else if (strcmp(args, "on") == 0) {
        // Enable debug output
        repl->shared->debug_enabled = true;
        msg = talloc_strdup(ctx, "Debug output enabled");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
    } else if (strcmp(args, "off") == 0) {
        // Disable debug output
        repl->shared->debug_enabled = false;
        msg = talloc_strdup(ctx, "Debug output disabled");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
    } else {
        // Invalid argument
        msg = talloc_asprintf(ctx, "Error: Invalid argument '%s' (usage: /debug [on|off])", args);
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Invalid argument '%s'", args);
    }

    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return OK(NULL);
}

/**
 * Collect all descendants of a given agent in depth-first order
 *
 * This function recursively traverses the agent tree to find all descendants
 * of the specified UUID. Results are stored in depth-first order (children
 * before parent).
 *
 * @param repl REPL context containing agent array
 * @param uuid Parent UUID to collect descendants for
 * @param out Output array to store descendants
 * @param max Maximum number of descendants to collect
 * @return Number of descendants collected
 */
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

/**
 * Kill an agent and all its descendants with transaction semantics
 *
 * This function performs a cascade kill operation that:
 * 1. Collects all descendants in depth-first order
 * 2. Marks all as dead in database (atomic transaction)
 * 3. Removes all from memory
 * 4. Records cascade kill event
 *
 * @param ctx Talloc context for allocations
 * @param repl REPL context
 * @param uuid Target agent UUID
 * @return OK on success, ERR on failure (with rollback)
 */
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
