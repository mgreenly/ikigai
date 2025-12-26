/**
 * @file commands_basic.c
 * @brief Basic REPL command implementations (clear, help, model, system, debug)
 */

#include "commands_basic.h"

#include "agent.h"
#include "commands.h"
#include "db/agent.h"
#include "db/message.h"
#include "event_render.h"
#include "logger.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

// Include provider.h after other headers to avoid type conflicts
#include "providers/provider.h"

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

res_t ik_cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    // Check if model name provided
    if (args == NULL) {     // LCOV_EXCL_BR_LINE
        char *msg = talloc_strdup(ctx, "Error: Model name required (usage: /model <name>[/thinking_level])");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Model name required");
    }

    // Check if an LLM request is currently active
    if (repl->current->state == IK_AGENT_STATE_WAITING_FOR_LLM) {
        char *msg = talloc_strdup(ctx, "Error: Cannot switch models during active request");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Cannot switch models during active request");
    }

    // Parse MODEL/THINKING syntax
    char *model_name = NULL;
    char *thinking_str = NULL;
    res_t parse_res = cmd_model_parse(ctx, args, &model_name, &thinking_str);
    if (is_err(&parse_res)) {
        char *msg = talloc_asprintf(ctx, "Error: %s", error_message(parse_res.err));
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return parse_res;
    }

    // Infer provider from model name
    const char *provider = ik_infer_provider(model_name);
    if (provider == NULL) {
        char *msg = talloc_asprintf(ctx, "Error: Unknown model '%s'", model_name);
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Unknown model '%s'", model_name);
    }

    // Parse thinking level (use current if not specified)
    ik_thinking_level_t thinking_level = repl->current->thinking_level;
    if (thinking_str != NULL) {
        if (strcmp(thinking_str, "none") == 0) {
            thinking_level = IK_THINKING_NONE;
        } else if (strcmp(thinking_str, "low") == 0) {
            thinking_level = IK_THINKING_LOW;
        } else if (strcmp(thinking_str, "med") == 0) {
            thinking_level = IK_THINKING_MED;
        } else if (strcmp(thinking_str, "high") == 0) {
            thinking_level = IK_THINKING_HIGH;
        } else {
            char *msg = talloc_asprintf(ctx,
                                        "Error: Invalid thinking level '%s' (must be: none, low, med, high)",
                                        thinking_str);
            if (!msg) {     // LCOV_EXCL_BR_LINE
                PANIC("OOM");   // LCOV_EXCL_LINE
            }
            ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
            return ERR(ctx, INVALID_ARG, "Invalid thinking level '%s'", thinking_str);
        }
    }

    // Update agent state
    if (repl->current->provider != NULL) {
        talloc_free(repl->current->provider);
    }
    repl->current->provider = talloc_strdup(repl->current, provider);
    if (!repl->current->provider) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    if (repl->current->model != NULL) {
        talloc_free(repl->current->model);
    }
    repl->current->model = talloc_strdup(repl->current, model_name);
    if (!repl->current->model) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    repl->current->thinking_level = thinking_level;

    // Invalidate cached provider instance
    ik_agent_invalidate_provider(repl->current);

    // Persist to database
    if (repl->shared->db_ctx != NULL) {
        const char *thinking_level_str = NULL;
        switch (thinking_level) {
            case IK_THINKING_NONE: thinking_level_str = "none"; break;
            case IK_THINKING_LOW:  thinking_level_str = "low";  break;
            case IK_THINKING_MED:  thinking_level_str = "med";  break;
            case IK_THINKING_HIGH: thinking_level_str = "high"; break;
        }

        res_t db_res = ik_db_agent_update_provider(repl->shared->db_ctx, repl->current->uuid,
                                                   provider, model_name, thinking_level_str);
        if (is_err(&db_res)) {
            // Log error but don't crash - memory state is authoritative
            yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
            yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "command", "model");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
            ik_log_warn_json(log_doc);  // LCOV_EXCL_LINE
            talloc_free(db_res.err);  // LCOV_EXCL_LINE
        }
    }

    // Build user feedback message
    char *feedback = NULL;

    // Check if model supports thinking
    bool supports_thinking = false;
    ik_model_supports_thinking(model_name, &supports_thinking);

    // Get thinking budget for feedback
    int32_t thinking_budget = 0;
    ik_model_get_thinking_budget(model_name, &thinking_budget);

    // Build thinking level description
    if (thinking_level == IK_THINKING_NONE) {
        feedback = talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: disabled",
                                   provider, model_name);
    } else if (strcmp(provider, "anthropic") == 0 && thinking_budget > 0) {
        // Anthropic: show concrete budget value
        const char *level_name = (thinking_level == IK_THINKING_LOW) ? "low" :
                                 (thinking_level == IK_THINKING_MED) ? "medium" : "high";
        // Calculate budget based on level (from 03-provider-types.md)
        int32_t min_budget = 1024;
        int32_t max_budget = thinking_budget;
        int32_t calculated_budget;
        if (thinking_level == IK_THINKING_LOW) {
            calculated_budget = min_budget + (max_budget - min_budget) / 3;
        } else if (thinking_level == IK_THINKING_MED) {
            calculated_budget = min_budget + (2 * (max_budget - min_budget)) / 3;
        } else {
            calculated_budget = max_budget;
        }
        feedback = talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s (%d tokens)",
                                   provider, model_name, level_name, calculated_budget);
    } else if (strcmp(provider, "google") == 0 && thinking_budget > 0) {
        // Google 2.5 series: show budget
        const char *level_name = (thinking_level == IK_THINKING_LOW) ? "low" :
                                 (thinking_level == IK_THINKING_MED) ? "medium" : "high";
        int32_t min_budget = 512;
        int32_t max_budget = thinking_budget;
        int32_t calculated_budget;
        if (thinking_level == IK_THINKING_LOW) {
            calculated_budget = min_budget + (max_budget - min_budget) / 3;
        } else if (thinking_level == IK_THINKING_MED) {
            calculated_budget = min_budget + (2 * (max_budget - min_budget)) / 3;
        } else {
            calculated_budget = max_budget;
        }
        feedback = talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s (%d tokens)",
                                   provider, model_name, level_name, calculated_budget);
    } else if (strcmp(provider, "openai") == 0) {
        // OpenAI: effort-based
        const char *effort = (thinking_level == IK_THINKING_NONE) ? "none" :
                             (thinking_level == IK_THINKING_LOW) ? "low" :
                             (thinking_level == IK_THINKING_MED) ? "medium" : "high";
        feedback = talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s effort",
                                   provider, model_name, effort);
    } else {
        // Generic or Google 3.x (level-based)
        const char *level_name = (thinking_level == IK_THINKING_LOW) ? "low" :
                                 (thinking_level == IK_THINKING_MED) ? "medium" : "high";
        feedback = talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s level",
                                   provider, model_name, level_name);
    }

    if (!feedback) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    ik_scrollback_append_line(repl->current->scrollback, feedback, strlen(feedback));

    // Warn if user requested thinking on non-thinking model
    if (!supports_thinking && thinking_level != IK_THINKING_NONE) {
        char *warning = talloc_asprintf(ctx, "Warning: Model '%s' does not support thinking/reasoning", model_name);
        if (!warning) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, warning, strlen(warning));
    }

    return OK(NULL);
}

