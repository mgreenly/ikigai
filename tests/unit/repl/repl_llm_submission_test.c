/**
 * @file repl_llm_submission_test.c
 * @brief Unit tests for REPL LLM submission flow (Phase 1.6)
 *
 * Tests the code path where a non-slash-command message is submitted
 * to the LLM when cfg and conversation are initialized.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"
#include "../../../src/render.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../../src/openai/client_multi.h"
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

// Helper function to create a REPL context with cfg and conversation
static ik_repl_ctx_t *create_test_repl_with_llm(void *ctx)
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

    // Create layer cake and layers
    ik_layer_cake_t *layer_cake = NULL;
    layer_cake = ik_layer_cake_create(ctx, 24);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->render = render;
    repl->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;
    repl->layer_cake = layer_cake;

    // Initialize reference fields
    repl->separator_visible = true;
    repl->input_buffer_visible = true;
    repl->input_text = "";
    repl->input_text_len = 0;
    repl->spinner_state.frame_index = 0;
    repl->spinner_state.visible = false;

    // Initialize state to IDLE
    repl->state = IK_REPL_STATE_IDLE;

    // Create layers
    ik_layer_t *scrollback_layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_layer_t *spinner_layer = ik_spinner_layer_create(ctx, "spinner", &repl->spinner_state);

    ik_layer_t *separator_layer = ik_separator_layer_create(ctx, "separator", &repl->separator_visible);

    ik_layer_t *input_layer = ik_input_layer_create(ctx, "input", &repl->input_buffer_visible,
                                                    &repl->input_text, &repl->input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(layer_cake, scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, spinner_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, input_layer);
    ck_assert(is_ok(&res));

    // Create config
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "test-api-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant.");
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    repl->shared = shared;

    // Create conversation
    res = ik_openai_conversation_create(ctx);
    ck_assert(is_ok(&res));
    repl->conversation = res.ok;

    // Create multi handle
    res = ik_openai_multi_create(ctx);
    ck_assert(is_ok(&res));
    repl->multi = res.ok;

    // Initialize curl_still_running
    repl->curl_still_running = 0;

    // Initialize assistant_response to NULL
    repl->assistant_response = NULL;

    return repl;
}

/* Test: Submitting a message when cfg and conversation are initialized */
START_TEST(test_submit_message_with_llm_initialized) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Type a message
    const char *message = "Hello";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Verify input buffer has the message
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);
    ck_assert_uint_eq(text_len, 5);

    // Submit the message (triggers LLM submission flow)
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify state transitioned to WAITING_FOR_LLM
    ck_assert_int_eq(repl->state, IK_REPL_STATE_WAITING_FOR_LLM);

    // Verify input buffer was cleared
    text_len = ik_byte_array_size(repl->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    // Verify user message was added to conversation
    ck_assert_uint_eq((unsigned int)repl->conversation->message_count, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Submitting a message when previous assistant_response exists */
START_TEST(test_submit_message_clears_previous_assistant_response)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Set a previous assistant_response
    repl->assistant_response = talloc_strdup(repl, "Previous response");
    ck_assert_ptr_nonnull(repl->assistant_response);

    // Type a message
    const char *message = "New question";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Submit the message
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify previous assistant_response was cleared
    ck_assert_ptr_null(repl->assistant_response);

    // Verify state transitioned to WAITING_FOR_LLM
    ck_assert_int_eq(repl->state, IK_REPL_STATE_WAITING_FOR_LLM);

    talloc_free(ctx);
}

END_TEST
/* Test: Submitting a message when cfg is NULL (should not send to LLM) */
START_TEST(test_submit_message_without_cfg)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Set cfg to NULL
    repl->shared->cfg = NULL;

    // Type a message
    const char *message = "Hello";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Submit the message (should NOT send to LLM since cfg is NULL)
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify state did NOT transition to WAITING_FOR_LLM
    ck_assert_int_eq(repl->state, IK_REPL_STATE_IDLE);

    // Verify input buffer was cleared
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    // Verify NO message was added to conversation (still 0)
    ck_assert_uint_eq((unsigned int)repl->conversation->message_count, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: API request failure is handled gracefully (empty API key) */
START_TEST(test_submit_message_api_request_failure)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Set empty API key to trigger failure
    talloc_free(repl->shared->cfg->openai_api_key);
    repl->shared->cfg->openai_api_key = talloc_strdup(repl->shared->cfg, "");

    // Type a message
    const char *message = "Hello";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Submit the message (should trigger API request that fails due to empty API key)
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify state transitioned back to IDLE (not stuck in WAITING_FOR_LLM)
    ck_assert_int_eq(repl->state, IK_REPL_STATE_IDLE);

    // Verify input buffer was cleared
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    // Verify error message was added to scrollback
    ck_assert_uint_gt(repl->scrollback->count, 0);

    // Verify user message was added to conversation (error happens after message creation)
    ck_assert_uint_eq((unsigned int)repl->conversation->message_count, 1);

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_llm_submission_suite(void)
{
    Suite *s = suite_create("REPL LLM Submission");

    TCase *tc_submission = tcase_create("Submission");
    tcase_set_timeout(tc_submission, 30);
    tcase_add_test(tc_submission, test_submit_message_with_llm_initialized);
    tcase_add_test(tc_submission, test_submit_message_clears_previous_assistant_response);
    tcase_add_test(tc_submission, test_submit_message_without_cfg);
    tcase_add_test(tc_submission, test_submit_message_api_request_failure);
    suite_add_tcase(s, tc_submission);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_llm_submission_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
