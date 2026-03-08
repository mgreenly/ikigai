/**
 * @file commands_basic.c
 * @brief Basic REPL command implementations (clear, help, debug)
 */

#include "apps/ikigai/commands_basic.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/agent_replay.h"
#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/summary.h"
#include "apps/ikigai/summary_worker.h"
#include "shared/logger.h"
#include "shared/panic.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/*
 * Dispatch a background session summary for the current epoch (non-blocking).
 * Called at the top of /clear.  poll() stores result to DB on completion.
 * Skips silently if epoch is empty, no provider/model, or IDs are invalid.
 */
static void clear_generate_session_summary(void *ctx, ik_agent_ctx_t *agent,
                                           ik_db_ctx_t *db)
{
    /* Skip if no model or provider configured */
    if (agent->model == NULL) {
        return;
    }
    if (agent->provider == NULL || agent->provider[0] == '\0') {
        return;
    }

    void *tmp = talloc_new(ctx);

    /* Find the last clear event to establish epoch start boundary */
    int64_t last_clear_id = 0;
    res_t find_res = ik_agent_find_clear(db, tmp, agent->uuid, 0, &last_clear_id);
    if (is_err(&find_res)) {
        talloc_free(find_res.err);
        last_clear_id = 0;
    }

    /* Query all messages in the current epoch from the database */
    ik_replay_range_t range = {
        .agent_uuid = agent->uuid,
        .start_id = last_clear_id,
        .end_id = 0,
    };
    ik_msg_t **epoch_msgs = NULL;
    size_t epoch_count = 0;
    res_t query_res = ik_agent_query_range(db, tmp, &range, &epoch_msgs, &epoch_count);
    if (is_err(&query_res)) {
        talloc_free(query_res.err);
        talloc_free(tmp);
        return;
    }

    /* Count conversation-kind messages; skip if epoch is empty */
    size_t conv_count = 0;
    for (size_t i = 0; i < epoch_count; i++) {
        if (ik_msg_is_conversation_kind(epoch_msgs[i]->kind)) {
            conv_count++;
        }
    }
    if (conv_count == 0 || epoch_count == 0) {
        talloc_free(tmp);
        return;
    }

    int64_t start_msg_id = epoch_msgs[0]->id;
    int64_t end_msg_id = epoch_msgs[epoch_count - 1]->id;

    /* Require valid DB IDs (0 means no DB row, e.g. in unit tests) */
    if (start_msg_id <= 0 || end_msg_id <= 0) {
        talloc_free(tmp);
        return;
    }

    /* Build malloc stubs: strdup kind/content so tmp can be freed now. */
    ik_msg_t *stubs = malloc(epoch_count * sizeof(ik_msg_t));
    ik_msg_t **ptrs  = malloc(epoch_count * sizeof(ik_msg_t *));
    if (stubs == NULL || ptrs == NULL) {          // LCOV_EXCL_BR_LINE
        free(stubs);                              // LCOV_EXCL_LINE
        free(ptrs);                               // LCOV_EXCL_LINE
        talloc_free(tmp);                         // LCOV_EXCL_LINE
        return;                                   // LCOV_EXCL_LINE
    }

    size_t stub_count = 0;
    bool oom = false;
    for (size_t i = 0; i < epoch_count; i++) {
        stubs[stub_count].id = epoch_msgs[i]->id;
        stubs[stub_count].kind    = strdup(epoch_msgs[i]->kind    ? epoch_msgs[i]->kind    : "");
        stubs[stub_count].content = strdup(epoch_msgs[i]->content ? epoch_msgs[i]->content : "");
        stubs[stub_count].data_json   = NULL;
        stubs[stub_count].interrupted = false;
        if (stubs[stub_count].kind == NULL || stubs[stub_count].content == NULL) { // LCOV_EXCL_BR_LINE
            oom = true;                   // LCOV_EXCL_LINE
            free(stubs[stub_count].kind); // LCOV_EXCL_LINE
            free(stubs[stub_count].content); // LCOV_EXCL_LINE
            break;                        // LCOV_EXCL_LINE
        }
        ptrs[stub_count] = &stubs[stub_count];
        stub_count++;
    }

    if (oom) {                                                                      // LCOV_EXCL_BR_LINE
        for (size_t i = 0; i < stub_count; i++) {                                  // LCOV_EXCL_LINE
            free(stubs[i].kind);                                                    // LCOV_EXCL_LINE
            free(stubs[i].content);                                                 // LCOV_EXCL_LINE
        }                                                                           // LCOV_EXCL_LINE
        free(stubs);                                                                // LCOV_EXCL_LINE
        free(ptrs);                                                                 // LCOV_EXCL_LINE
        talloc_free(tmp);                                                           // LCOV_EXCL_LINE
        return;                                                                     // LCOV_EXCL_LINE
    }

    talloc_free(tmp);  /* stubs own the strings; DB msgs no longer needed */

    /* Ownership of stubs/ptrs transfers to dispatch; freed by poll() on join. */
    ik_session_summary_dispatch(agent, agent->model,
                                (ik_msg_t * const *)ptrs, stub_count,
                                stubs, ptrs,
                                IK_SUMMARY_PREVIOUS_SESSION_MAX_TOKENS,
                                start_msg_id, end_msg_id);
}