res_t ik_cmd_system(void *ctx, ik_repl_ctx_t *repl, const char *args)
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

res_t cmd_model_parse(void *ctx, const char *input, char **model, char **thinking)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(input != NULL);   // LCOV_EXCL_BR_LINE
    assert(model != NULL);   // LCOV_EXCL_BR_LINE
    assert(thinking != NULL); // LCOV_EXCL_BR_LINE

    // Find slash separator
    const char *slash = strchr(input, '/');

    if (slash == NULL) {
        // No thinking level specified - use current
        *model = talloc_strdup(ctx, input);
        if (!*model) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        *thinking = NULL;
        return OK(NULL);
    }

    // Check for malformed input (trailing slash with no thinking level)
    if (slash[1] == '\0') {
        return ERR(ctx, INVALID_ARG, "Malformed input: trailing '/' with no thinking level");
    }

    // Extract model name (before slash)
    size_t model_len = (size_t)(slash - input);
    if (model_len == 0) {
        return ERR(ctx, INVALID_ARG, "Malformed input: empty model name");
    }

    *model = talloc_strndup(ctx, input, model_len);
    if (!*model) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    // Extract thinking level (after slash)
    *thinking = talloc_strdup(ctx, slash + 1);
    if (!*thinking) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    return OK(NULL);
}
