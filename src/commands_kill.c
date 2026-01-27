/**
 * @file commands_kill.c
 * @brief Kill command handler implementation
 */

#include "commands.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "db/message.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "scrollback_utils.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Collect all descendants of a given agent in depth-first order
static size_t collect_descendants(ik_repl_ctx_t *repl,
                                  const char *uuid,
                                  ik_agent_ctx_t **out,
                                  size_t max)
{
    size_t count = 0;

    // Find children
    for (size_t i = 0; i < repl->agent_count && count < max; i++) {     // LCOV_EXCL_BR_LINE
        if (repl->agents[i]->parent_uuid != NULL &&
            strcmp(repl->agents[i]->parent_uuid, uuid) == 0) {
            // Recurse first (depth-first)
            count += collect_descendants(repl, repl->agents[i]->uuid,
                                         out + count, max - count);

            // Then add this child
            if (count < max) {     // LCOV_EXCL_BR_LINE
                out[count++] = repl->agents[i];
            }
        }
    }

    return count;
}

// Kill an agent and all its descendants with transaction semantics
static res_t cmd_kill_cascade(void *ctx, ik_repl_ctx_t *repl, const char *uuid)
{
    // Begin transaction (Q15)
    res_t res = ik_db_begin(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Collect descendants
    ik_agent_ctx_t *victims[256];
    size_t count = collect_descendants(repl, uuid, victims, 256);

    // Kill descendants (depth-first order)
    for (size_t i = 0; i < count; i++) {
        res = ik_db_agent_mark_dead(repl->shared->db_ctx, victims[i]->uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    // Kill target
    res = ik_db_agent_mark_dead(repl->shared->db_ctx, uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Record cascade kill event (Q20)
    char *metadata_json = talloc_asprintf(ctx,
                                          "{\"killed_by\": \"user\", \"target\": \"%s\", \"cascade\": true, \"count\": %zu}",
                                          uuid,
                                          count + 1);
    if (metadata_json == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    res = ik_db_message_insert(repl->shared->db_ctx,
                               repl->shared->session_id,
                               repl->current->uuid,
                               "agent_killed",
                               NULL,
                               metadata_json);
    talloc_free(metadata_json);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        ik_db_rollback(repl->shared->db_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Commit
    res = ik_db_commit(repl->shared->db_ctx);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Remove from memory (after DB commit succeeds)
    for (size_t i = 0; i < count; i++) {
        res = ik_repl_remove_agent(repl, victims[i]->uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }
    res = ik_repl_remove_agent(repl, uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }

    // Update navigation context after removal
    ik_repl_update_nav_context(repl);

    // Report
    char msg[64];
    int32_t written = snprintf(msg, sizeof(msg), "Killed %zu agents", count + 1);
    if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");     // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, (size_t)written);

    return OK(NULL);
}

res_t ik_cmd_kill(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    (void)ctx;

    // Sync barrier (Q10): wait for pending fork
    while (atomic_load(&repl->shared->fork_pending)) {     // LCOV_EXCL_BR_LINE
        // In unit tests, this will not loop because we control fork_pending manually
        // In production, this would process events while waiting
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};  // 10ms     // LCOV_EXCL_LINE
        nanosleep(&ts, NULL);     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // No args = kill self
    if (args == NULL || args[0] == '\0') {
        if (repl->current->parent_uuid == NULL) {
            char *err_msg = ik_scrollback_format_warning(ctx, "Cannot kill root agent");
            ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
            talloc_free(err_msg);
            return OK(NULL);
        }

        const char *uuid = repl->current->uuid;
        ik_agent_ctx_t *parent = ik_repl_find_agent(repl,
                                                    repl->current->parent_uuid);

        if (parent == NULL) {
            return ERR(ctx, INVALID_ARG, "Parent agent not found");
        }

        // Record kill event in parent's history (Q20)
        char *metadata_json = talloc_asprintf(ctx,
                                              "{\"killed_by\": \"user\", \"target\": \"%s\"}", uuid);
        if (metadata_json == NULL) {  // LCOV_EXCL_BR_LINE
            PANIC("Out of memory");  // LCOV_EXCL_LINE
        }

        res_t res = ik_db_message_insert(repl->shared->db_ctx,
                                         repl->shared->session_id,
                                         parent->uuid,
                                         "agent_killed",
                                         NULL,
                                         metadata_json);
        talloc_free(metadata_json);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        // Mark dead in registry (sets status='dead', ended_at=now)
        res = ik_db_agent_mark_dead(repl->shared->db_ctx, uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        // Switch to parent first (saves state), then remove dead agent
        res = ik_repl_switch_agent(repl, parent);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        res = ik_repl_remove_agent(repl, uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        // Update navigation context after removal
        ik_repl_update_nav_context(repl);

        // Notify
        char msg[64];
        int32_t written = snprintf(msg, sizeof(msg), "Agent %.22s terminated", uuid);
        if (written < 0 || (size_t)written >= sizeof(msg)) {  // LCOV_EXCL_BR_LINE
            PANIC("snprintf failed");  // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(parent->scrollback, msg, (size_t)written);

        return OK(NULL);
    }

    // Handle targeted kill
    // Parse UUID and --cascade flag
    const char *uuid_arg = args;
    bool cascade = false;

    // Check for --cascade flag
    const char *cascade_flag = strstr(args, "--cascade");
    char *uuid_copy = NULL;
    if (cascade_flag != NULL) {
        cascade = true;
        // Extract UUID (everything before --cascade)
        size_t uuid_len = (size_t)(cascade_flag - args);
        // Trim trailing whitespace
        while (uuid_len > 0 && isspace((unsigned char)args[uuid_len - 1])) {     // LCOV_EXCL_BR_LINE
            uuid_len--;
        }
        uuid_copy = talloc_strndup(ctx, args, uuid_len);
        if (!uuid_copy) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");     // LCOV_EXCL_LINE
        }
        uuid_arg = uuid_copy;
    }

    // Find target agent by UUID (partial match allowed)
    ik_agent_ctx_t *target = ik_repl_find_agent(repl, uuid_arg);
    if (target == NULL) {
        char *err_msg;
        if (ik_repl_uuid_ambiguous(repl, uuid_arg)) {     // LCOV_EXCL_BR_LINE
            err_msg = ik_scrollback_format_warning(ctx, "Ambiguous UUID prefix");     // LCOV_EXCL_LINE
        } else {     // LCOV_EXCL_LINE
            err_msg = ik_scrollback_format_warning(ctx, "Agent not found");
        }
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        talloc_free(err_msg);
        return OK(NULL);
    }

    // Check if root
    if (target->parent_uuid == NULL) {
        char *err_msg = ik_scrollback_format_warning(ctx, "Cannot kill root agent");
        ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
        talloc_free(err_msg);
        return OK(NULL);
    }

    // If killing current, use self-kill logic
    if (target == repl->current) {
        return ik_cmd_kill(ctx, repl, NULL);
    }

    const char *target_uuid = target->uuid;

    // If cascade flag is set, use cascade kill
    if (cascade) {
        return cmd_kill_cascade(ctx, repl, target_uuid);
    }

    // Record kill event in current agent's history (Q20)
    char *metadata_json = talloc_asprintf(ctx,
                                          "{\"killed_by\": \"user\", \"target\": \"%s\"}", target_uuid);
    if (metadata_json == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    res_t res = ik_db_message_insert(repl->shared->db_ctx,
                                     repl->shared->session_id,
                                     repl->current->uuid,
                                     "agent_killed",
                                     NULL,
                                     metadata_json);
    talloc_free(metadata_json);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Mark dead in registry (sets status='dead', ended_at=now)
    res = ik_db_agent_mark_dead(repl->shared->db_ctx, target_uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Remove from agents array and free agent context
    res = ik_repl_remove_agent(repl, target_uuid);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Update navigation context after removal
    ik_repl_update_nav_context(repl);

    // Notify
    char msg[64];
    int32_t written = snprintf(msg, sizeof(msg), "Agent %.22s terminated", target_uuid);
    if (written < 0 || (size_t)written >= sizeof(msg)) {  // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");  // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, (size_t)written);

    return OK(NULL);
}
