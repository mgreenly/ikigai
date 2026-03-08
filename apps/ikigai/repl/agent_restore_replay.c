// Agent restoration replay helpers
#include "apps/ikigai/repl/agent_restore_replay.h"

#include "apps/ikigai/event_render.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "shared/wrapper_json.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

void ik_agent_restore_populate_conversation(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];
        if (ik_msg_is_conversation_kind(msg->kind) && !msg->interrupted) {
            ik_message_t *provider_msg = NULL;
            res_t res = ik_message_from_db_msg(agent, msg, &provider_msg);
            if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - Parse error tested in message tests
                yyjson_mut_doc *log_doc = ik_log_create();     // LCOV_EXCL_LINE
                yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, root, "event", "message_parse_failed");     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, root, "agent_uuid", agent->uuid);     // LCOV_EXCL_LINE
                yyjson_mut_obj_add_str(log_doc, root, "error", error_message(res.err));     // LCOV_EXCL_LINE
                ik_logger_warn_json(logger, log_doc);     // LCOV_EXCL_LINE
                continue;     // LCOV_EXCL_LINE
            }
            if (provider_msg != NULL) {  // NULL for system messages (handled via request->system_prompt)
                if (provider_msg->role == IK_ROLE_USER && agent->token_cache != NULL) {
                    ik_token_cache_add_turn(agent->token_cache);
                }
                res = ik_agent_add_message(agent, provider_msg);
                if (is_err(&res)) {     // LCOV_EXCL_BR_LINE - OOM tested in agent tests
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
}

// Helper: count conversation messages before a given message ID in the replay context
// Returns the number of non-interrupted conversation messages that appear before target_id.
static size_t replay_conv_count_before_id(ik_replay_context_t *replay_ctx, int64_t target_id)
{
    size_t conv_count = 0;
    for (size_t i = 0; i < replay_ctx->count; i++) {
        ik_msg_t *m = replay_ctx->messages[i];
        if (m->id == target_id) {
            return conv_count;
        }
        if (ik_msg_is_conversation_kind(m->kind) && !m->interrupted) {
            conv_count++;
        }
    }
    return conv_count;  // target not found - return full count
}

// Helper: trim skills loaded at or after target_conv_count (rewind side effect)
static void replay_trim_skills_at(ik_agent_ctx_t *agent, size_t target_conv_count)
{
    while (agent->loaded_skill_count > 0 &&
           agent->loaded_skills[agent->loaded_skill_count - 1]->load_position >= target_conv_count) {
        agent->loaded_skill_count--;
        talloc_free(agent->loaded_skills[agent->loaded_skill_count]);
    }
}

// Helper: trim skillset catalog entries loaded at or after target_conv_count (rewind side effect)
static void replay_trim_catalog_at(ik_agent_ctx_t *agent, size_t target_conv_count)
{
    while (agent->skillset_catalog_count > 0 &&
           agent->skillset_catalog[agent->skillset_catalog_count - 1]->load_position >= target_conv_count) {
        agent->skillset_catalog_count--;
        talloc_free(agent->skillset_catalog[agent->skillset_catalog_count]);
    }
}

// Helper: parse target_message_id from rewind event data_json
static int64_t parse_rewind_target_id(const char *data_json)
{
    if (data_json == NULL) return -1;

    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    if (doc == NULL) return -1;

    yyjson_val *root = yyjson_doc_get_root_(doc);
    int64_t target_id = -1;

    if (root != NULL) {
        yyjson_val *id_val = yyjson_obj_get_(root, "target_message_id");
        if (id_val != NULL && yyjson_is_int(id_val)) {
            target_id = yyjson_get_int(id_val);
        }
    }

    yyjson_doc_free(doc);
    return target_id;
}

void ik_agent_restore_populate_scrollback(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    // Track conversation message count as we replay events in order.
    // This mirrors agent->message_count at each point in time during the original session,
    // used as load_position for skill_load events and trim threshold for rewind.
    size_t running_conv_count = 0;

    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];

        // Replay command side effects (e.g., /model sets agent->provider)
        if (msg->kind != NULL && strcmp(msg->kind, "command") == 0) {
            ik_agent_restore_replay_command_effects(agent, msg, logger);
        }

        // Replay fork event side effects (extract pinned_paths from child fork event)
        if (msg->kind != NULL && strcmp(msg->kind, "fork") == 0) {
            ik_agent_restore_replay_command_effects(agent, msg, logger);
        }

        // Replay skill_load: add to loaded_skills[] at current conversation position
        if (msg->kind != NULL && strcmp(msg->kind, "skill_load") == 0) {
            ik_agent_restore_replay_skill_load(agent, msg, running_conv_count);
        }

        // Replay skill_unload: remove from loaded_skills[]
        if (msg->kind != NULL && strcmp(msg->kind, "skill_unload") == 0) {
            ik_agent_restore_replay_skill_unload(agent, msg);
        }

        // Replay skillset: populate skillset_catalog[] at current conversation position
        if (msg->kind != NULL && strcmp(msg->kind, "skillset") == 0) {
            ik_agent_restore_replay_skillset(agent, msg, running_conv_count);
        }

        // Replay rewind: trim skills and catalog entries loaded after the rewind target position
        if (msg->kind != NULL && strcmp(msg->kind, "rewind") == 0) {
            int64_t target_id = parse_rewind_target_id(msg->data_json);
            if (target_id >= 0) {
                size_t target_conv_count = replay_conv_count_before_id(replay_ctx, target_id);
                replay_trim_skills_at(agent, target_conv_count);
                replay_trim_catalog_at(agent, target_conv_count);
            }
        }

        // Track conversation message count for load_position
        if (ik_msg_is_conversation_kind(msg->kind) && !msg->interrupted) {
            running_conv_count++;
        }

        res_t res = ik_event_render(agent->scrollback, msg->kind, msg->content, msg->data_json, msg->interrupted);
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

void ik_agent_restore_marks(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    if (replay_ctx->mark_stack.count > 0) {     // LCOV_EXCL_BR_LINE - TODO: Mark replay not yet implemented in agent_replay
        unsigned int mark_count = (unsigned int)replay_ctx->mark_stack.count;     // LCOV_EXCL_LINE
        agent->marks = talloc_zero_array(agent, ik_mark_t *, mark_count);     // LCOV_EXCL_LINE
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
