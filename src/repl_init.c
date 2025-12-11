// REPL initialization and cleanup
#include "repl.h"

#include "repl/session_restore.h"
#include "config.h"
#include "db/connection.h"
#include "db/session.h"
#include "logger.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "panic.h"
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
    if (repl->tool_thread_running) {  // LCOV_EXCL_BR_LINE
        pthread_join_(repl->tool_thread, NULL);  // LCOV_EXCL_LINE
    }

    pthread_mutex_destroy_(&repl->tool_thread_mutex);
    return 0;
}

res_t ik_repl_init(void *parent, ik_shared_ctx_t *shared, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     // LCOV_EXCL_BR_LINE
    assert(shared != NULL);     // LCOV_EXCL_BR_LINE
    assert(repl_out != NULL);   // LCOV_EXCL_BR_LINE

    ik_cfg_t *cfg = shared->cfg;

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

    // Initialize input buffer
    repl->input_buffer = ik_input_buffer_create(repl);

    // Initialize input parser
    repl->input_parser = ik_input_parser_create(repl);

    // Initialize scrollback buffer (Phase 4)
    repl->scrollback = ik_scrollback_create(repl, repl->shared->term->screen_cols);

    // Initialize viewport offset to 0 (at bottom)
    repl->viewport_offset = 0;

    // Initialize scroll detector (rel-05)
    repl->scroll_det = ik_scroll_detector_create(repl);

    // Set quit flag to false
    repl->quit = false;

    // Initialize layer-based rendering (Phase 1.3)
    // Initialize reference fields
    repl->spinner_state.frame_index = 0;
    repl->spinner_state.visible = false;  // Initially hidden
    repl->separator_visible = true;
    repl->lower_separator_visible = true;  // Lower separator initially visible
    repl->input_buffer_visible = true;
    repl->input_text = "";
    repl->input_text_len = 0;

    // Initialize completion context to NULL (inactive) (rel-04)
    repl->completion = NULL;

    // Create layer cake
    repl->layer_cake = ik_layer_cake_create(repl, (size_t)repl->shared->term->screen_rows);

    // Create scrollback layer
    repl->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", repl->scrollback);

    // Create spinner layer (Phase 1.4)
    repl->spinner_layer = ik_spinner_layer_create(repl, "spinner", &repl->spinner_state);

    // Create separator layer
    repl->separator_layer = ik_separator_layer_create(repl, "separator", &repl->separator_visible);

    // Set debug info on upper separator (for debugging viewport issues)
    ik_separator_layer_set_debug(repl->separator_layer,
                                 &repl->debug_viewport_offset,
                                 &repl->debug_viewport_row,
                                 &repl->debug_viewport_height,
                                 &repl->debug_document_height,
                                 &repl->render_elapsed_us);

    // Create lower separator layer
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &repl->lower_separator_visible);

    // Create input layer
    repl->input_layer = ik_input_layer_create(repl, "input", &repl->input_buffer_visible,
                                              &repl->input_text, &repl->input_text_len);

    // Create completion layer (rel-04)
    repl->completion_layer = ik_completion_layer_create(repl, "completion", &repl->completion);

    // Add layers to cake (in order: scrollback, spinner, separator, input, lower_separator, completion)
    result = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->lower_separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->completion_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Initialize curl_multi handle for non-blocking HTTP (Phase 1.6)
    repl->multi = TRY(ik_openai_multi_create(repl));  // LCOV_EXCL_BR_LINE
    repl->curl_still_running = 0;  // No active transfers initially
    repl->state = IK_REPL_STATE_IDLE;  // Start in IDLE state

    // Initialize conversation for session messages (Phase 1.6)
    repl->conversation = ik_openai_conversation_create(repl).ok;

    // Initialize assistant response accumulator (Phase 1.6)
    repl->assistant_response = NULL;

    // Initialize streaming line buffer
    repl->streaming_line_buffer = NULL;

    // Initialize HTTP error message (Phase 1.7)
    repl->http_error_message = NULL;

    // Initialize marks array (Phase 1.7)
    repl->marks = NULL;
    repl->mark_count = 0;

    // Restore session if database is configured (must be after repl allocated)
    if (shared->db_ctx != NULL) {
        result = ik_repl_restore_session_(repl, shared->db_ctx, cfg);
        if (is_err(&result)) {
            talloc_free(repl);
            return result;
        }
    }

    // Initialize tool thread mutex
    int mutex_ret = pthread_mutex_init_(&repl->tool_thread_mutex, NULL);
    if (mutex_ret != 0) {
        talloc_free(repl);
        return ERR(parent, IO, "Failed to initialize tool thread mutex");
    }

    // Initialize tool thread state
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;
    repl->tool_thread_ctx = NULL;
    repl->tool_thread_result = NULL;

    // Set destructor for cleanup (handles mutex destruction)
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
