// Agent restoration on startup
#include "agent_restore.h"

#include "../db/agent.h"
#include "../db/agent_replay.h"
#include "../db/message.h"
#include "../error.h"
#include "../event_render.h"
#include "../logger.h"
#include "../msg.h"
#include "../openai/client.h"
#include "../repl.h"
#include "../shared.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Sort agents by created_at ascending (oldest first)
static int compare_agents_by_created_at(const void *a, const void *b)
{
    const ik_db_agent_row_t *agent_a = *(const ik_db_agent_row_t *const *)a;
    const ik_db_agent_row_t *agent_b = *(const ik_db_agent_row_t *const *)b;
    if (agent_a->created_at < agent_b->created_at) return -1;
    if (agent_a->created_at > agent_b->created_at) return 1;     // LCOV_EXCL_BR_LINE
    return 0;
}

// Helper: Populate agent conversation from replay context
static void populate_conversation(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];
        if (ik_msg_is_conversation_kind(msg->kind)) {
            ik_msg_t *conv_msg = talloc_steal(agent->conversation, msg);
            res_t res = ik_openai_conversation_add_msg(agent->conversation, conv_msg);
            if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - OOM/API error tested in openai tests
                yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
                yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, root, "event", "conversation_add_failed");     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
                ik_logger_warn_json(logger, log_doc);     // LCOV_EXCL_LINE
            }
        }
    }
}

// Helper: Populate agent scrollback from replay context
static void populate_scrollback(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];
        res_t res = ik_event_render(agent->scrollback, msg->kind, msg->content, msg->data_json);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - Render error tested in event_render tests
            yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
            yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "event", "scrollback_render_failed");     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
            ik_logger_warn_json(logger, log_doc);     // LCOV_EXCL_LINE
        }
    }
}

// Helper: Restore mark stack from replay context
static void restore_marks(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    if (replay_ctx->mark_stack.count > 0) {     // LCOV_EXCL_BR_LINE - TODO: Mark replay not yet implemented in agent_replay
        unsigned int mark_count = (unsigned int)replay_ctx->mark_stack.count;     // LCOV_EXCL_LINE
        agent->marks = talloc_array(agent, ik_mark_t *, mark_count);     // LCOV_EXCL_LINE
        if (agent->marks != NULL) {     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
            for (size_t j = 0; j < replay_ctx->mark_stack.count; j++) {     // LCOV_EXCL_LINE
                ik_replay_mark_t *replay_mark = &replay_ctx->mark_stack.marks[j];     // LCOV_EXCL_LINE
                ik_mark_t *mark = talloc_zero(agent, ik_mark_t);     // LCOV_EXCL_LINE
                if (mark != NULL) {     // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
                    mark->message_index = replay_mark->context_idx;     // LCOV_EXCL_LINE
                    mark->label = replay_mark->label     // LCOV_EXCL_LINE
                        ? talloc_strdup(mark, replay_mark->label)     // LCOV_EXCL_LINE
                        : NULL;     // LCOV_EXCL_LINE
                    mark->timestamp = NULL;     // LCOV_EXCL_LINE
                    agent->marks[agent->mark_count++] = mark;     // LCOV_EXCL_LINE
                }
            }
        }
    }
}

