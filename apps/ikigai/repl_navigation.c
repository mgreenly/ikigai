#include "apps/ikigai/repl.h"
#include "apps/ikigai/layer_wrappers.h"

#include <assert.h>
#include <string.h>


#include "shared/poison.h"
// Helper to check if two agents have the same parent
static bool same_parent(const char *parent1, const char *parent2)
{
    if (parent1 == NULL && parent2 == NULL) {
        return true;
    }
    if (parent1 != NULL && parent2 != NULL) {
        return strcmp(parent1, parent2) == 0;
    }
    return false;
}

// Helper to find agent by UUID in array
static ik_agent_ctx_t *find_agent_by_uuid(ik_repl_ctx_t *repl, const char *uuid)
{
    for (size_t i = 0; i < repl->agent_count; i++) {     // LCOV_EXCL_BR_LINE
        if (strcmp(repl->agents[i]->uuid, uuid) == 0) {
            return repl->agents[i];
        }
    }
    return NULL;     // LCOV_EXCL_LINE - Defensive: caller only passes UUIDs from same array
}

res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    if (new_agent == NULL) {
        return ERR(repl, INVALID_ARG, "Cannot switch to NULL agent");
    }

    if (new_agent == repl->current) {
        return OK(NULL);  // No-op, already on this agent
    }

    // State is already stored on agent_ctx:
    // - repl->current->input_buffer (per-agent)
    // - repl->current->viewport_offset (per-agent)
    // These don't need explicit save/restore because
    // they're already per-agent fields.

    // Switch current pointer
    repl->current = new_agent;

    // Update navigation context for new current agent
    ik_repl_update_nav_context(repl);

    return OK(NULL);
}

res_t ik_repl_nav_prev_sibling(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(repl->current != NULL);  // LCOV_EXCL_BR_LINE

    const char *parent = repl->current->parent_uuid;

    // Collect siblings (all in agents[] are running)
    size_t sibling_count = 0;
    ik_agent_ctx_t *siblings[64];
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *a = repl->agents[i];
        // Same parent means sibling (both NULL or both equal strings)
        bool same_parent = (parent == NULL && a->parent_uuid == NULL) ||
                           (parent != NULL && a->parent_uuid != NULL && strcmp(parent, a->parent_uuid) == 0);    // LCOV_EXCL_BR_LINE
        if (same_parent) {
            siblings[sibling_count++] = a;
        }
    }

    if (sibling_count <= 1) {
        return OK(NULL);  // No siblings or only self
    }

    // Find current index and switch to previous
    size_t current_idx = 0;
    for (size_t i = 0; i < sibling_count; i++) {     // LCOV_EXCL_BR_LINE
        if (siblings[i] == repl->current) {
            current_idx = i;
            break;
        }
    }
    size_t prev_idx = (current_idx == 0) ? (sibling_count - 1) : (current_idx - 1);
    CHECK(ik_repl_switch_agent(repl, siblings[prev_idx]));     // LCOV_EXCL_BR_LINE
    return OK(NULL);
}

res_t ik_repl_nav_next_sibling(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(repl->current != NULL);  // LCOV_EXCL_BR_LINE

    const char *parent = repl->current->parent_uuid;

    // Collect siblings (all in agents[] are running)
    size_t sibling_count = 0;
    ik_agent_ctx_t *siblings[64];
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *a = repl->agents[i];
        // Same parent means sibling (both NULL or both equal strings)
        bool same_parent = (parent == NULL && a->parent_uuid == NULL) ||
                           (parent != NULL && a->parent_uuid != NULL && strcmp(parent, a->parent_uuid) == 0);    // LCOV_EXCL_BR_LINE
        if (same_parent) {
            siblings[sibling_count++] = a;
        }
    }

    if (sibling_count <= 1) {
        return OK(NULL);  // No siblings or only self
    }

    // Find current index and switch to next
    size_t current_idx = 0;
    for (size_t i = 0; i < sibling_count; i++) {     // LCOV_EXCL_BR_LINE
        if (siblings[i] == repl->current) {
            current_idx = i;
            break;
        }
    }
    size_t next_idx = (current_idx + 1) % sibling_count;
    CHECK(ik_repl_switch_agent(repl, siblings[next_idx]));     // LCOV_EXCL_BR_LINE
    return OK(NULL);
}

