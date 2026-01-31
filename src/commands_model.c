/**
 * @file commands_model.c
 * @brief Model configuration command implementations (model, system)
 */

#include "commands_model.h"

#include "agent.h"
#include "db/agent.h"
#include "logger.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "shared.h"
#include "wrapper.h"

// Include provider.h after other headers to avoid type conflicts
#include "providers/provider.h"
#include "providers/anthropic/thinking.h"
#include "providers/google/thinking.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

/**
 * Helper to build feedback message for model switch
 */
static char *cmd_model_build_feedback(TALLOC_CTX *ctx, const char *provider,
                                      const char *model_name, ik_thinking_level_t thinking_level)
{
    const char *level_name = (thinking_level == IK_THINKING_NONE) ? "none" :
                             (thinking_level == IK_THINKING_LOW)  ? "low" :
                             (thinking_level == IK_THINKING_MED)  ? "med" : "high";

    if (strcmp(provider, "anthropic") == 0) {
        int32_t budget = ik_anthropic_thinking_budget(model_name, thinking_level);
        if (budget > 0) {
            return talloc_asprintf(ctx, "Switched to Anthropic %s\n  Thinking: %s (%d tokens)",
                                   model_name, level_name, budget);
        }
        return talloc_asprintf(ctx, "Switched to Anthropic %s\n  Thinking: %s",
                               model_name, level_name);
    } else if (strcmp(provider, "google") == 0) {
        int32_t budget = ik_google_thinking_budget(model_name, thinking_level);
        if (budget >= 0) {
            return talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s (%d tokens)",
                                   provider, model_name, level_name, budget);
        }
        return talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s",
                               provider, model_name, level_name);
    } else if (strcmp(provider, "openai") == 0) {
        return talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s effort",
                               provider, model_name, level_name);
    } else {
        return talloc_asprintf(ctx, "Switched to %s %s\n  Thinking: %s",
                               provider, model_name, level_name);
    }
}

res_t ik_cmd_model(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    // Check if model name provided
    if (args == NULL) {     // LCOV_EXCL_BR_LINE
        char *msg = ik_scrollback_format_warning(ctx, "Model name required (usage: /model <name>[/thinking_level])");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return ERR(ctx, INVALID_ARG, "Model name required");
    }

    // Check if an LLM request is currently active
    if (atomic_load(&repl->current->state) == IK_AGENT_STATE_WAITING_FOR_LLM) {
        char *msg = ik_scrollback_format_warning(ctx, "Cannot switch models during active request");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
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
            char *msg = talloc_asprintf(ctx, "Error: Invalid thinking level '%s' (must be: none, low, med, high)", thinking_str);
            if (!msg) PANIC("OOM");   // LCOV_EXCL_BR_LINE
            ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
            return ERR(ctx, INVALID_ARG, "Invalid thinking level '%s'", thinking_str);
        }
    }

    // Validate Google models BEFORE switching
    if (strcmp(provider, "google") != 0) goto skip_google_validation;

    // Validate thinking level compatibility
    res_t validate_res = ik_google_validate_thinking(ctx, model_name, thinking_level);
    if (is_err(&validate_res)) {
        char *msg = talloc_asprintf(ctx, "Error: %s", error_message(validate_res.err));
        if (!msg) PANIC("OOM");   // LCOV_EXCL_BR_LINE
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return validate_res;
    }

    // For Gemini 2.5, verify model is in BUDGET_TABLE
    if (ik_google_model_series(model_name) != IK_GEMINI_2_5) goto skip_google_validation;
    if (ik_google_thinking_budget(model_name, thinking_level) != -1) goto skip_google_validation;

    char *msg = talloc_asprintf(ctx, "Error: Unknown Gemini 2.5 model '%s'", model_name);
    if (!msg) PANIC("OOM");   // LCOV_EXCL_BR_LINE
    ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    return ERR(ctx, INVALID_ARG, "Unknown Gemini 2.5 model '%s'", model_name);

skip_google_validation:

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
        switch (thinking_level) { // LCOV_EXCL_BR_LINE
            case IK_THINKING_NONE: thinking_level_str = "none"; break;
            case IK_THINKING_LOW:  thinking_level_str = "low";  break;
            case IK_THINKING_MED:  thinking_level_str = "med";  break;
            case IK_THINKING_HIGH: thinking_level_str = "high"; break;
            default: // LCOV_EXCL_LINE
                PANIC("Invalid thinking level"); // LCOV_EXCL_LINE
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
    char *feedback = cmd_model_build_feedback(ctx, provider, model_name, thinking_level);
    if (!feedback) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    ik_scrollback_append_line(repl->current->scrollback, feedback, strlen(feedback));

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