// Helper: Handle fresh install by writing initial events
static void handle_fresh_install(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE

    // Write clear event to establish session start
    res_t clear_res = ik_db_message_insert(
        db_ctx, repl->shared->session_id, repl->current->uuid, "clear", NULL, "{}"
    );
    if (is_err(&clear_res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *clear_log = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *clear_root = yyjson_mut_doc_get_root(clear_log);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(clear_log, clear_root, "event", "fresh_install_clear_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(clear_log, clear_root, "error", error_message(clear_res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, clear_log);     // LCOV_EXCL_LINE
    }

    // Write system message if configured
    ik_cfg_t *cfg = repl->shared->cfg;
    if (cfg != NULL && cfg->openai_system_message != NULL) {     // LCOV_EXCL_BR_LINE
        res_t system_res = ik_db_message_insert(
            db_ctx, repl->shared->session_id, repl->current->uuid,
            "system", cfg->openai_system_message, "{}"
        );
        if (is_err(&system_res)) {     // LCOV_EXCL_BR_LINE
            yyjson_mut_doc *sys_log = ik_log_create();     // LCOV_EXCL_LINE
            yyjson_mut_val *sys_root = yyjson_mut_doc_get_root(sys_log);     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(sys_log, sys_root, "event", "fresh_install_system_failed");     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(sys_log, sys_root, "error", error_message(system_res.err));     // LCOV_EXCL_LINE
            ik_logger_warn_json(repl->shared->logger, sys_log);     // LCOV_EXCL_LINE
        } else {
            res_t render_res = ik_event_render(
                repl->current->scrollback, "system", cfg->openai_system_message, "{}"
            );
            if (is_err(&render_res)) {     // LCOV_EXCL_BR_LINE
                yyjson_mut_doc *render_log = ik_log_create();     // LCOV_EXCL_LINE
                yyjson_mut_val *render_root = yyjson_mut_doc_get_root(render_log);     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(render_log, render_root, "event", "fresh_install_render_failed");     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(render_log, render_root, "error", error_message(render_res.err));     // LCOV_EXCL_LINE
                ik_logger_warn_json(repl->shared->logger, render_log);     // LCOV_EXCL_LINE
            }

            ik_msg_t *sys_msg = talloc_zero(repl->current->conversation, ik_msg_t);
            if (sys_msg != NULL) {     // LCOV_EXCL_BR_LINE
                sys_msg->id = 0;
                sys_msg->kind = talloc_strdup(sys_msg, "system");
                sys_msg->content = talloc_strdup(sys_msg, cfg->openai_system_message);
                sys_msg->data_json = talloc_strdup(sys_msg, "{}");
                res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, sys_msg);
                if (is_err(&add_res)) {     // LCOV_EXCL_BR_LINE
                    yyjson_mut_doc *conv_log = ik_log_create();     // LCOV_EXCL_LINE
                    yyjson_mut_val *conv_root = yyjson_mut_doc_get_root(conv_log);     // LCOV_EXCL_LINE
                    yyjson_mut_obj_add_str(conv_log, conv_root, "event", "fresh_install_conversation_failed");     // LCOV_EXCL_LINE
                    yyjson_mut_obj_add_str(conv_log, conv_root, "error", error_message(add_res.err));     // LCOV_EXCL_LINE
                    ik_logger_warn_json(repl->shared->logger, conv_log);     // LCOV_EXCL_LINE
                }
            }
        }
    }

    yyjson_mut_doc *fresh_log = ik_log_create();
    yyjson_mut_val *fresh_root = yyjson_mut_doc_get_root(fresh_log);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_str(fresh_log, fresh_root, "event", "fresh_install_complete");
    ik_logger_debug_json(repl->shared->logger, fresh_log);
}

// Helper: Restore Agent 0 (root agent)
static void restore_agent_zero(
    ik_repl_ctx_t *repl,
    ik_db_ctx_t *db_ctx,
    TALLOC_CTX *tmp,
    const char *uuid)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);       // LCOV_EXCL_BR_LINE

    ik_agent_ctx_t *agent = repl->current;
    agent->repl = repl;

    // Replay history
    ik_replay_context_t *replay_ctx = NULL;
    res_t res = ik_agent_replay_history(db_ctx, tmp, uuid, &replay_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - DB failure tested separately
        yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "event", "agent0_replay_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, log_doc);     // LCOV_EXCL_LINE
        return;     // LCOV_EXCL_LINE
    }

    // Populate conversation, scrollback, and marks
    populate_conversation(agent, replay_ctx, repl->shared->logger);
    populate_scrollback(agent, replay_ctx, repl->shared->logger);
    restore_marks(agent, replay_ctx);

    // Log success
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_str(log_doc, root, "event", "agent0_restored");
    yyjson_mut_obj_add_uint(log_doc, root, "message_count", replay_ctx->count);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_uint(log_doc, root, "mark_count", agent->mark_count);     // LCOV_EXCL_BR_LINE
    ik_logger_debug_json(repl->shared->logger, log_doc);

    // Handle fresh install if no history
    if (replay_ctx->count == 0) {
        handle_fresh_install(repl, db_ctx);
    }
}

