/**
 * @file commands_reap.c
 * @brief Reap command handler implementation - removes dead agents from memory
 */

#include "apps/ikigai/commands.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "shared/panic.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
// Collect all descendants of a given agent in depth-first order (dead or alive)
static size_t collect_all_descendants(ik_repl_ctx_t *repl,
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
            count += collect_all_descendants(repl, repl->agents[i]->uuid,
                                             out + count, max - count);

            // Then add this child
            if (count < max) {     // LCOV_EXCL_BR_LINE
                out[count++] = repl->agents[i];
            }
        }
    }

    return count;
}

// Find first living (non-dead) agent
static ik_agent_ctx_t *find_first_living_agent(ik_repl_ctx_t *repl)
{
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (!repl->agents[i]->dead) {
            return repl->agents[i];
        }
    }
    return NULL;
}

// Check if agent or any ancestor is being reaped
static bool is_affected_by_reap(ik_agent_ctx_t *agent,
                                ik_agent_ctx_t **victims,
                                size_t victim_count)
{
    // Check if agent itself is in victim list
    for (size_t i = 0; i < victim_count; i++) {
        if (victims[i] == agent) {
            return true;
        }
    }

    // Check if any ancestor is in victim list (walk up parent chain)
    const char *check_uuid = agent->parent_uuid;
    while (check_uuid != NULL) {
        for (size_t i = 0; i < victim_count; i++) {
            if (strcmp(victims[i]->uuid, check_uuid) == 0) {
                return true;
            }
        }
        // Find parent in array to continue walking up
        ik_agent_ctx_t *parent = NULL;
        for (size_t i = 0; i < agent->repl->agent_count; i++) {
            if (strcmp(agent->repl->agents[i]->uuid, check_uuid) == 0) {
                parent = agent->repl->agents[i];
                break;
            }
        }
        if (parent == NULL) {
            break;  // Parent not found in array
        }
        check_uuid = parent->parent_uuid;
    }

    return false;
}

res_t ik_cmd_reap(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    ik_agent_ctx_t *victims[256];
    size_t victim_count = 0;

    // Parse mode
    if (args == NULL || args[0] == '\0') {
        // Bulk mode: collect all dead agents
        for (size_t i = 0; i < repl->agent_count && victim_count < 256; i++) {
            if (repl->agents[i]->dead) {
                victims[victim_count++] = repl->agents[i];
            }
        }

        if (victim_count == 0) {
            const char *msg = "No dead agents to reap";
            ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
            return OK(NULL);
        }
    } else {
        // Targeted mode: UUID specified
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

        // Verify target is dead
        if (!target->dead) {
            char *err_msg = ik_scrollback_format_warning(ctx, "Agent is not dead");
            ik_scrollback_append_line(repl->current->scrollback, err_msg, strlen(err_msg));
            talloc_free(err_msg);
            return OK(NULL);
        }

        // Collect target and all descendants (dead or alive)
        victims[victim_count++] = target;
        victim_count += collect_all_descendants(repl, target->uuid,
                                                victims + victim_count,
                                                256 - victim_count);
    }

    // Check if current agent is affected by reap (either being reaped or descendant of reaped agent)
    bool need_switch = is_affected_by_reap(repl->current, victims, victim_count);

    // Switch to first living agent if needed
    if (need_switch) {
        ik_agent_ctx_t *first_living = find_first_living_agent(repl);
        if (first_living == NULL) {
            const char *err = "Cannot reap: no living agents remain";
            ik_scrollback_append_line(repl->current->scrollback, err, strlen(err));
            return OK(NULL);
        }

        // Switch before reaping (so we're not viewing a dead agent)
        res_t res = ik_repl_switch_agent(repl, first_living);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }
    }

    // Remove all victims from repl->agents[] and free their memory
    // Process in reverse order to avoid index shifting issues
    for (size_t i = 0; i < victim_count; i++) {
        // Find agent in array (might have shifted)
        bool found = false;
        for (size_t j = 0; j < repl->agent_count; j++) {
            if (repl->agents[j] == victims[i]) {
                found = true;
                break;
            }
        }

        if (!found) {     // LCOV_EXCL_BR_LINE
            continue;  // Already removed (ancestor-descendant scenario)     // LCOV_EXCL_LINE
        }

        // Remove from array
        res_t res = ik_repl_remove_agent(repl, victims[i]->uuid);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            return res;     // LCOV_EXCL_LINE
        }

        // Mark as reaped in DB (preserve row for audit trail and foreign keys)
        res_t db_res = ik_db_agent_mark_reaped(repl->shared->db_ctx, victims[i]->uuid);
        if (is_err(&db_res)) {     // LCOV_EXCL_BR_LINE
            return db_res;     // LCOV_EXCL_LINE
        }

        // Free memory
        talloc_free(victims[i]);
    }

    // Report count to current agent's scrollback
    char msg[64];
    int32_t written = snprintf(msg, sizeof(msg), "Reaped %zu agents", victim_count);
    if (written < 0 || (size_t)written >= sizeof(msg)) {     // LCOV_EXCL_BR_LINE
        PANIC("snprintf failed");     // LCOV_EXCL_LINE
    }
    ik_scrollback_append_line(repl->current->scrollback, msg, (size_t)written);

    return OK(NULL);
}
