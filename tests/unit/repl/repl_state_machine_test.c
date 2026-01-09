#include "agent.h"
/**
 * @file repl_state_machine_test.c
 * @brief Unit tests for REPL state machine (Phase 1.6 Task 6.4)
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/render.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../test_utils.h"

// Forward declaration for wrapper function
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock write wrapper
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

// Helper function to create a minimal REPL context for testing
static ik_repl_ctx_t *create_test_repl(void *ctx)
{
    res_t res;

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create term context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    // Create scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Create layer cake and layers (minimal setup for render_frame)
    ik_layer_cake_t *layer_cake = NULL;
    layer_cake = ik_layer_cake_create(ctx, 24);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->render = render;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;
    repl->current->layer_cake = layer_cake;

    // Initialize reference fields
    repl->current->separator_visible = true;
    repl->current->input_buffer_visible = true;
    repl->current->input_text = "";
    repl->current->input_text_len = 0;
    repl->current->spinner_state.frame_index = 0;
    repl->current->spinner_state.visible = false;

    // Initialize state to IDLE (default)
    repl->current->state = IK_AGENT_STATE_IDLE;

    // Create layers
    ik_layer_t *scrollback_layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_layer_t *spinner_layer = ik_spinner_layer_create(ctx, "spinner", &repl->current->spinner_state);

    ik_layer_t *separator_layer = ik_separator_layer_create(ctx, "separator", &repl->current->separator_visible);

    ik_layer_t *input_layer = ik_input_layer_create(ctx, "input", &repl->current->input_buffer_visible,
                                                    &repl->current->input_text, &repl->current->input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(layer_cake, scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, spinner_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, input_layer);
    ck_assert(is_ok(&res));

    return repl;
}

/* Test: Initial state is IDLE */
START_TEST(test_repl_initial_state_is_idle) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Verify initial state is IDLE
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    talloc_free(ctx);
}
END_TEST
/* Test: When state is IDLE, spinner is hidden and input is visible */
START_TEST(test_repl_state_idle_visibility) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Set state to IDLE
    repl->current->state = IK_AGENT_STATE_IDLE;

    // Call render_frame to update visibility flags
    res_t res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Verify: spinner hidden, input visible
    ck_assert(!repl->current->spinner_state.visible);
    ck_assert(repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: When state is WAITING_FOR_LLM, spinner is visible and input is hidden */
START_TEST(test_repl_state_waiting_for_llm_visibility) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Set state to WAITING_FOR_LLM
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;

    // Call render_frame to update visibility flags
    res_t res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    // Verify: spinner visible, input hidden
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: State transition IDLE -> WAITING_FOR_LLM */
START_TEST(test_repl_state_transition_idle_to_waiting) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Start in IDLE state
    repl->current->state = IK_AGENT_STATE_IDLE;
    res_t res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert(!repl->current->spinner_state.visible);
    ck_assert(repl->current->input_buffer_visible);

    // Transition to WAITING_FOR_LLM
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: State transition WAITING_FOR_LLM -> IDLE */
START_TEST(test_repl_state_transition_waiting_to_idle) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Start in WAITING_FOR_LLM state
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    res_t res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);

    // Transition to IDLE
    repl->current->state = IK_AGENT_STATE_IDLE;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert(!repl->current->spinner_state.visible);
    ck_assert(repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: Transition function IDLE -> WAITING_FOR_LLM */
START_TEST(test_repl_transition_to_waiting_for_llm_function) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Start in IDLE state (default)
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);
    ck_assert(!repl->current->spinner_state.visible);
    ck_assert(repl->current->input_buffer_visible);

    // Call transition function
    ik_agent_transition_to_waiting_for_llm(repl->current);

    // Verify state changed
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: Transition function WAITING_FOR_LLM -> IDLE */
START_TEST(test_repl_transition_to_idle_function) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Start in WAITING_FOR_LLM state
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->spinner_state.visible = true;
    repl->current->input_buffer_visible = false;

    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);

    // Call transition function
    ik_agent_transition_to_idle(repl->current);

    // Verify state changed
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);
    ck_assert(!repl->current->spinner_state.visible);
    ck_assert(repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST
/* Test: Full cycle IDLE -> WAITING -> IDLE */
START_TEST(test_repl_state_full_cycle) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Start in IDLE
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // Transition to WAITING_FOR_LLM
    ik_agent_transition_to_waiting_for_llm(repl->current);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);

    // Transition back to IDLE
    ik_agent_transition_to_idle(repl->current);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);
    ck_assert(!repl->current->spinner_state.visible);
    ck_assert(repl->current->input_buffer_visible);

    talloc_free(ctx);
}

END_TEST

static Suite *repl_state_machine_suite(void)
{
    Suite *s = suite_create("REPL State Machine");

    TCase *tc_state = tcase_create("State Machine");
    tcase_set_timeout(tc_state, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_state, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_state, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_state, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_state, IK_TEST_TIMEOUT);
    tcase_add_test(tc_state, test_repl_initial_state_is_idle);
    tcase_add_test(tc_state, test_repl_state_idle_visibility);
    tcase_add_test(tc_state, test_repl_state_waiting_for_llm_visibility);
    tcase_add_test(tc_state, test_repl_state_transition_idle_to_waiting);
    tcase_add_test(tc_state, test_repl_state_transition_waiting_to_idle);
    tcase_add_test(tc_state, test_repl_transition_to_waiting_for_llm_function);
    tcase_add_test(tc_state, test_repl_transition_to_idle_function);
    tcase_add_test(tc_state, test_repl_state_full_cycle);
    suite_add_tcase(s, tc_state);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_state_machine_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