// Helper: Restore a child agent
static void restore_child_agent(
    ik_repl_ctx_t *repl,
    ik_db_ctx_t *db_ctx,
    ik_db_agent_row_t *agent_row)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent_row != NULL);  // LCOV_EXCL_BR_LINE

    // Restore agent context from DB row
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(repl, repl->shared, agent_row, &agent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - OOM/allocation error tested separately
        yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "event", "agent_restore_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent_row->uuid);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, log_doc);     // LCOV_EXCL_LINE
        (void)ik_db_agent_mark_dead(db_ctx, agent_row->uuid);     // LCOV_EXCL_LINE
        return;     // LCOV_EXCL_LINE
    }

    agent->repl = repl;

    // Replay history
    ik_replay_context_t *replay_ctx = NULL;
    res = ik_agent_replay_history(db_ctx, agent, agent->uuid, &replay_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - DB failure tested separately
        yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "event", "agent_replay_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, log_doc);     // LCOV_EXCL_LINE
        (void)ik_db_agent_mark_dead(db_ctx, agent->uuid);     // LCOV_EXCL_LINE
        talloc_free(agent);     // LCOV_EXCL_LINE
        return;     // LCOV_EXCL_LINE
    }

    // Populate conversation, scrollback, and marks
    populate_conversation(agent, replay_ctx, repl->shared->logger);
    populate_scrollback(agent, replay_ctx, repl->shared->logger);
    restore_marks(agent, replay_ctx);

    // Add to repl->agents[] array
    res = ik_repl_add_agent(repl, agent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - OOM error tested separately
        yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "event", "agent_add_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, log_doc);     // LCOV_EXCL_LINE
        (void)ik_db_agent_mark_dead(db_ctx, agent->uuid);     // LCOV_EXCL_LINE
        talloc_free(agent);     // LCOV_EXCL_LINE
        return;     // LCOV_EXCL_LINE
    }

    // Log success
    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_str(log_doc, root, "event", "agent_restored");
    yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_uint(log_doc, root, "message_count", replay_ctx->count);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_uint(log_doc, root, "mark_count", agent->mark_count);     // LCOV_EXCL_BR_LINE
    ik_logger_debug_json(repl->shared->logger, log_doc);
}

res_t ik_repl_restore_agents(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) {  // LCOV_EXCL_BR_LINE
        return ERR(repl, OUT_OF_MEMORY, "Out of memory");  // LCOV_EXCL_LINE
    }

    // 1. Query all running agents from database
    ik_db_agent_row_t **agents = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_list_running(db_ctx, tmp, &agents, &count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // 2. Sort by created_at (oldest first) - parents before children
    if (count > 1) {
        qsort(agents, count, sizeof(ik_db_agent_row_t *), compare_agents_by_created_at);
    }

    // 3. For each agent, restore full state:
    for (size_t i = 0; i < count; i++) {
        if (agents[i]->parent_uuid == NULL) {
            restore_agent_zero(repl, db_ctx, tmp, agents[i]->uuid);
        } else {
            restore_child_agent(repl, db_ctx, agents[i]);
        }
    }

    // Update navigation context for current agent after restoration
    ik_repl_update_nav_context(repl);

    talloc_free(tmp);
    return OK(NULL);
}
