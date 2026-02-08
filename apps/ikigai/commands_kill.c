/**
 * @file commands_kill.c
 * @brief Kill command handler implementation
 */

#include "apps/ikigai/commands.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/notify.h"
#include "shared/panic.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#include "shared/poison.h"
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

    // Check if we're in a transaction (NOTIFY requires autocommit)
    bool can_notify = (PQtransactionStatus(repl->shared->db_ctx->conn) == PQTRANS_IDLE);

    // Mark as dead in memory and fire NOTIFY for each victim
    ik_agent_ctx_t *target_agent = ik_repl_find_agent(repl, uuid);
    bool killing_current = (target_agent == repl->current);

    if (target_agent != NULL) {     // LCOV_EXCL_BR_LINE
        target_agent->dead = true;

        // Render kill event to target's scrollback
        char msg[64];
        int32_t written = snprintf(msg, sizeof(msg), "Agent killed (cascade, %zu total)", count + 1);
        if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
            PANIC("snprintf failed");     // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(target_agent->scrollback, msg, (size_t)written);

        // Fire NOTIFY to wake waiting parent
        if (can_notify && target_agent->parent_uuid != NULL) {     // LCOV_EXCL_BR_LINE
            char channel[128];
            written = snprintf(channel, sizeof(channel), "agent_event_%s", target_agent->parent_uuid);
            if (written < 0 || (size_t)written >= sizeof(channel)) {     // LCOV_EXCL_BR_LINE
                PANIC("snprintf failed");     // LCOV_EXCL_LINE
            }
            res = ik_db_notify(repl->shared->db_ctx, channel, "dead");
            if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
                return res;     // LCOV_EXCL_LINE
            }
        }
    }

    for (size_t i = 0; i < count; i++) {
        victims[i]->dead = true;

        // Render kill event to descendant's scrollback
        char msg[64];
        int32_t written = snprintf(msg, sizeof(msg), "Agent killed (cascade)");
        if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
            PANIC("snprintf failed");     // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(victims[i]->scrollback, msg, (size_t)written);

        // Fire NOTIFY to wake waiting parent
        if (can_notify && victims[i]->parent_uuid != NULL) {     // LCOV_EXCL_BR_LINE
            char channel[128];
            written = snprintf(channel, sizeof(channel), "agent_event_%s", victims[i]->parent_uuid);
            if (written < 0 || (size_t)written >= sizeof(channel)) {     // LCOV_EXCL_BR_LINE
                PANIC("snprintf failed");     // LCOV_EXCL_LINE
            }
            res = ik_db_notify(repl->shared->db_ctx, channel, "dead");
            if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
                return res;     // LCOV_EXCL_LINE
            }
        }
    }

    // If killing current agent, switch to parent
    if (killing_current && target_agent != NULL && target_agent->parent_uuid != NULL) {     // LCOV_EXCL_BR_LINE
        ik_agent_ctx_t *parent = ik_repl_find_agent(repl, target_agent->parent_uuid);
        if (parent == NULL) {     // LCOV_EXCL_BR_LINE
            return ERR(ctx, INVALID_ARG, "Parent agent not found");     // LCOV_EXCL_LINE
        }
        res = ik_repl_switch_agent(repl, parent);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }

        // Report to parent's scrollback
        char msg[64];
        int32_t written = snprintf(msg, sizeof(msg), "Agent %.22s terminated", uuid);
        if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
            PANIC("snprintf failed");     // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(parent->scrollback, msg, (size_t)written);
    } else {
        // Report to current agent's scrollback
        char msg[64];
        int32_t written = snprintf(msg, sizeof(msg), "Killed %zu agents", count + 1);
        if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
            PANIC("snprintf failed");     // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, (size_t)written);
    }

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
        // Self-kill always uses cascade logic
        return cmd_kill_cascade(ctx, repl, uuid);
    }

    // Handle targeted kill - parse UUID
    const char *uuid_arg = args;

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

    // Always use cascade kill (Run 2 design)
    return cmd_kill_cascade(ctx, repl, target->uuid);
}
