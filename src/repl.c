#include "repl.h"
#include "repl_actions.h"
#include "signal_handler.h"
#include "panic.h"
#include "wrapper.h"
#include "format.h"
#include "input_buffer/core.h"
#include "render_cursor.h"
#include "openai/client_multi.h"
#include <assert.h>
#include <talloc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/select.h>
#include <sys/time.h>

res_t ik_repl_init(void *parent, ik_repl_ctx_t **repl_out)
{
    assert(parent != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(repl_out != NULL);   /* LCOV_EXCL_BR_LINE */

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
    result = ik_input_buffer_create(repl, &repl->input_buffer);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Initialize input parser
    result = ik_input_parser_create(repl, &repl->input_parser);
    if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

    // Initialize scrollback buffer (Phase 4)
    result = ik_scrollback_create(repl, repl->term->screen_cols, &repl->scrollback);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

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
    result = ik_layer_cake_create(repl, (size_t)repl->term->screen_rows, &repl->layer_cake);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create scrollback layer
    result = ik_scrollback_layer_create(repl, "scrollback", repl->scrollback, &repl->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (Phase 1.4)
    result = ik_spinner_layer_create(repl, "spinner", &repl->spinner_state, &repl->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create separator layer
    result = ik_separator_layer_create(repl, "separator", &repl->separator_visible, &repl->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create input layer
    result = ik_input_layer_create(repl, "input", &repl->input_buffer_visible,
                                   &repl->input_text, &repl->input_text_len,
                                   &repl->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

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

    // Set up signal handlers (SIGWINCH for terminal resize)
    result = ik_signal_handler_init(parent);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - Signal handler setup failure is rare (invalid signal number)
        talloc_free(repl);  // LCOV_EXCL_LINE
        return result;  // LCOV_EXCL_LINE
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

// Helper: Calculate effective select() timeout
static inline long calculate_select_timeout_ms_(const ik_repl_ctx_t *repl, long curl_timeout_ms)
{
    // Spinner timer: 80ms when visible, no timeout when hidden
    long spinner_timeout_ms = repl->spinner_state.visible ? 80 : -1;  // LCOV_EXCL_BR_LINE

    // Use minimum of both timeouts (if both are set)
    if (spinner_timeout_ms >= 0 && curl_timeout_ms >= 0) {  // LCOV_EXCL_BR_LINE
        return (spinner_timeout_ms < curl_timeout_ms) ? spinner_timeout_ms : curl_timeout_ms;  // LCOV_EXCL_LINE
    }
    if (spinner_timeout_ms >= 0) {  // LCOV_EXCL_BR_LINE
        return spinner_timeout_ms;  // LCOV_EXCL_LINE
    }
    if (curl_timeout_ms >= 0) {  // LCOV_EXCL_BR_LINE
        return curl_timeout_ms;  // LCOV_EXCL_LINE
    }
    // Use a 1-second timeout to prevent blocking forever
    return 1000;
}

// Helper: Set up file descriptor sets for select()
static inline res_t setup_fd_sets_(ik_repl_ctx_t *repl,
                                   fd_set *read_fds,
                                   fd_set *write_fds,
                                   fd_set *exc_fds,
                                   int *max_fd_out)
{
    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    FD_ZERO(exc_fds);

    // Add terminal fd
    int32_t terminal_fd = repl->term->tty_fd;
    FD_SET(terminal_fd, read_fds);
    int max_fd = terminal_fd;

    // Add curl_multi fds
    int curl_max_fd = -1;
    res_t result = ik_openai_multi_fdset(repl->multi, read_fds, write_fds, exc_fds, &curl_max_fd);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        return result;  // LCOV_EXCL_LINE
    }
    if (curl_max_fd > max_fd) {  // LCOV_EXCL_BR_LINE
        max_fd = curl_max_fd;  // LCOV_EXCL_LINE
    }

    *max_fd_out = max_fd;
    return OK(NULL);
}

// Helper: Handle terminal input
static inline res_t handle_terminal_input_(ik_repl_ctx_t *repl, int terminal_fd, bool *should_exit)
{
    char byte;
    ssize_t n = posix_read_(terminal_fd, &byte, 1);
    if (n < 0) {  // LCOV_EXCL_BR_LINE
        // LCOV_EXCL_START
        if (errno == EINTR) {
            return OK(NULL);
        }
        *should_exit = true;
        return OK(NULL);
        // LCOV_EXCL_STOP
    }
    if (n == 0) {
        *should_exit = true;
        return OK(NULL);
    }

    // Parse and process action
    ik_input_action_t action;
    ik_input_parse_byte(repl->input_parser, byte, &action);

    res_t result = ik_repl_process_action(repl, &action);
    if (is_err(&result)) return result; // LCOV_EXCL_LINE

    // Render if needed
    if (action.type != IK_INPUT_UNKNOWN) {
        return ik_repl_render_frame(repl);
    }

    return OK(NULL);
}

res_t ik_repl_run(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Initial render
    res_t result = ik_repl_render_frame(repl);
    if (is_err(&result)) {
        return result;
    }

    // Main event loop
    bool should_exit = false;
    while (!repl->quit && !should_exit) {  // LCOV_EXCL_BR_LINE
        // Check for pending resize
        CHECK(ik_signal_check_resize(repl));  // LCOV_EXCL_BR_LINE

        // Set up fd_sets
        fd_set read_fds, write_fds, exc_fds;
        int max_fd;
        CHECK(setup_fd_sets_(repl, &read_fds, &write_fds, &exc_fds, &max_fd));  // LCOV_EXCL_BR_LINE

        // Calculate timeout
        long curl_timeout_ms = -1;
        CHECK(ik_openai_multi_timeout(repl->multi, &curl_timeout_ms));  // LCOV_EXCL_BR_LINE
        long effective_timeout_ms = calculate_select_timeout_ms_(repl, curl_timeout_ms);

        struct timeval timeout;
        timeout.tv_sec = effective_timeout_ms / 1000;
        timeout.tv_usec = (effective_timeout_ms % 1000) * 1000;

        // Call select()
        int ready = posix_select_(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);

        if (ready < 0) {  // LCOV_EXCL_BR_LINE
            // LCOV_EXCL_START
            if (errno == EINTR) {
                CHECK(ik_signal_check_resize(repl));
                continue;
            }
            break;
            // LCOV_EXCL_STOP
        }

        // Handle timeout (spinner animation)
        if (ready == 0 && repl->spinner_state.visible) {  // LCOV_EXCL_BR_LINE
            ik_spinner_advance(&repl->spinner_state);  // LCOV_EXCL_LINE
            CHECK(ik_repl_render_frame(repl));  // LCOV_EXCL_LINE
            continue;  // LCOV_EXCL_LINE
        }

        // Handle terminal input
        if (FD_ISSET(repl->term->tty_fd, &read_fds)) {  // LCOV_EXCL_BR_LINE
            CHECK(handle_terminal_input_(repl, repl->term->tty_fd, &should_exit));
            if (should_exit) break;
        }

        // Handle curl_multi events
        // Only call curl_multi_perform when there's work to do:
        // - ready == 0 means select() timed out (curl may need to handle its timeouts)
        // - curl_still_running > 0 means there are active transfers that need processing
        if (ready == 0 || repl->curl_still_running > 0) {
            CHECK(ik_openai_multi_perform(repl->multi, &repl->curl_still_running));  // LCOV_EXCL_BR_LINE
            CHECK(ik_openai_multi_info_read(repl->multi));  // LCOV_EXCL_BR_LINE
        }
    }

    return OK(NULL);
}

res_t ik_repl_submit_line(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Get current input buffer text
    const char *text = (const char *)repl->input_buffer->text->data;
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);

    // Append to scrollback (only if there's content and scrollback exists)
    if (text_len > 0 && repl->scrollback != NULL) {
        res_t result = ik_scrollback_append_line(repl->scrollback, text, text_len);
        if (is_err(&result)) return result; // LCOV_EXCL_LINE - Defensive, allocation failures are rare
    }

    // Clear input buffer
    ik_input_buffer_clear(repl->input_buffer);

    // Auto-scroll to bottom (reset viewport offset)
    repl->viewport_offset = 0;

    return OK(NULL);
}

res_t ik_repl_handle_resize(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->render != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update terminal dimensions
    int rows, cols;
    res_t result = ik_term_get_size(repl->term, &rows, &cols);
    if (is_err(&result)) {
        return result;
    }

    // Update render context dimensions
    repl->render->rows = rows;
    repl->render->cols = cols;

    // Invalidate layout caches (will recalculate on next ensure_layout call)
    ik_scrollback_ensure_layout(repl->scrollback, cols);
    ik_input_buffer_ensure_layout(repl->input_buffer, cols);

    // Trigger immediate redraw with new dimensions
    return ik_repl_render_frame(repl);
}

void ik_repl_transition_to_waiting_for_llm(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->state == IK_REPL_STATE_IDLE);   /* LCOV_EXCL_BR_LINE */

    // Update state
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;

    // Show spinner, hide input
    repl->spinner_state.visible = true;
    repl->input_buffer_visible = false;
}

void ik_repl_transition_to_idle(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */

    // Update state
    repl->state = IK_REPL_STATE_IDLE;

    // Hide spinner, show input
    repl->spinner_state.visible = false;
    repl->input_buffer_visible = true;
}
