/**
 * @file repl_streaming_advanced_test.c
 * @brief Unit tests for REPL streaming callback - advanced scenarios
 *
 * Tests the streaming callback code paths for complex scenarios.
 */

#include "repl_streaming_test_common.h"
#include "../../../src/agent.h"

/* Test: Streaming callback with empty lines in content (Task 6.2: line length branch) */
START_TEST(test_streaming_callback_with_empty_lines) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    g_write_callback = NULL;
    g_write_data = NULL;
    invoke_write_callback = false;

    // Type and submit a message
    const char *message = "Test";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t initial_count = ik_scrollback_get_line_count(repl->current->scrollback);

    // Simulate receiving SSE data with content containing empty lines in the middle
    // "Hello\n\nWorld" should create 3 lines: "Hello", "", "World"
    const char *sse_multiline = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\\n\\nWorld\"}}]}\n\n";
    mock_response_data = sse_multiline;
    mock_response_len = strlen(sse_multiline);
    invoke_write_callback = true;

    // Call multi_perform to trigger the write callback
    res = ik_openai_multi_perform(repl->current->multi, &repl->current->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify scrollback has new lines added
    // Should have: initial + "Hello" + "" = initial + 2
    // Note: "World" (no trailing newline) is buffered but not added to scrollback yet
    size_t after_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(after_count, initial_count + 2);

    // Verify the content of the added lines
    const char *line_text = NULL;
    size_t line_len = 0;

    // First line should be "Hello"
    res = ik_scrollback_get_line_text(repl->current->scrollback, initial_count, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 5);
    ck_assert_mem_eq(line_text, "Hello", 5);

    // Second line should be empty
    res = ik_scrollback_get_line_text(repl->current->scrollback, initial_count + 1, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 0);

    // "World" should be in the streaming_line_buffer (not in scrollback yet)
    ck_assert_ptr_nonnull(repl->current->streaming_line_buffer);
    ck_assert_str_eq(repl->current->streaming_line_buffer, "World");

    // Clean up
    invoke_write_callback = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Streaming callback flushes buffered line on newline (Task: cover lines 80-91) */
START_TEST(test_streaming_callback_buffered_line_flush)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    g_write_callback = NULL;
    g_write_data = NULL;
    invoke_write_callback = false;

    // Type and submit a message
    const char *message = "Test";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t initial_count = ik_scrollback_get_line_count(repl->current->scrollback);

    // First chunk: content without trailing newline (gets buffered)
    const char *chunk1 = "data: {\"choices\":[{\"delta\":{\"content\":\"First\"}}]}\n\n";
    mock_response_data = chunk1;
    mock_response_len = strlen(chunk1);
    invoke_write_callback = true;

    res = ik_openai_multi_perform(repl->current->multi, &repl->current->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify "First" is buffered (not in scrollback yet)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), initial_count);
    ck_assert_ptr_nonnull(repl->current->streaming_line_buffer);
    ck_assert_str_eq(repl->current->streaming_line_buffer, "First");

    // Second chunk: content with newline (flushes buffered content)
    const char *chunk2 = "data: {\"choices\":[{\"delta\":{\"content\":\" part\\nSecond part\"}}]}\n\n";
    mock_response_data = chunk2;
    mock_response_len = strlen(chunk2);

    res = ik_openai_multi_perform(repl->current->multi, &repl->current->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify "First part" was flushed to scrollback
    size_t after_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(after_count, initial_count + 1);

    const char *line_text = NULL;
    size_t line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, initial_count, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 10);
    ck_assert_mem_eq(line_text, "First part", 10);

    // Verify "Second part" is now in the buffer
    ck_assert_ptr_nonnull(repl->current->streaming_line_buffer);
    ck_assert_str_eq(repl->current->streaming_line_buffer, "Second part");

    // Clean up
    invoke_write_callback = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Submitting new message clears streaming_line_buffer (Task: cover lines 262-263) */
START_TEST(test_new_message_clears_streaming_buffer)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Set up streaming_line_buffer with content (simulating previous streaming)
    repl->current->streaming_line_buffer = talloc_strdup(repl, "buffered content");
    ck_assert_ptr_nonnull(repl->current->streaming_line_buffer);

    // Reset mock state
    g_write_callback = NULL;
    g_write_data = NULL;
    invoke_write_callback = false;

    // Type and submit a new message
    const char *message = "New message";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify streaming_line_buffer was cleared
    ck_assert_ptr_null(repl->current->streaming_line_buffer);

    // Verify state transitioned to WAITING_FOR_LLM
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    talloc_free(ctx);
}

END_TEST
/* Test: Submission with debug enabled (cover line 270 true branch) */
START_TEST(test_submission_with_debug_enabled)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Create a debug pipe
    res_t res = ik_debug_pipe_create(repl, "openai");
    ck_assert(is_ok(&res));
    repl->shared->openai_debug_pipe = res.ok;
    repl->shared->debug_enabled = true;

    // Reset mock state
    g_write_callback = NULL;
    g_write_data = NULL;
    invoke_write_callback = false;

    // Type and submit a message
    const char *message = "Test";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify state transitioned to WAITING_FOR_LLM
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_streaming_suite(void)
{
    Suite *s = suite_create("REPL Streaming Advanced");

    TCase *tc_streaming = tcase_create("Streaming");
    tcase_set_timeout(tc_streaming, 30);
    tcase_add_test(tc_streaming, test_streaming_callback_with_empty_lines);
    tcase_add_test(tc_streaming, test_streaming_callback_buffered_line_flush);
    tcase_add_test(tc_streaming, test_new_message_clears_streaming_buffer);
    tcase_add_test(tc_streaming, test_submission_with_debug_enabled);
    suite_add_tcase(s, tc_streaming);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_streaming_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
