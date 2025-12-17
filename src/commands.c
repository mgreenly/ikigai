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
#include <inttypes.h>
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

// Public declaration for cmd_send (non-static, declared in commands.h)
res_t cmd_send(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for cmd_check_mail (non-static, declared in commands.h)
res_t cmd_check_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for cmd_read_mail (non-static, declared in commands.h)
res_t cmd_read_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);
res_t cmd_delete_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for cmd_filter_mail (non-static, declared in commands.h)
res_t cmd_filter_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for cmd_agents (non-static, declared in commands.h)
res_t cmd_agents(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Command registry
static const ik_command_t commands[] = {
    {"clear", "Clear scrollback, session messages, and marks", cmd_clear},
    {"mark", "Create a checkpoint for rollback (usage: /mark [label])",
     ik_cmd_mark},
    {"rewind", "Rollback to a checkpoint (usage: /rewind [label])", ik_cmd_rewind},
    {"fork", "Create a child agent (usage: /fork)", cmd_fork},
    {"kill", "Terminate agent (usage: /kill [uuid])", cmd_kill},
    {"send", "Send mail to agent (usage: /send <uuid> \"message\")", cmd_send},
    {"check-mail", "Check inbox for messages", cmd_check_mail},
    {"read-mail", "Read a message (usage: /read-mail <id>)", cmd_read_mail},
    {"delete-mail", "Delete a message (usage: /delete-mail <id>)", cmd_delete_mail},
    {"filter-mail", "Filter inbox by sender (usage: /filter-mail --from <uuid>)", cmd_filter_mail},
    {"agents", "Display agent hierarchy tree", cmd_agents},
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

    // Capture scrollback line count before command execution
    size_t lines_before = ik_scrollback_get_line_count(repl->current->scrollback);

    // Look up command in registry
    for (size_t i = 0; i < command_count; i++) {     // LCOV_EXCL_BR_LINE
        if (strcmp(cmd_name, commands[i].name) == 0) {         // LCOV_EXCL_BR_LINE
            // Found matching command, invoke handler
            res_t handler_res = commands[i].handler(ctx, repl, args);

            // Persist command output to database if handler succeeded
            if (is_ok(&handler_res)) {
                ik_cmd_persist_to_db(ctx, repl, input, cmd_name, args, lines_before);
            }

            return handler_res;
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

void ik_cmd_persist_to_db(void *ctx, ik_repl_ctx_t *repl, const char *input,
                          const char *cmd_name, const char *args,
                          size_t lines_before)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    assert(input != NULL);    // LCOV_EXCL_BR_LINE
    assert(cmd_name != NULL); // LCOV_EXCL_BR_LINE

    // Only persist if database is available
    if (repl->shared->db_ctx == NULL || repl->shared->session_id <= 0) {
        return;
    }

    // Build command content: input + output
    size_t lines_after = ik_scrollback_get_line_count(repl->current->scrollback);

    // Allocate buffer for content
    char *content = talloc_asprintf(ctx, "%s\n", input);
    if (!content) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Append command output from scrollback
    for (size_t line_idx = lines_before; line_idx < lines_after; line_idx++) {
        const char *line_text = NULL;
        size_t line_len = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_idx, &line_text, &line_len);
        assert(is_ok(&line_res));  // LCOV_EXCL_BR_LINE
        char *new_content = talloc_asprintf(ctx, "%s%s\n", content, line_text);
        if (!new_content) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        talloc_free(content);
        content = new_content;
    }

    // Build data_json with command metadata
    char *data_json = NULL;
    if (args != NULL) {
        data_json = talloc_asprintf(ctx, "{\"command\":\"%s\",\"args\":\"%s\"}", cmd_name, args);
    } else {
        data_json = talloc_asprintf(ctx, "{\"command\":\"%s\",\"args\":null}", cmd_name);
    }
    if (!data_json) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Persist to database
    res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                        repl->current->uuid, "command", content, data_json);
    if (is_err(&db_res)) {
        // Log error but don't crash - memory state is authoritative
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "command", cmd_name);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_command");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
        ik_log_warn_json(log_doc);  // LCOV_EXCL_LINE
        talloc_free(db_res.err);  // LCOV_EXCL_LINE
    }
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
    ik_logger_reinit(repl->shared->logger, cwd);

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
                                            repl->current->uuid, "clear", NULL, NULL);
        if (is_err(&db_res)) {
            // Log error but don't crash - memory state is authoritative
            yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
            yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "command", "clear");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_clear");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
            ik_log_warn_json(log_doc);  // LCOV_EXCL_LINE
            talloc_free(db_res.err);  // LCOV_EXCL_LINE
        }

        // Write system message if configured (matching new session creation pattern)
        if (repl->shared->cfg->openai_system_message != NULL) {
            res_t system_res = ik_db_message_insert(
                repl->shared->db_ctx,
                repl->shared->session_id,
                repl->current->uuid,
                "system",
                repl->shared->cfg->openai_system_message,
                "{}"
                );
            if (is_err(&system_res)) {
                // Log error but don't crash - memory state is authoritative
                yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
                yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, log_root, "command", "clear");  // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, log_root, "operation", "persist_system_message");  // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(system_res.err));  // LCOV_EXCL_LINE
                ik_log_warn_json(log_doc);
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
