// Agent restoration on startup
#include "agent_restore.h"

#include "../db/agent.h"
#include "../error.h"
#include "../logger.h"
#include "../repl.h"

#include <assert.h>
#include <stdlib.h>
#include <talloc.h>

// Comparison function for qsort - sort by created_at ascending (oldest first)
static int compare_agents_by_created_at(const void *a, const void *b)
{
    const ik_db_agent_row_t *agent_a = *(const ik_db_agent_row_t **)a;
    const ik_db_agent_row_t *agent_b = *(const ik_db_agent_row_t **)b;

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
        return ERR(repl, "Out of memory");  // LCOV_EXCL_LINE
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

    // 3. For each agent (loop structure only, body in next task):
    for (size_t i = 0; i < count; i++) {
        // Skip Agent 0 (already created in repl_init)
        // Agent 0 is the root agent with no parent
        if (agents[i]->parent_uuid == NULL) {
            continue;
        }

        // TODO: Loop body in next task (gap1-restore-agents-loop.md)
        // - Call ik_agent_restore() to create agent context from DB row
        // - Call ik_agent_replay_history() to get message history
        // - Populate conversation from replay context
        // - Populate scrollback from replay context
        // - Restore marks from replay context
        // - Add agent to repl->agents[] array
        (void)agents[i];  // Suppress unused warning until loop body implemented
    }

    talloc_free(tmp);
    return OK(NULL);
}
