// Agent restoration on startup
#include "apps/ikigai/debug_log.h"
#include "apps/ikigai/repl/agent_restore.h"
#include "apps/ikigai/repl/agent_restore_replay.h"
#include "apps/ikigai/repl/agent_restore_replay_toolset.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/agent_replay.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "shared/panic.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
// Sort agents by created_at (oldest first)
static int compare_agents_by_created_at(const void *a, const void *b)
{
    const ik_db_agent_row_t *agent_a = *(const ik_db_agent_row_t *const *)a;
    const ik_db_agent_row_t *agent_b = *(const ik_db_agent_row_t *const *)b;
    if (agent_a->created_at < agent_b->created_at) return -1;
    if (agent_a->created_at > agent_b->created_at) return 1;     // LCOV_EXCL_BR_LINE
    return 0;
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

    // Write synthetic pin command for default system prompt
    if (repl->shared->paths != NULL) {     // LCOV_EXCL_BR_LINE
        const char *data_dir = ik_paths_get_data_dir(repl->shared->paths);
        char *system_prompt_path = talloc_asprintf(repl, "%s/system/prompt.md", data_dir);
        if (system_prompt_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *pin_data = talloc_asprintf(
            repl, "{\"command\":\"pin\",\"args\":\"%s\"}", system_prompt_path
            );
        if (pin_data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        res_t pin_res = ik_db_message_insert(
            db_ctx, repl->shared->session_id, repl->current->uuid,
            "command", NULL, pin_data
            );
        talloc_free(system_prompt_path);
        talloc_free(pin_data);
        if (is_err(&pin_res)) {     // LCOV_EXCL_BR_LINE
            yyjson_mut_doc *pin_log = ik_log_create();     // LCOV_EXCL_LINE
            yyjson_mut_val *pin_root = yyjson_mut_doc_get_root(pin_log);     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(pin_log, pin_root, "event", "fresh_install_pin_failed");     // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(pin_log, pin_root, "error", error_message(pin_res.err));     // LCOV_EXCL_LINE
            ik_logger_warn_json(repl->shared->logger, pin_log);     // LCOV_EXCL_LINE
        }
    }

    yyjson_mut_doc *fresh_log = ik_log_create();
    yyjson_mut_val *fresh_root = yyjson_mut_doc_get_root(fresh_log);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_str(fresh_log, fresh_root, "event", "fresh_install_complete");     // LCOV_EXCL_BR_LINE
    ik_logger_debug_json(repl->shared->logger, fresh_log);
}

// Helper: Restore Agent 0 (root agent)
static void restore_agent_zero(
    ik_repl_ctx_t *repl,
    ik_db_ctx_t *db_ctx,
    TALLOC_CTX *tmp,
    ik_db_agent_row_t *agent_row)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(agent_row != NULL);  // LCOV_EXCL_BR_LINE

    const char *uuid = agent_row->uuid;
    ik_agent_ctx_t *agent = repl->current;
    agent->repl = repl;

    // Restore provider configuration from DB row
    ik_agent_restore_from_row(agent, agent_row);

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
    ik_agent_restore_populate_conversation(agent, replay_ctx, repl->shared->logger);
    ik_agent_restore_populate_scrollback(agent, replay_ctx, repl->shared->logger);
    ik_agent_restore_marks(agent, replay_ctx);
    DEBUG_LOG("[agent_restore] uuid=%s messages=%zu marks=%zu",
              agent->uuid, replay_ctx->count, agent->mark_count);

    // Replay pins (independent of clear boundaries)
    res = ik_agent_replay_pins(db_ctx, agent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *pin_log = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *pin_root = yyjson_mut_doc_get_root(pin_log);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(pin_log, pin_root, "event", "agent0_pin_replay_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(pin_log, pin_root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, pin_log);     // LCOV_EXCL_LINE
    }

    // Replay toolset (independent of clear boundaries)
    res = ik_agent_replay_toolset(db_ctx, agent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *toolset_log = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *toolset_root = yyjson_mut_doc_get_root(toolset_log);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(toolset_log, toolset_root, "event", "agent0_toolset_replay_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(toolset_log, toolset_root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, toolset_log);     // LCOV_EXCL_LINE
    }

    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_str(log_doc, root, "event", "agent0_restored");     // LCOV_EXCL_BR_LINE
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

    // Restore provider configuration from DB row
    ik_agent_restore_from_row(agent, agent_row);

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
    ik_agent_restore_populate_conversation(agent, replay_ctx, repl->shared->logger);
    ik_agent_restore_populate_scrollback(agent, replay_ctx, repl->shared->logger);
    ik_agent_restore_marks(agent, replay_ctx);
    DEBUG_LOG("[agent_restore] uuid=%s messages=%zu marks=%zu",
              agent->uuid, replay_ctx->count, agent->mark_count);

    // Replay pins (independent of clear boundaries)
    res = ik_agent_replay_pins(db_ctx, agent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *pin_log = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *pin_root = yyjson_mut_doc_get_root(pin_log);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(pin_log, pin_root, "event", "agent_pin_replay_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(pin_log, pin_root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(pin_log, pin_root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, pin_log);     // LCOV_EXCL_LINE
    }

    // Replay toolset (independent of clear boundaries)
    res = ik_agent_replay_toolset(db_ctx, agent);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *toolset_log = ik_log_create();     // LCOV_EXCL_LINE
        yyjson_mut_val *toolset_root = yyjson_mut_doc_get_root(toolset_log);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(toolset_log, toolset_root, "event", "agent_toolset_replay_failed");     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(toolset_log, toolset_root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(toolset_log, toolset_root, "error", error_message(res.err));     // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, toolset_log);     // LCOV_EXCL_LINE
    }

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

    yyjson_mut_doc *log_doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_str(log_doc, root, "event", "agent_restored");     // LCOV_EXCL_BR_LINE
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
            restore_agent_zero(repl, db_ctx, tmp, agents[i]);
        } else {
            restore_child_agent(repl, db_ctx, agents[i]);
        }
    }

    // Update navigation context for current agent after restoration
    ik_repl_update_nav_context(repl);

    talloc_free(tmp);
    return OK(NULL);
}
