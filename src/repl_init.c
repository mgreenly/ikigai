// REPL initialization and cleanup
#include "repl.h"

#include "agent.h"
#include "db/agent.h"
#include "db/agent_zero.h"
#include "db/connection.h"
#include "db/session.h"
#include "logger.h"
#include "panic.h"
#include "repl/agent_restore.h"
#include "shared.h"
#include "signal_handler.h"
#include "wrapper.h"

#include <assert.h>
#include <talloc.h>

// Destructor for REPL context - handles cleanup on exit or Ctrl+C
static int repl_destructor(ik_repl_ctx_t *repl)
{
    // If tool thread is running, wait for it to finish before cleanup.
    // This handles Ctrl+C gracefully - we don't cancel the thread or leave
    // it orphaned. If a bash command is stuck, we wait (known limitation:
    // "Bash command timeout - not implemented" in README Out of Scope).
    // The alternative (pthread_cancel) risks leaving resources in bad state.
    if (repl->current != NULL && repl->current->tool_thread_running) {  // LCOV_EXCL_BR_LINE
        pthread_join_(repl->current->tool_thread, NULL);  // LCOV_EXCL_LINE
    }

    // Mutex cleanup now handled by agent destructor
    return 0;
}

res_t ik_repl_init(void *parent, ik_shared_ctx_t *shared, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     // LCOV_EXCL_BR_LINE
    assert(shared != NULL);     // LCOV_EXCL_BR_LINE
    assert(repl_out != NULL);   // LCOV_EXCL_BR_LINE

    // Set up signal handlers (SIGWINCH for terminal resize) - must be before repl alloc
    res_t result = ik_signal_handler_init(parent);
    if (is_err(&result)) {
        return result;
    }

    // Phase 2: All failable inits succeeded - allocate repl context
    ik_repl_ctx_t *repl = talloc_zero_(parent, sizeof(ik_repl_ctx_t));
    if (repl == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Wire up successfully initialized components
    repl->shared = shared;

    // Initialize agent array
    repl->agents = NULL;
    repl->agent_count = 0;
    repl->agent_capacity = 0;

    // Create agent context (owns display state)
    result = ik_agent_create(repl, repl->shared, NULL, &repl->current);
    if (is_err(&result)) {
        // Reparent error to parent before freeing repl (error is child of repl)
        talloc_steal(parent, result.err);
        talloc_free(repl);
        return result;
    }

    // Set repl backpointer on agent
    repl->current->repl = repl;

    // Add initial agent to array
    result = ik_repl_add_agent(repl, repl->current);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        talloc_steal(parent, result.err);  // LCOV_EXCL_LINE
        talloc_free(repl);  // LCOV_EXCL_LINE
        return result;  // LCOV_EXCL_LINE
    }

    // Initialize input parser
    repl->input_parser = ik_input_parser_create(repl);

    // Initialize scroll detector (rel-05)
    repl->scroll_det = ik_scroll_detector_create(repl);

    // Set quit flag to false
    repl->quit = false;

    // Initialize layer-based rendering (Phase 1.3)
    // Initialize reference fields
    repl->lower_separator_visible = true;  // Lower separator initially visible

    // Note: completion initialization removed - now in agent context (repl->current->completion)

    // Create lower separator layer (not part of agent - stays in repl)
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &repl->lower_separator_visible);

    // Add lower separator to agent's layer cake
    result = ik_layer_cake_add_layer(repl->current->layer_cake, repl->lower_separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Ensure Agent 0 exists in registry if database is configured
    if (shared->db_ctx != NULL) {
        char *agent_zero_uuid = NULL;
        result = ik_db_ensure_agent_zero(shared->db_ctx, &agent_zero_uuid);
        if (is_err(&result)) {
            talloc_free(repl);
            return result;
        }
        repl->current->uuid = talloc_steal(repl->current, agent_zero_uuid);

        // Get or create session
        int64_t session_id = 0;
        result = ik_db_session_get_active(shared->db_ctx, &session_id);
        if (is_err(&result)) {
            talloc_free(repl);
            return result;
        }

        // If no active session exists, create one
        if (session_id == 0) {
            result = ik_db_session_create(shared->db_ctx, &session_id);
            if (is_err(&result)) {
                talloc_free(repl);
                return result;
            }
        }

        // Store session_id in shared context
        shared->session_id = session_id;

        // Restore all running agents from database (including Agent 0)
        result = ik_repl_restore_agents(repl, shared->db_ctx);
        if (is_err(&result)) {
            talloc_free(repl);
            return result;
        }
    }

    // Tool state initialization now in agent_create
    // Set destructor for cleanup
    talloc_set_destructor(repl, repl_destructor);

    *repl_out = repl;
    return OK(repl);
}

void ik_repl_cleanup(ik_repl_ctx_t *repl)
{
    if (repl == NULL) {
        return;
    }

    // Terminal cleanup now handled by shared context
    // Other components cleaned up via talloc hierarchy
    talloc_free(repl);
}
