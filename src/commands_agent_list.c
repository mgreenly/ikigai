/**
 * @file commands_agent_list.c
 * @brief /agents command implementation - displays agent hierarchy
 */

#include "commands.h"

#include "agent.h"
#include "db/agent.h"
#include "db/connection.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>


#include "poison.h"
// /agents command implementation - displays agent hierarchy tree
res_t ik_cmd_agents(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE
    (void)args;

    TALLOC_CTX *tmp_ctx = talloc_new(ctx);
    if (tmp_ctx == NULL) {     // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    // Add header
    const char *header = "Agent Hierarchy:";
    res_t res = ik_scrollback_append_line(repl->current->scrollback, header, strlen(header));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Add blank line
    res = ik_scrollback_append_line(repl->current->scrollback, "", 0);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Get all running agents from database
    ik_db_agent_row_t **all_agents = NULL;
    size_t all_count = 0;
    res = ik_db_agent_list_running(repl->shared->db_ctx, tmp_ctx, &all_agents, &all_count);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Build tree by iterating through agents in depth-first order
    // We'll use a simple approach: process each depth level iteratively
    size_t running_count = 0;
    size_t dead_count = 0;

    // Queue for breadth-first traversal (stores indices and depths)
    size_t *queue_idx = talloc_zero_array(tmp_ctx, size_t, (uint32_t)all_count);
    size_t *queue_depth = talloc_zero_array(tmp_ctx, size_t, (uint32_t)all_count);
    if (queue_idx == NULL || queue_depth == NULL) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE
    size_t queue_start = 0;
    size_t queue_end = 0;

    // Find and queue root agents (parent_uuid = NULL)
    for (size_t i = 0; i < all_count; i++) {     // LCOV_EXCL_BR_LINE
        if (all_agents[i]->parent_uuid == NULL) {     // LCOV_EXCL_BR_LINE
            queue_idx[queue_end] = i;
            queue_depth[queue_end] = 0;
            queue_end++;
        }
    }

    // Process queue
    while (queue_start < queue_end) {     // LCOV_EXCL_BR_LINE
        size_t idx = queue_idx[queue_start];
        size_t depth = queue_depth[queue_start];
        queue_start++;

        ik_db_agent_row_t *agent = all_agents[idx];

        // Count status
        if (strcmp(agent->status, "running") == 0) {     // LCOV_EXCL_BR_LINE
            running_count++;
        } else {     // LCOV_EXCL_BR_LINE
            dead_count++;     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        // Build line with indentation
        char line[256];
        size_t offset = 0;

        // Root agent: use marker column
        bool is_current = strcmp(agent->uuid, repl->current->uuid) == 0;
        char marker = (depth == 0 && is_current) ? '*' : ' ';
        line[offset++] = marker;
        line[offset++] = ' ';

        // Child agents: add tree prefix
        if (depth > 0) {
            // 4 spaces for each depth level above 1
            for (size_t d = 1; d < depth; d++) {
                memcpy(&line[offset], "    ", 4);
                offset += 4;
            }
            // Tree character
            memcpy(&line[offset], "+-- ", 4);
            offset += 4;
        }

        // Add full UUID
        size_t uuid_len = strlen(agent->uuid);
        memcpy(&line[offset], agent->uuid, uuid_len);
        offset += uuid_len;

        // Add status
        line[offset++] = ' ';
        line[offset++] = '(';
        size_t status_len = strlen(agent->status);
        memcpy(&line[offset], agent->status, status_len);
        offset += status_len;
        line[offset++] = ')';

        // Add root label if parent is NULL
        if (agent->parent_uuid == NULL) {
            memcpy(&line[offset], " - root", 7);
            offset += 7;
        }

        line[offset] = '\0';

        // Append line to scrollback
        res = ik_scrollback_append_line(repl->current->scrollback, line, offset);
        if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
            talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
            return res;     // LCOV_EXCL_LINE
        }     // LCOV_EXCL_LINE

        // Find and queue children
        for (size_t i = 0; i < all_count; i++) {     // LCOV_EXCL_BR_LINE
            if (all_agents[i]->parent_uuid != NULL) {     // LCOV_EXCL_BR_LINE
                if (strcmp(all_agents[i]->parent_uuid, agent->uuid) == 0) {     // LCOV_EXCL_BR_LINE
                    queue_idx[queue_end] = i;
                    queue_depth[queue_end] = depth + 1;
                    queue_end++;
                }
            }
        }
    }

    // Add blank line before summary
    res = ik_scrollback_append_line(repl->current->scrollback, "", 0);
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    // Add summary
    char summary[64];
    snprintf(summary, sizeof(summary), "%" PRIu64 " running, %" PRIu64 " dead",
             (uint64_t)running_count, (uint64_t)dead_count);
    res = ik_scrollback_append_line(repl->current->scrollback, summary, strlen(summary));
    if (is_err(&res)) {     // LCOV_EXCL_BR_LINE
        talloc_free(tmp_ctx);     // LCOV_EXCL_LINE
        return res;     // LCOV_EXCL_LINE
    }     // LCOV_EXCL_LINE

    talloc_free(tmp_ctx);
    return OK(NULL);
}
