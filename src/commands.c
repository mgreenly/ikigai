/**
 * @file commands.c
 * @brief REPL command registry and dispatcher implementation
 */

#include "commands.h"

#include "ansi.h"
#include "commands_basic.h"
#include "commands_mark.h"
#include "commands_model.h"
#include "commands_pin.h"
#include "commands_tool.h"
#include "commands_toolset.h"
#include "db/message.h"
#include "logger.h"
#include "output_style.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "shared.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
// Public declaration for ik_cmd_fork (non-static, declared in commands.h)
res_t ik_cmd_fork(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for ik_cmd_kill (non-static, declared in commands.h)
res_t ik_cmd_kill(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for ik_cmd_send (non-static, declared in commands.h)
res_t ik_cmd_send(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for ik_cmd_check_mail (non-static, declared in commands.h)
res_t ik_cmd_check_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for ik_cmd_read_mail (non-static, declared in commands.h)
res_t ik_cmd_read_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);
res_t ik_cmd_delete_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for ik_cmd_filter_mail (non-static, declared in commands.h)
res_t ik_cmd_filter_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Public declaration for ik_cmd_agents (non-static, declared in commands.h)
res_t ik_cmd_agents(void *ctx, ik_repl_ctx_t *repl, const char *args);

// Command registry
static const ik_command_t commands[] = {
    {"clear", "Clear scrollback, session messages, and marks", ik_cmd_clear},
    {"mark", "Create a checkpoint for rollback (usage: /mark [label])",
     ik_cmd_mark},
    {"rewind", "Rollback to a checkpoint (usage: /rewind [label])", ik_cmd_rewind},
    {"fork", "Create a child agent (usage: /fork)", ik_cmd_fork},
    {"kill", "Terminate agent (usage: /kill [uuid])", ik_cmd_kill},
    {"mail-send", "Send mail to agent (usage: /mail-send <uuid> \"message\")", ik_cmd_send},
    {"mail-check", "Check inbox for messages", ik_cmd_check_mail},
    {"mail-read", "Read a message (usage: /mail-read <id>)", ik_cmd_read_mail},
    {"mail-delete", "Delete a message (usage: /mail-delete <id>)", ik_cmd_delete_mail},
    {"mail-filter", "Filter inbox by sender (usage: /mail-filter --from <uuid>)", ik_cmd_filter_mail},
    {"agents", "Display agent hierarchy tree", ik_cmd_agents},
    {"help", "Show available commands", ik_cmd_help},
    {"model", "Switch LLM model (usage: /model <name>)", ik_cmd_model},
    {"system", "Set system message (usage: /system <text>)", ik_cmd_system},
    {"tool", "List tools or show schema (usage: /tool [name])", ik_cmd_tool},
    {"refresh", "Reload tool registry", ik_cmd_refresh},
    {"pin", "Manage pinned documents (usage: /pin [path])", ik_cmd_pin},
    {"unpin", "Remove pinned document (usage: /unpin <path>)", ik_cmd_unpin},
    {"toolset", "Filter tools visible to LLM (usage: /toolset [tool1, tool2, ...])", ik_cmd_toolset},
    {"exit", "Exit the application", ik_cmd_exit},
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
        char *msg = ik_scrollback_format_warning(ctx, "Empty command");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
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

    // Echo command to scrollback before execution
    {
        int32_t color_code = ik_output_color(IK_OUTPUT_SLASH_CMD);
        char color_seq[16] = {0};
        if (ik_ansi_colors_enabled() && color_code >= 0) {
            ik_ansi_fg_256(color_seq, sizeof(color_seq), (uint8_t)color_code);
        }

        char *echo_line = NULL;
        if (color_seq[0] != '\0') {
            echo_line = talloc_asprintf(ctx, "%s%s%s", color_seq, input, IK_ANSI_RESET);
        } else {
            echo_line = talloc_strdup(ctx, input);
        }

        if (!echo_line) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");    // LCOV_EXCL_LINE
        }

        res_t echo_res = ik_scrollback_append_line(repl->current->scrollback, echo_line, strlen(echo_line));
        talloc_free(echo_line);
        if (is_err(&echo_res)) {     // LCOV_EXCL_BR_LINE
            return echo_res;     // LCOV_EXCL_LINE
        }

        // Append blank line after command echo
        echo_res = ik_scrollback_append_line(repl->current->scrollback, "", 0);
        if (is_err(&echo_res)) {     // LCOV_EXCL_BR_LINE
            return echo_res;     // LCOV_EXCL_LINE
        }
    }

    // Capture scrollback line count before command execution
    size_t lines_before = ik_scrollback_get_line_count(repl->current->scrollback);

    // Look up command in registry
    for (size_t i = 0; i < command_count; i++) {     // LCOV_EXCL_BR_LINE
        if (strcmp(cmd_name, commands[i].name) == 0) {         // LCOV_EXCL_BR_LINE
            // Found matching command, invoke handler
            res_t handler_res = commands[i].handler(ctx, repl, args);

            // Add blank line after command output if handler produced output
            size_t lines_after = ik_scrollback_get_line_count(repl->current->scrollback);
            if (is_ok(&handler_res) && lines_after > lines_before) {
                res_t blank_res = ik_scrollback_append_line(repl->current->scrollback, "", 0);
                if (is_err(&blank_res)) {     // LCOV_EXCL_BR_LINE
                    return blank_res;     // LCOV_EXCL_LINE
                }
            }

            // Persist command output to database if handler succeeded
            if (is_ok(&handler_res)) {
                ik_cmd_persist_to_db(ctx, repl, input, cmd_name, args, lines_before);
            }

            return handler_res;
        }
    }

    // Unknown command
    char *err_text = talloc_asprintf(ctx, "Unknown command '%s'", cmd_name);
    if (!err_text) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    char *msg = ik_scrollback_format_warning(ctx, err_text);
    talloc_free(err_text);
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    talloc_free(msg);
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

    // Build command output (content only, not echo)
    size_t lines_after = ik_scrollback_get_line_count(repl->current->scrollback);

    // Build content from command output lines
    char *content = NULL;
    if (lines_after > lines_before) {
        // Append command output from scrollback
        for (size_t line_idx = lines_before; line_idx < lines_after; line_idx++) {
            const char *line_text = NULL;
            size_t line_len = 0;
            res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_idx, &line_text, &line_len);
            assert(is_ok(&line_res));  // LCOV_EXCL_BR_LINE
            if (content == NULL) {
                content = talloc_asprintf(ctx, "%s\n", line_text);
            } else {
                char *new_content = talloc_asprintf(ctx, "%s%s\n", content, line_text);
                if (!new_content) {     // LCOV_EXCL_BR_LINE
                    PANIC("OOM");   // LCOV_EXCL_LINE
                }
                talloc_free(content);
                content = new_content;
            }
            if (!content) {     // LCOV_EXCL_BR_LINE
                PANIC("OOM");   // LCOV_EXCL_LINE
            }
        }
    }

    // Build data_json with command metadata and echo
    char *data_json = NULL;
    if (args != NULL) {
        data_json = talloc_asprintf(ctx, "{\"command\":\"%s\",\"args\":\"%s\",\"echo\":\"%s\"}",
                                    cmd_name, args, input);
    } else {
        data_json = talloc_asprintf(ctx, "{\"command\":\"%s\",\"args\":null,\"echo\":\"%s\"}",
                                    cmd_name, input);
    }
    if (!data_json) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Persist to database
    res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                        repl->current->uuid, "command", content, data_json);
    if (is_err(&db_res)) {     // LCOV_EXCL_BR_LINE - Success path tested in integration, error path in unit tests
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
