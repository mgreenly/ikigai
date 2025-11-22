// REPL initialization and cleanup
#include "repl.h"

#include "config.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "signal_handler.h"
#include "wrapper.h"

#include <assert.h>
#include <talloc.h>

res_t ik_repl_init(void *parent, ik_cfg_t *cfg, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     // LCOV_EXCL_BR_LINE
    assert(cfg != NULL);        // LCOV_EXCL_BR_LINE
    assert(repl_out != NULL);   // LCOV_EXCL_BR_LINE

    // Allocate REPL context
    ik_repl_ctx_t *repl = talloc_zero_(parent, sizeof(ik_repl_ctx_t));
    if (repl == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize terminal (raw mode + alternate screen)
    res_t result = ik_term_init(repl, &repl->term);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize render
    result = ik_render_create(repl,
                              repl->term->screen_rows,
                              repl->term->screen_cols,
                              repl->term->tty_fd,
                              &repl->render);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    // Initialize input buffer
    repl->input_buffer = ik_input_buffer_create(repl);

    // Initialize input parser
    repl->input_parser = ik_input_parser_create(repl);

    // Initialize scrollback buffer (Phase 4)
    repl->scrollback = ik_scrollback_create(repl, repl->term->screen_cols);

    // Initialize viewport offset to 0 (at bottom)
    repl->viewport_offset = 0;

    // Set quit flag to false
    repl->quit = false;

    // Initialize layer-based rendering (Phase 1.3)
    // Initialize reference fields
    repl->spinner_state.frame_index = 0;
    repl->spinner_state.visible = false;  // Initially hidden
    repl->separator_visible = true;
    repl->input_buffer_visible = true;
    repl->input_text = "";
    repl->input_text_len = 0;

    // Create layer cake
    repl->layer_cake = ik_layer_cake_create(repl, (size_t)repl->term->screen_rows);

    // Create scrollback layer
    repl->scrollback_layer = ik_scrollback_layer_create(repl, "scrollback", repl->scrollback);

    // Create spinner layer (Phase 1.4)
    repl->spinner_layer = ik_spinner_layer_create(repl, "spinner", &repl->spinner_state);

    // Create separator layer
    repl->separator_layer = ik_separator_layer_create(repl, "separator", &repl->separator_visible);

    // Create input layer
    repl->input_layer = ik_input_layer_create(repl, "input", &repl->input_buffer_visible,
                                              &repl->input_text, &repl->input_text_len);

    // Add layers to cake (in order: scrollback, spinner, separator, input)
    result = ik_layer_cake_add_layer(repl->layer_cake, repl->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    result = ik_layer_cake_add_layer(repl->layer_cake, repl->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Initialize curl_multi handle for non-blocking HTTP (Phase 1.6)
    repl->multi = TRY(ik_openai_multi_create(repl));  // LCOV_EXCL_BR_LINE
    repl->curl_still_running = 0;  // No active transfers initially
    repl->state = IK_REPL_STATE_IDLE;  // Start in IDLE state

    // Store config reference (borrowed from parent)
    repl->cfg = cfg;

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

    // Debug pipe manager
    repl->debug_mgr = TRY(ik_debug_mgr_create(repl));  // LCOV_EXCL_BR_LINE
    repl->openai_debug_pipe = TRY(ik_debug_mgr_add_pipe(repl->debug_mgr, "[openai]"));  // LCOV_EXCL_BR_LINE
    repl->debug_enabled = false;

    // Set up signal handlers (SIGWINCH for terminal resize)
    result = ik_signal_handler_init(parent);
    if (is_err(&result)) {
        talloc_free(repl);
        return result;
    }

    *repl_out = repl;
    return OK(repl);
}

void ik_repl_cleanup(ik_repl_ctx_t *repl)
{
    if (repl == NULL) {
        return;
    }

    // Cleanup terminal (restore state)
    if (repl->term != NULL) {
        ik_term_cleanup(repl->term);
    }

    // Other components cleaned up via talloc hierarchy
    talloc_free(repl);
}
