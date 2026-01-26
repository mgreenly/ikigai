// Agent restoration replay helpers
#include "agent_restore_replay.h"

#include "../error.h"
#include "../event_render.h"
#include "../logger.h"
#include "../message.h"
#include "../msg.h"
#include "../providers/provider.h"
#include "../wrapper_json.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

// Forward declarations
static void replay_command_effects(ik_agent_ctx_t *agent, ik_msg_t *msg, ik_logger_t *logger);
static void replay_fork_event(ik_agent_ctx_t *agent, yyjson_val *root);
static void replay_model_command(ik_agent_ctx_t *agent, const char *args, ik_logger_t *logger);
static void replay_pin_command(ik_agent_ctx_t *agent, const char *args);
static void replay_unpin_command(ik_agent_ctx_t *agent, const char *args);

void ik_agent_restore_populate_conversation(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];
        if (ik_msg_is_conversation_kind(msg->kind)) {
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

void ik_agent_restore_populate_scrollback(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger)
{
    assert(agent != NULL);          // LCOV_EXCL_BR_LINE
    assert(replay_ctx != NULL);     // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < replay_ctx->count; j++) {
        ik_msg_t *msg = replay_ctx->messages[j];

        // Replay command side effects (e.g., /model sets agent->provider)
        if (msg->kind != NULL && strcmp(msg->kind, "command") == 0) {
            replay_command_effects(agent, msg, logger);
        }

        // Replay fork event side effects (extract pinned_paths from child fork event)
        if (msg->kind != NULL && strcmp(msg->kind, "fork") == 0) {
            replay_command_effects(agent, msg, logger);
        }

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

void ik_agent_restore_marks(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx)
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

// Helper: Replay fork event - extract pinned_paths from child fork event
static void replay_fork_event(ik_agent_ctx_t *agent, yyjson_val *root)
{
    yyjson_val *role_val = yyjson_obj_get_(root, "role");
    const char *role = yyjson_get_str(role_val);

    if (role == NULL || strcmp(role, "child") != 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    yyjson_val *pins_val = yyjson_obj_get_(root, "pinned_paths");
    if (!yyjson_is_arr(pins_val)) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    size_t arr_size = yyjson_arr_size(pins_val);

    if (agent->pinned_paths != NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(agent->pinned_paths);     // LCOV_EXCL_LINE
        agent->pinned_paths = NULL;     // LCOV_EXCL_LINE
        agent->pinned_count = 0;     // LCOV_EXCL_LINE
    }

    if (arr_size == 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    agent->pinned_paths = talloc_array(agent, char *, (unsigned int)arr_size);
    if (agent->pinned_paths == NULL) return;  // LCOV_EXCL_LINE

    size_t idx = 0;
    size_t max_idx = 0;
    yyjson_val *path_val = NULL;
    yyjson_arr_foreach(pins_val, idx, max_idx, path_val) {     // LCOV_EXCL_BR_LINE
        const char *path_str = yyjson_get_str(path_val);
        if (path_str != NULL) {     // LCOV_EXCL_BR_LINE
            agent->pinned_paths[agent->pinned_count] = talloc_strdup(agent, path_str);
            if (agent->pinned_paths[agent->pinned_count] == NULL) return;  // LCOV_EXCL_LINE
            agent->pinned_count++;
        }
    }
}

// Helper: Replay model command
static void replay_model_command(ik_agent_ctx_t *agent, const char *args, ik_logger_t *logger)
{
    char *model_copy = talloc_strdup(agent, args);
    if (model_copy == NULL) return;  // LCOV_EXCL_LINE

    char *slash = strchr(model_copy, '/');
    if (slash != NULL) {
        *slash = '\0';
    }

    const char *provider = ik_infer_provider(model_copy);

    if (agent->provider != NULL) {
        talloc_free(agent->provider);
    }
    if (agent->model != NULL) {
        talloc_free(agent->model);
    }

    agent->provider = talloc_strdup(agent, provider ? provider : "openai");  // LCOV_EXCL_BR_LINE
    agent->model = talloc_strdup(agent, model_copy);  // LCOV_EXCL_BR_LINE

    if (agent->provider_instance != NULL) {
        talloc_free(agent->provider_instance);
        agent->provider_instance = NULL;
    }

    yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_BR_LINE
    if (log_doc != NULL) {  // LCOV_EXCL_BR_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "replay_model_command");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "provider", agent->provider);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "model", agent->model);  // LCOV_EXCL_LINE
        ik_logger_info_json(logger, log_doc);  // LCOV_EXCL_LINE
    }  // LCOV_EXCL_LINE

    talloc_free(model_copy);
}

// Helper: Replay pin command
static void replay_pin_command(ik_agent_ctx_t *agent, const char *args)
{
    for (size_t i = 0; i < agent->pinned_count; i++) {
        if (strcmp(agent->pinned_paths[i], args) == 0) {     // LCOV_EXCL_BR_LINE
            return;     // LCOV_EXCL_LINE
        }
    }

    char **new_paths = talloc_realloc(agent, agent->pinned_paths,
                                       char *, (unsigned int)(agent->pinned_count + 1));
    if (new_paths == NULL) return;  // LCOV_EXCL_LINE
    agent->pinned_paths = new_paths;

    agent->pinned_paths[agent->pinned_count] = talloc_strdup(agent, args);
    if (agent->pinned_paths[agent->pinned_count] == NULL) return;  // LCOV_EXCL_LINE
    agent->pinned_count++;
}

// Helper: Replay unpin command
static void replay_unpin_command(ik_agent_ctx_t *agent, const char *args)
{
    int64_t found_index = -1;
    for (size_t i = 0; i < agent->pinned_count; i++) {
        if (strcmp(agent->pinned_paths[i], args) == 0) {
            found_index = (int64_t)i;
            break;
        }
    }

    if (found_index < 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    talloc_free(agent->pinned_paths[found_index]);
    for (size_t i = (size_t)found_index; i < agent->pinned_count - 1; i++) {
        agent->pinned_paths[i] = agent->pinned_paths[i + 1];
    }
    agent->pinned_count--;

    if (agent->pinned_count == 0) {     // LCOV_EXCL_BR_LINE
        talloc_free(agent->pinned_paths);     // LCOV_EXCL_LINE
        agent->pinned_paths = NULL;     // LCOV_EXCL_LINE
    } else {
        char **new_paths = talloc_realloc(agent, agent->pinned_paths,
                                           char *, (unsigned int)agent->pinned_count);
        if (new_paths != NULL) {  // LCOV_EXCL_BR_LINE
            agent->pinned_paths = new_paths;
        }
    }
}

// Replay command side effects
// Some commands (like /model) have side effects that need to be re-applied
// when replaying history to restore agent state.
static void replay_command_effects(ik_agent_ctx_t *agent, ik_msg_t *msg, ik_logger_t *logger)
{
    assert(agent != NULL);      // LCOV_EXCL_BR_LINE
    assert(msg != NULL);        // LCOV_EXCL_BR_LINE

    if (msg->data_json == NULL) {
        return;
    }

    // Parse data_json
    yyjson_doc *doc = yyjson_read(msg->data_json, strlen(msg->data_json), 0);
    if (doc == NULL) {
        return;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL) {
        yyjson_doc_free(doc);
        return;
    }

    // Check if this is a fork event
    if (msg->kind != NULL && strcmp(msg->kind, "fork") == 0) {
        replay_fork_event(agent, root);
        yyjson_doc_free(doc);
        return;
    }

    // Handle command events
    yyjson_val *cmd_val = yyjson_obj_get_(root, "command");
    yyjson_val *args_val = yyjson_obj_get_(root, "args");

    const char *cmd_name = yyjson_get_str(cmd_val);
    const char *args = yyjson_get_str(args_val);  // May be NULL

    if (cmd_name == NULL) {
        yyjson_doc_free(doc);
        return;
    }

    // Handle /model command
    if (strcmp(cmd_name, "model") == 0 && args != NULL) {
        replay_model_command(agent, args, logger);
    }

    // Handle /pin command
    if (strcmp(cmd_name, "pin") == 0 && args != NULL) {
        replay_pin_command(agent, args);
    }

    // Handle /unpin command
    if (strcmp(cmd_name, "unpin") == 0 && args != NULL) {
        replay_unpin_command(agent, args);
    }

    yyjson_doc_free(doc);
}
