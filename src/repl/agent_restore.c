// Agent restoration on startup
#include "agent_restore.h"

#include "../db/agent.h"
#include "../db/agent_replay.h"
#include "../error.h"
#include "../event_render.h"
#include "../logger.h"
#include "../openai/client.h"
#include "../repl.h"
#include "../shared.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Check if a message kind should be included in conversation
// Returns true for user, assistant, system, tool_call, tool_result
// Returns false for clear, mark, rewind (control events, not conversation)
static bool is_conversation_kind(const char *kind)
{
    if (kind == NULL) return false;

    // Control events that don't go in conversation
    if (strcmp(kind, "clear") == 0) return false;
    if (strcmp(kind, "mark") == 0) return false;
    if (strcmp(kind, "rewind") == 0) return false;

    // Everything else is conversation content
    return true;
}

// Comparison function for qsort - sort by created_at ascending (oldest first)
static int compare_agents_by_created_at(const void *a, const void *b)
{
    // qsort passes pointers to array elements; array elements are ik_db_agent_row_t*
    // So we need to cast void* to pointer-to-element-type (ik_db_agent_row_t**)
    // then dereference to get the actual element (ik_db_agent_row_t*)
    const ik_db_agent_row_t *const *ptr_a = (const ik_db_agent_row_t *const *)a;
    const ik_db_agent_row_t *const *ptr_b = (const ik_db_agent_row_t *const *)b;
    const ik_db_agent_row_t *agent_a = *ptr_a;
    const ik_db_agent_row_t *agent_b = *ptr_b;

    // Sort ascending: older agents first (lower created_at values)
    if (agent_a->created_at < agent_b->created_at) return -1;
    if (agent_a->created_at > agent_b->created_at) return 1;
    return 0;
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
    if (is_err(&res)) {
        talloc_free(tmp);
        return res;
    }

    // 2. Sort by created_at (oldest first) - parents before children
    if (count > 1) {
        qsort(agents, count, sizeof(ik_db_agent_row_t *), compare_agents_by_created_at);
    }

    // 3. For each agent, restore full state:
    for (size_t i = 0; i < count; i++) {
        // Skip Agent 0 (already created in repl_init)
        // Agent 0 is the root agent with no parent
        if (agents[i]->parent_uuid == NULL) {
            continue;
        }

        // --- Step 1: Restore agent context from DB row ---
        ik_agent_ctx_t *agent = NULL;
        res = ik_agent_restore(repl, repl->shared, agents[i], &agent);
        if (is_err(&res)) {
            // Log warning, mark as dead, continue with other agents
            yyjson_mut_doc *log_doc = ik_log_create();
            yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
            yyjson_mut_obj_add_str(log_doc, root, "event", "agent_restore_failed");
            yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agents[i]->uuid);
            yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));
            ik_logger_warn_json(repl->shared->logger, log_doc);
            (void)ik_db_agent_mark_dead(db_ctx, agents[i]->uuid);
            continue;
        }

        // --- Step 2: Replay history to get message context ---
        ik_replay_context_t *replay_ctx = NULL;
        res = ik_agent_replay_history(db_ctx, agent, agent->uuid, &replay_ctx);
        if (is_err(&res)) {
            yyjson_mut_doc *log_doc = ik_log_create();
            yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
            yyjson_mut_obj_add_str(log_doc, root, "event", "agent_replay_failed");
            yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);
            yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));
            ik_logger_warn_json(repl->shared->logger, log_doc);
            (void)ik_db_agent_mark_dead(db_ctx, agent->uuid);
            talloc_free(agent);
            continue;
        }

        // --- Step 3: Populate conversation (filter non-conversation kinds) ---
        for (size_t j = 0; j < replay_ctx->count; j++) {
            ik_msg_t *msg = replay_ctx->messages[j];
            if (is_conversation_kind(msg->kind)) {
                // Steal message from replay_ctx to agent's conversation
                ik_msg_t *conv_msg = talloc_steal(agent->conversation, msg);
                res = ik_openai_conversation_add_msg(agent->conversation, conv_msg);
                if (is_err(&res)) {
                    yyjson_mut_doc *log_doc = ik_log_create();
                    yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
                    yyjson_mut_obj_add_str(log_doc, root, "event", "conversation_add_failed");
                    yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);
                    yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));
                    ik_logger_warn_json(repl->shared->logger, log_doc);
                    // Continue anyway - partial conversation is better than none
                }
            }
        }

        // --- Step 4: Populate scrollback via event render ---
        for (size_t j = 0; j < replay_ctx->count; j++) {
            ik_msg_t *msg = replay_ctx->messages[j];
            res = ik_event_render(agent->scrollback, msg->kind, msg->content, msg->data_json);
            if (is_err(&res)) {
                yyjson_mut_doc *log_doc = ik_log_create();
                yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
                yyjson_mut_obj_add_str(log_doc, root, "event", "scrollback_render_failed");
                yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);
                yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));
                ik_logger_warn_json(repl->shared->logger, log_doc);
                // Continue anyway - partial scrollback is better than none
            }
        }

        // --- Step 5: Restore marks from replay context ---
        if (replay_ctx->mark_stack.count > 0) {
            unsigned int mark_count = (unsigned int)replay_ctx->mark_stack.count;
            agent->marks = talloc_array(agent, ik_mark_t *, mark_count);
            if (agent->marks != NULL) {
                for (size_t j = 0; j < replay_ctx->mark_stack.count; j++) {
                    ik_replay_mark_t *replay_mark = &replay_ctx->mark_stack.marks[j];
                    ik_mark_t *mark = talloc_zero(agent, ik_mark_t);
                    if (mark != NULL) {
                        mark->message_index = replay_mark->context_idx;
                        mark->label = replay_mark->label
                            ? talloc_strdup(mark, replay_mark->label)
                            : NULL;
                        mark->timestamp = NULL;  // Not preserved in replay
                        agent->marks[agent->mark_count++] = mark;
                    }
                }
            }
        }

        // --- Step 6: Add to repl->agents[] array ---
        res = ik_repl_add_agent(repl, agent);
        if (is_err(&res)) {
            yyjson_mut_doc *log_doc = ik_log_create();
            yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
            yyjson_mut_obj_add_str(log_doc, root, "event", "agent_add_failed");
            yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);
            yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));
            ik_logger_warn_json(repl->shared->logger, log_doc);
            (void)ik_db_agent_mark_dead(db_ctx, agent->uuid);
            talloc_free(agent);
            continue;
        }

        // Log success for debugging
        yyjson_mut_doc *log_doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);
        yyjson_mut_obj_add_str(log_doc, root, "event", "agent_restored");
        yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);
        yyjson_mut_obj_add_uint(log_doc, root, "message_count", replay_ctx->count);
        yyjson_mut_obj_add_uint(log_doc, root, "mark_count", agent->mark_count);
        ik_logger_debug_json(repl->shared->logger, log_doc);
    }

    talloc_free(tmp);
    return OK(NULL);
}