res_t ik_cmd_clear(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)ctx;      // Used only in assert (compiled out in release builds)
    (void)args;     // Unused for /clear

    // Generate and store previous-session summary before clearing state
    if (repl->shared->db_ctx != NULL) {
        clear_generate_session_summary(ctx, repl->current, repl->shared->db_ctx);
    }

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

    // Reset token cache: clear all cached values, empty turn array, reset context window
    if (repl->current->token_cache != NULL) {
        ik_token_cache_reset(repl->current->token_cache);
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

    // Clear loaded skills
    if (repl->current->loaded_skills != NULL) {  // LCOV_EXCL_BR_LINE
        for (size_t i = 0; i < repl->current->loaded_skill_count; i++) {
            talloc_free(repl->current->loaded_skills[i]);
        }
        repl->current->loaded_skill_count = 0;
    }

    // Reset recent summary (current epoch ends with /clear)
    if (repl->current->recent_summary != NULL) {
        talloc_free(repl->current->recent_summary);
        repl->current->recent_summary = NULL;
        repl->current->recent_summary_tokens = 0;
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
        // Use effective system prompt with fallback chain
        char *db_prompt = NULL;
        res_t db_prompt_res = ik_agent_get_effective_system_prompt(repl->current, &db_prompt);
        if (is_ok(&db_prompt_res) && db_prompt != NULL) {  // LCOV_EXCL_BR_LINE
            res_t system_res = ik_db_message_insert(
                repl->shared->db_ctx,
                repl->shared->session_id,
                repl->current->uuid,
                "system",
                db_prompt,
                "{}"
                );
            talloc_free(db_prompt);
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
    // Use effective system prompt with fallback chain:
    // 1. Pinned files (if any)
    // 2. $IKIGAI_DATA_DIR/system/prompt.md (if exists)
    // 3. cfg->openai_system_message (config fallback)
    char *effective_prompt = NULL;
    res_t prompt_res = ik_agent_get_effective_system_prompt(repl->current, &effective_prompt);
    if (is_err(&prompt_res)) {  // LCOV_EXCL_BR_LINE
        return prompt_res;  // LCOV_EXCL_LINE
    }
    if (effective_prompt != NULL) {  // LCOV_EXCL_BR_LINE
        res_t render_res = ik_event_render(
            repl->current->scrollback,
            "system",
            effective_prompt,
            "{}",
            false
            );
        talloc_free(effective_prompt);
        if (is_err(&render_res)) {  // LCOV_EXCL_BR_LINE
            return render_res;  // LCOV_EXCL_LINE
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

res_t ik_cmd_summary(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    ik_agent_ctx_t *agent = repl->current;

    // Recent summary header
    res_t res = ik_scrollback_append_line(agent->scrollback,
                                          "\u2500\u2500 Recent Summary \u2500\u2500",
                                          strlen("\u2500\u2500 Recent Summary \u2500\u2500"));
    if (is_err(&res)) return res;  // LCOV_EXCL_LINE

    // Recent summary text or "(none)"
    const char *recent_text = (agent->recent_summary != NULL) ? agent->recent_summary : "(none)";
    res = ik_scrollback_append_line(agent->scrollback, recent_text, strlen(recent_text));
    if (is_err(&res)) return res;  // LCOV_EXCL_LINE

    // Blank separator
    res = ik_scrollback_append_line(agent->scrollback, "", 0);
    if (is_err(&res)) return res;  // LCOV_EXCL_LINE

    // Previous sessions header
    size_t n = agent->session_summary_count;
    char *sessions_header = talloc_asprintf(ctx, "\u2500\u2500 Previous Sessions (%zu/5) \u2500\u2500", n);
    if (sessions_header == NULL) PANIC("OOM");  // LCOV_EXCL_BR_LINE
    res = ik_scrollback_append_line(agent->scrollback, sessions_header, strlen(sessions_header));
    talloc_free(sessions_header);
    if (is_err(&res)) return res;  // LCOV_EXCL_LINE

    // Each session summary
    for (size_t i = 0; i < n; i++) {
        char *line = talloc_asprintf(ctx, "[%zu] %s", i + 1, agent->session_summaries[i]->summary);
        if (line == NULL) PANIC("OOM");  // LCOV_EXCL_BR_LINE
        res = ik_scrollback_append_line(agent->scrollback, line, strlen(line));
        talloc_free(line);
        if (is_err(&res)) return res;  // LCOV_EXCL_LINE
    }

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