res_t ik_repl_nav_parent(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(repl->current != NULL);  // LCOV_EXCL_BR_LINE

    if (repl->current->parent_uuid == NULL) {
        return OK(NULL);  // Already at root
    }

    // Dead agents are removed from agents[], so find returns NULL
    // if parent was killed. Separator shows grayed indicator.
    ik_agent_ctx_t *parent = ik_repl_find_agent(repl,
                                                repl->current->parent_uuid);
    if (parent) {
        CHECK(ik_repl_switch_agent(repl, parent));     // LCOV_EXCL_BR_LINE
    }
    return OK(NULL);
}

res_t ik_repl_nav_child(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(repl->current != NULL);  // LCOV_EXCL_BR_LINE

    // Find most recent running child
    // Only agents in agents[] are running (dead ones removed)
    ik_agent_ctx_t *newest = NULL;
    int64_t newest_time = 0;

    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *a = repl->agents[i];
        if (a->parent_uuid &&
            strcmp(a->parent_uuid, repl->current->uuid) == 0) {
            // If no candidate yet, take this one regardless of timestamp
            // Otherwise, take if newer (handles created_at = 0 from legacy data)
            if (newest == NULL || a->created_at > newest_time) {     // LCOV_EXCL_BR_LINE
                newest = a;
                newest_time = a->created_at;
            }
        }
    }

    if (newest) {
        CHECK(ik_repl_switch_agent(repl, newest));     // LCOV_EXCL_BR_LINE
    }
    return OK(NULL);
}

// Calculate and update navigation context for current agent's separator
void ik_repl_update_nav_context(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);       // LCOV_EXCL_BR_LINE

    if (repl->current == NULL || repl->current->separator_layer == NULL) {
        return;
    }

    const char *parent_uuid = repl->current->parent_uuid;
    const char *prev_sibling = NULL;
    const char *next_sibling = NULL;
    size_t child_count = 0;

    // Scan all agents to find children and siblings
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        if (agent == repl->current) {
            continue;
        }

        // Count children
        if (agent->parent_uuid != NULL &&
            strcmp(agent->parent_uuid, repl->current->uuid) == 0) {
            child_count++;
        }

        // Check if this is a sibling
        if (!same_parent(parent_uuid, agent->parent_uuid)) {
            continue;
        }

        // This is a sibling - find prev/next based on created_at
        if (agent->created_at < repl->current->created_at) {
            // Potential prev sibling (keep the most recent one)
            if (prev_sibling == NULL) {
                prev_sibling = agent->uuid;
            } else {
                ik_agent_ctx_t *current_prev = find_agent_by_uuid(repl, prev_sibling);
                if (current_prev != NULL && agent->created_at > current_prev->created_at) {     // LCOV_EXCL_BR_LINE
                    prev_sibling = agent->uuid;
                }
            }
        } else {
            // Potential next sibling (keep the earliest one)
            if (next_sibling == NULL) {
                next_sibling = agent->uuid;
            } else {
                ik_agent_ctx_t *current_next = find_agent_by_uuid(repl, next_sibling);
                if (current_next != NULL && agent->created_at < current_next->created_at) {     // LCOV_EXCL_BR_LINE
                    next_sibling = agent->uuid;
                }
            }
        }
    }

    // Update the separator layer
    ik_separator_layer_set_nav_context(
        repl->current->separator_layer,
        parent_uuid,
        prev_sibling,
        repl->current->uuid,
        next_sibling,
        child_count
        );
}
