/**
 * @file commands_basic.c
 * @brief Basic REPL command implementations (clear, help, debug)
 */

#include "commands_basic.h"

#include "agent.h"
#include "commands.h"
#include "db/message.h"
#include "event_render.h"
#include "logger.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

res_t ik_cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args)
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
    ik_agent_clear_messages(repl->current);

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
            yyjson_mut_doc *log_doc = ik_log_create();
            yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
            yyjson_mut_obj_add_str_(log_doc, log_root, "event", "db_persist_failed");
            yyjson_mut_obj_add_str_(log_doc, log_root, "command", "clear");
            yyjson_mut_obj_add_str_(log_doc, log_root, "operation", "persist_clear");
            yyjson_mut_obj_add_str_(log_doc, log_root, "error", error_message(db_res.err));
            ik_log_warn_json(log_doc);
            talloc_free(db_res.err);
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
                yyjson_mut_doc *log_doc = ik_log_create();
                yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);
                yyjson_mut_obj_add_str_(log_doc, log_root, "event", "db_persist_failed");
                yyjson_mut_obj_add_str_(log_doc, log_root, "command", "clear");
                yyjson_mut_obj_add_str_(log_doc, log_root, "operation", "persist_system_message");
                yyjson_mut_obj_add_str_(log_doc, log_root, "error", error_message(system_res.err));
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

res_t ik_cmd_help(void *ctx, ik_repl_ctx_t *repl, const char *args)
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

    // Create sorted array of command indices
    size_t *indices = talloc_array_(ctx, sizeof(size_t), count);
    if (!indices) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    for (size_t i = 0; i < count; i++) {
        indices[i] = i;
    }

    // Bubble sort indices by command name (alphabetically)
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            if (strcmp(cmds[indices[j]].name, cmds[indices[j + 1]].name) > 0) {
                size_t temp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp;
            }
        }
    }

    // Append each command with description in alphabetical order
    for (size_t i = 0; i < count; i++) {     // LCOV_EXCL_BR_LINE
        size_t idx = indices[i];
        char *cmd_line = talloc_asprintf(ctx, "  /%s - %s",
                                         cmds[idx].name, cmds[idx].description);
        if (!cmd_line) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        result = ik_scrollback_append_line(repl->current->scrollback, cmd_line, strlen(cmd_line));
        talloc_free(cmd_line);
        if (is_err(&result)) {  /* LCOV_EXCL_BR_LINE */
            talloc_free(indices);  // LCOV_EXCL_LINE
            return result;  // LCOV_EXCL_LINE
        }
    }

    talloc_free(indices);

    return OK(NULL);
}

res_t ik_cmd_debug(void *ctx, ik_repl_ctx_t *repl, const char *args)
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

res_t ik_cmd_exit(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)ctx;
    (void)args;

    // Abort any in-flight LLM calls by invalidating all provider instances
    // This triggers cleanup of HTTP connections and cancels pending requests
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_invalidate_provider(repl->agents[i]);
    }

    repl->quit = true;

    return OK(NULL);
}
