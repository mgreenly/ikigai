#include "repl.h"
#include "panic.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

res_t ik_repl_add_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    assert(repl != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    // Grow array if needed
    if (repl->agent_count >= repl->agent_capacity) {
        size_t new_capacity = repl->agent_capacity == 0 ? 4 : repl->agent_capacity * 2;
        ik_agent_ctx_t **new_agents = talloc_realloc(repl, repl->agents, ik_agent_ctx_t *, (unsigned int)new_capacity);
        if (new_agents == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        repl->agents = new_agents;
        repl->agent_capacity = new_capacity;
    }

    // Add agent to array
    repl->agents[repl->agent_count] = agent;
    repl->agent_count++;

    return OK(NULL);
}

res_t ik_repl_remove_agent(ik_repl_ctx_t *repl, const char *uuid)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);  // LCOV_EXCL_BR_LINE

    // Find agent by UUID
    size_t index = 0;
    bool found = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, uuid) == 0) {
            index = i;
            found = true;
            break;
        }
    }

    if (!found) {
        return ERR(repl, AGENT_NOT_FOUND, "Agent not found: %s", uuid);
    }

    // Update current pointer if we're removing the current agent
    if (repl->agents[index] == repl->current) {
        repl->current = NULL;
    }

    // Shift remaining agents down
    for (size_t i = index; i < repl->agent_count - 1; i++) {
        repl->agents[i] = repl->agents[i + 1];
    }

    repl->agent_count--;

    return OK(NULL);
}

ik_agent_ctx_t *ik_repl_find_agent(ik_repl_ctx_t *repl, const char *uuid_prefix)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid_prefix != NULL);  // LCOV_EXCL_BR_LINE

    // Minimum prefix length is 4 characters
    size_t prefix_len = strlen(uuid_prefix);
    if (prefix_len < 4) {
        return NULL;
    }

    // First pass: check for exact match (takes priority)
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, uuid_prefix) == 0) {
            return repl->agents[i];
        }
    }

    // Second pass: check for prefix match
    ik_agent_ctx_t *match = NULL;
    size_t match_count = 0;

    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strncmp(repl->agents[i]->uuid, uuid_prefix, prefix_len) == 0) {
            match = repl->agents[i];
            match_count++;
            if (match_count > 1) {
                // Ambiguous - multiple matches
                return NULL;
            }
        }
    }

    return match;
}

bool ik_repl_uuid_ambiguous(ik_repl_ctx_t *repl, const char *uuid_prefix)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid_prefix != NULL);  // LCOV_EXCL_BR_LINE

    // Minimum prefix length is 4 characters
    size_t prefix_len = strlen(uuid_prefix);
    if (prefix_len < 4) {
        return false;
    }

    // Count matches
    size_t match_count = 0;

    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strncmp(repl->agents[i]->uuid, uuid_prefix, prefix_len) == 0) {
            match_count++;
            if (match_count > 1) {
                return true;
            }
        }
    }

    return false;
}
