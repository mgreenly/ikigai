/**
 * @file session_restore.c
 * @brief Session restoration logic for Model B (continuous sessions)
 */

#include "session_restore.h"

#include "../db/message.h"
#include "../db/replay.h"
#include "../db/session.h"
#include "../error.h"
#include "../event_render.h"
#include "../msg.h"
#include "../openai/client.h"
#include "../panic.h"
#include "../repl.h"
#include "../scrollback.h"
#include "../wrapper.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

// NOTE: When returning errors after talloc_free(tmp), we must first
// reparent the error to repl via talloc_steal(). See fix.md for details
// on this use-after-free bug pattern.
res_t ik_repl_restore_session(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx, ik_cfg_t *cfg)
{
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    assert(db_ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);      // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = talloc_new(repl);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Check for active session
    int64_t session_id = 0;
    res_t res = ik_db_session_get_active(db_ctx, &session_id);
    if (is_err(&res)) {
        talloc_free(tmp);
        return res;
    }

    if (session_id > 0) {
        // Existing session path: load and replay messages
        repl->current_session_id = session_id;

        // Load messages from database
        res_t load_res = ik_db_messages_load(tmp, db_ctx, session_id);
        if (is_err(&load_res)) {
            talloc_steal(repl, load_res.err);  // Reparent error before freeing tmp
            talloc_free(tmp);
            return load_res;
        }

        // Get the replay context
        ik_replay_context_t *replay_ctx = load_res.ok;

        // Rebuild mark stack from replay context
        if (replay_ctx->mark_stack.count > 0) {
            repl->marks = talloc_array(repl, ik_mark_t *, (unsigned int)replay_ctx->mark_stack.count);
            if (repl->marks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            for (size_t i = 0; i < replay_ctx->mark_stack.count; i++) {
                ik_replay_mark_t *replay_mark = &replay_ctx->mark_stack.marks[i];

                // Create mark structure
                ik_mark_t *mark = talloc_zero(repl->marks, ik_mark_t);
                if (mark == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

                // Set message index from context_idx
                mark->message_index = replay_mark->context_idx;

                // Copy label if present
                if (replay_mark->label != NULL) {
                    mark->label = talloc_strdup(mark, replay_mark->label);
                    if (mark->label == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                } else {
                    mark->label = NULL;
                }

                // Set timestamp to NULL (we don't persist timestamps yet)
                mark->timestamp = NULL;

                repl->marks[i] = mark;
            }

            repl->mark_count = replay_ctx->mark_stack.count;
        }

        // Populate scrollback with replayed messages using event renderer
        for (size_t i = 0; i < replay_ctx->count; i++) {
            ik_message_t *msg = replay_ctx->messages[i];

            // Use universal event renderer for consistent display
            res_t render_res = ik_event_render(
                repl->scrollback,
                msg->kind,
                msg->content,
                msg->data_json
            );
            if (is_err(&render_res)) {
                talloc_free(tmp);
                return render_res;
            }
        }

        // Rebuild conversation from replay context for LLM context
        for (size_t i = 0; i < replay_ctx->count; i++) {
            ik_message_t *db_msg = replay_ctx->messages[i];

            // Convert DB format to canonical format
            res_t msg_res = ik_msg_from_db_(tmp, db_msg);
            if (is_err(&msg_res)) {
                talloc_steal(repl, msg_res.err);  // Reparent error before freeing tmp
                talloc_free(tmp);
                return msg_res;
            }

            // Add to conversation if not skipped (NULL means skip)
            ik_msg_t *msg = msg_res.ok;
            if (msg != NULL) {
                res_t add_res = ik_openai_conversation_add_msg_(repl->conversation, msg);
                if (is_err(&add_res)) {
                    talloc_free(tmp);
                    return add_res;
                }
            }
        }

        talloc_free(tmp);
        return OK(repl);
    } else {
        // New session path: create session and write initial events
        res_t create_res = ik_db_session_create(db_ctx, &session_id);
        if (is_err(&create_res)) {
            talloc_free(tmp);
            return create_res;
        }

        repl->current_session_id = session_id;

        // Write initial clear event
        res_t clear_res = ik_db_message_insert(db_ctx, session_id, "clear", NULL, "{}");
        if (is_err(&clear_res)) {
            talloc_free(tmp);
            return clear_res;
        }

        // Write system message if configured
        if (cfg->openai_system_message != NULL) {
            res_t system_res = ik_db_message_insert(
                db_ctx,
                session_id,
                "system",
                cfg->openai_system_message,
                "{}"
            );
            if (is_err(&system_res)) {
                talloc_free(tmp);
                return system_res;
            }

            // Add system message to scrollback using event renderer
            res_t render_res = ik_event_render(
                repl->scrollback,
                "system",
                cfg->openai_system_message,
                "{}"
            );
            if (is_err(&render_res)) {
                talloc_free(tmp);
                return render_res;
            }
        }

        talloc_free(tmp);
        return OK(repl);
    }
}
