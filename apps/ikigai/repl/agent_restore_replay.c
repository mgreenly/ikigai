// Agent restoration replay helpers
#include "apps/ikigai/repl/agent_restore_replay.h"

#include "apps/ikigai/event_render.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_scheduler.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "shared/wrapper_json.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

// ---------------------------------------------------------------------------
// Parallel batch replay helpers
// ---------------------------------------------------------------------------

// Extract a string field from a JSON object string.
static char *replay_extract_str(TALLOC_CTX *ctx, const char *data_json, const char *key)
{
    if (data_json == NULL) return NULL;
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    if (doc == NULL) return NULL;
    yyjson_val *root = yyjson_doc_get_root_(doc);
    char *result = NULL;
    yyjson_val *val = yyjson_obj_get_(root, key);
    if (val != NULL && yyjson_is_str(val)) {
        const char *s = yyjson_get_str_(val);
        if (s != NULL) result = talloc_strdup(ctx, s);
    }
    yyjson_doc_free(doc);
    return result;
}

// Extract a bool field from a JSON object string.
static bool replay_extract_bool(const char *data_json, const char *key)
{
    if (data_json == NULL) return false;
    yyjson_doc *doc = yyjson_read(data_json, strlen(data_json), 0);
    if (doc == NULL) return false;
    yyjson_val *root = yyjson_doc_get_root_(doc);
    bool result = false;
    yyjson_val *val = yyjson_obj_get_(root, key);
    if (val != NULL && yyjson_is_bool(val)) {
        result = yyjson_get_bool(val);
    }
    yyjson_doc_free(doc);
    return result;
}

// Count consecutive tool_call+tool_result pairs with the given batch_id starting at start_idx.
// Returns the total number of messages consumed (pair_count * 2).
static size_t count_batch_messages(ik_replay_context_t *replay_ctx, size_t start_idx,
                                   const char *batch_id)
{
    size_t count = 0;
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) return 0;
    for (size_t i = start_idx; i + 1 < replay_ctx->count; i += 2) {
        ik_msg_t *tc_msg = replay_ctx->messages[i];
        ik_msg_t *tr_msg = replay_ctx->messages[i + 1];
        if (tc_msg->kind == NULL || strcmp(tc_msg->kind, "tool_call") != 0) break;
        if (tr_msg->kind == NULL || strcmp(tr_msg->kind, "tool_result") != 0) break;
        char *bid = replay_extract_str(tmp, tc_msg->data_json, "batch_id");
        if (bid == NULL || strcmp(bid, batch_id) != 0) break;
        count += 2;
    }
    talloc_free(tmp);
    return count;
}

// Replay a parallel batch using a dry-run scheduler to emit status display lines.
// start_idx: index of the first tool_call in the batch.
// batch_count: total number of messages (tool_call+tool_result pairs * 2).
static void replay_parallel_batch(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx,
                                   size_t start_idx, size_t batch_count)
{
    size_t pair_count = batch_count / 2;
    if (pair_count == 0) return;

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) return;

    ik_tool_scheduler_t *sched = ik_tool_scheduler_create(tmp, agent);
    sched->replay_mode   = true;
    sched->stream_complete = true;

    const char **entry_outputs = talloc_zero_array(tmp, const char *, (unsigned int)pair_count);
    bool *entry_success        = talloc_zero_array(tmp, bool, (unsigned int)pair_count);
    if (entry_outputs == NULL || entry_success == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp); // LCOV_EXCL_LINE
        return; // LCOV_EXCL_LINE
    }

    for (size_t i = 0; i < pair_count; i++) {
        ik_msg_t *tc_msg = replay_ctx->messages[start_idx + i * 2];
        ik_msg_t *tr_msg = replay_ctx->messages[start_idx + i * 2 + 1];

        const char *tc_id   = replay_extract_str(tmp, tc_msg->data_json, "tool_call_id");
        const char *tc_name = replay_extract_str(tmp, tc_msg->data_json, "tool_name");
        const char *tc_args = replay_extract_str(tmp, tc_msg->data_json, "tool_args");
        if (tc_id   == NULL) tc_id   = talloc_strdup(tmp, "");
        if (tc_name == NULL) tc_name = talloc_strdup(tmp, "unknown");
        if (tc_args == NULL) tc_args = talloc_strdup(tmp, "{}");

        ik_tool_call_t *tc = ik_tool_call_create(tmp, tc_id, tc_name, tc_args);
        res_t res = ik_tool_scheduler_add(sched, tc);
        if (is_err(&res)) { // LCOV_EXCL_BR_LINE
            talloc_free(tmp); // LCOV_EXCL_LINE
            return; // LCOV_EXCL_LINE
        }

        entry_outputs[i] = replay_extract_str(tmp, tr_msg->data_json, "output");
        if (entry_outputs[i] == NULL) entry_outputs[i] = "{}";
        entry_success[i] = replay_extract_bool(tr_msg->data_json, "success");
    }

    ik_tool_scheduler_begin(sched);

    for (int32_t i = 0; i < sched->count; i++) {
        if (ik_schedule_is_terminal(sched->entries[i].status)) continue;
        if (entry_success[(size_t)i]) {
            char *result = talloc_strdup(sched, entry_outputs[(size_t)i]);
            ik_tool_scheduler_on_complete(sched, i, result);
        } else {
            ik_tool_scheduler_on_error(sched, i, entry_outputs[(size_t)i]);
        }
    }

    ik_tool_scheduler_destroy(sched);
    talloc_free(tmp);
}

// If msg is the start of a parallel batch, replay it and return the number of messages consumed.
// Also updates *conv_count_inout for the remaining batch messages.
// Returns 0 if msg is not a parallel batch start.
static size_t try_replay_batch(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx,
                                ik_msg_t *msg, size_t j, size_t *conv_count_inout)
{
    if (msg->kind == NULL || strcmp(msg->kind, "tool_call") != 0 || msg->interrupted) {
        return 0;
    }
    TALLOC_CTX *tmp = talloc_new(NULL);
    char *batch_id = replay_extract_str(tmp, msg->data_json, "batch_id");
    if (batch_id == NULL) {
        talloc_free(tmp);
        return 0;
    }
    size_t batch_count = count_batch_messages(replay_ctx, j, batch_id);
    if (batch_count == 0) {
        talloc_free(tmp);
        return 0;
    }
    for (size_t k = j + 1; k < j + batch_count; k++) {
        ik_msg_t *bm = replay_ctx->messages[k];
        if (ik_msg_is_conversation_kind(bm->kind) && !bm->interrupted) {
            (*conv_count_inout)++;
        }
    }
    replay_parallel_batch(agent, replay_ctx, j, batch_count);
    talloc_free(tmp);
    return batch_count;
}

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

        // Detect parallel batch: tool_call with batch_id triggers dry-run scheduler replay.
        size_t batch_consumed = try_replay_batch(agent, replay_ctx, msg, j, &running_conv_count);
        if (batch_consumed > 0) {
            j += batch_consumed - 1;
            continue;
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
