/**
 * @file repl_streaming_test.c
 * @brief Unit tests for REPL streaming callback (Phase 1.6)
 *
 * Tests the streaming callback code paths.
 */

#include "repl_streaming_test_common.h"

/* Test: Streaming callback receives data and appends to scrollback */
START_TEST(test_streaming_callback_appends_to_scrollback) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    g_write_callback = NULL;
    g_write_data = NULL;
    invoke_write_callback = false;

    // Type a message
    const char *message = "Test";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Submit message
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify state transitioned to WAITING_FOR_LLM
    ck_assert_int_eq(repl->state, IK_REPL_STATE_WAITING_FOR_LLM);

    // Simulate receiving SSE data
    const char *sse_data = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello world\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    // Call multi_perform to trigger the write callback
    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify streaming data was appended to scrollback
    // The scrollback should now have the user message "Test" + blank line
    // Note: "Hello world" (no trailing newline) is buffered but not added to scrollback yet
    size_t line_count = ik_scrollback_get_line_count(repl->scrollback);
    ck_assert_uint_eq(line_count, 2); // User message + blank line (streamed content has no newline)

    // Verify assistant_response was accumulated
    ck_assert_ptr_nonnull(repl->assistant_response);
    ck_assert(strlen(repl->assistant_response) > 0);

    // Clean up
    invoke_write_callback = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Multiple streaming chunks accumulate assistant_response */
START_TEST(test_streaming_callback_accumulates_response)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    g_write_callback = NULL;
    g_write_data = NULL;
    invoke_write_callback = false;

    // Type and submit a message
    const char *message = "Hi";
    for (size_t i = 0; message[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)message[i]};
        res_t res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Simulate first chunk
    const char *chunk1 = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = chunk1;
    mock_response_len = strlen(chunk1);
    invoke_write_callback = true;

    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify first chunk was accumulated
    ck_assert_ptr_nonnull(repl->assistant_response);
    size_t len_after_first = strlen(repl->assistant_response);
    ck_assert(len_after_first > 0);

    // Simulate second chunk
    const char *chunk2 = "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n\n";
    mock_response_data = chunk2;
    mock_response_len = strlen(chunk2);

    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify second chunk was appended
    ck_assert_ptr_nonnull(repl->assistant_response);
    size_t len_after_second = strlen(repl->assistant_response);
    ck_assert(len_after_second > len_after_first); // Should be longer now

    // Clean up
    invoke_write_callback = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Streaming callback with empty chunk (Task 6.1: empty output check) */
START_TEST(test_streaming_callback_empty_chunk)
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

    // Get initial scrollback line count (should have user message)
    size_t initial_count = ik_scrollback_get_line_count(repl->scrollback);

    // Simulate receiving SSE data with empty content
    const char *sse_empty = "data: {\"choices\":[{\"delta\":{\"content\":\"\"}}]}\n\n";
    mock_response_data = sse_empty;
    mock_response_len = strlen(sse_empty);
    invoke_write_callback = true;

    // Call multi_perform to trigger the write callback with empty content
    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify scrollback count unchanged (empty chunk should not add lines)
    size_t after_count = ik_scrollback_get_line_count(repl->scrollback);
    ck_assert_uint_eq(after_count, initial_count);

    // However, assistant_response should still be initialized (even if empty)
    ck_assert_ptr_nonnull(repl->assistant_response);
    ck_assert_uint_eq(strlen(repl->assistant_response), 0);

    // Clean up
    invoke_write_callback = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Streaming content ending with newline (cover line 37 branch) */
START_TEST(test_streaming_callback_content_ending_with_newline)
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

    size_t initial_count = ik_scrollback_get_line_count(repl->scrollback);

    // Simulate receiving SSE data with content ending with newline
    // "Test\\n" should add one line "Test" and leave nothing in the buffer
    const char *sse_data = "data: {\"choices\":[{\"delta\":{\"content\":\"Test\\n\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    // Call multi_perform to trigger the write callback
    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify scrollback has one new line added
    size_t after_count = ik_scrollback_get_line_count(repl->scrollback);
    ck_assert_uint_eq(after_count, initial_count + 1);

    // Verify the streaming buffer is empty (newline was consumed)
    ck_assert_ptr_null(repl->streaming_line_buffer);

    // Clean up
    invoke_write_callback = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Streaming callback with empty lines in content (Task 6.2: line length branch) */
START_TEST(test_streaming_callback_with_empty_lines)
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

    size_t initial_count = ik_scrollback_get_line_count(repl->scrollback);

    // Simulate receiving SSE data with content containing empty lines in the middle
    // "Hello\n\nWorld" should create 3 lines: "Hello", "", "World"
    const char *sse_multiline = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\\n\\nWorld\"}}]}\n\n";
    mock_response_data = sse_multiline;
    mock_response_len = strlen(sse_multiline);
    invoke_write_callback = true;

    // Call multi_perform to trigger the write callback
    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify scrollback has new lines added
    // Should have: initial + "Hello" + "" = initial + 2
    // Note: "World" (no trailing newline) is buffered but not added to scrollback yet
    size_t after_count = ik_scrollback_get_line_count(repl->scrollback);
    ck_assert_uint_eq(after_count, initial_count + 2);

    // Verify the content of the added lines
    const char *line_text = NULL;
    size_t line_len = 0;

    // First line should be "Hello"
    res = ik_scrollback_get_line_text(repl->scrollback, initial_count, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 5);
    ck_assert_mem_eq(line_text, "Hello", 5);

    // Second line should be empty
    res = ik_scrollback_get_line_text(repl->scrollback, initial_count + 1, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 0);

    // "World" should be in the streaming_line_buffer (not in scrollback yet)
    ck_assert_ptr_nonnull(repl->streaming_line_buffer);
    ck_assert_str_eq(repl->streaming_line_buffer, "World");

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

    size_t initial_count = ik_scrollback_get_line_count(repl->scrollback);

    // First chunk: content without trailing newline (gets buffered)
    const char *chunk1 = "data: {\"choices\":[{\"delta\":{\"content\":\"First\"}}]}\n\n";
    mock_response_data = chunk1;
    mock_response_len = strlen(chunk1);
    invoke_write_callback = true;

    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify "First" is buffered (not in scrollback yet)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), initial_count);
    ck_assert_ptr_nonnull(repl->streaming_line_buffer);
    ck_assert_str_eq(repl->streaming_line_buffer, "First");

    // Second chunk: content with newline (flushes buffered content)
    const char *chunk2 = "data: {\"choices\":[{\"delta\":{\"content\":\" part\\nSecond part\"}}]}\n\n";
    mock_response_data = chunk2;
    mock_response_len = strlen(chunk2);

    res = ik_openai_multi_perform(repl->multi, &repl->curl_still_running);
    ck_assert(is_ok(&res));

    // Verify "First part" was flushed to scrollback
    size_t after_count = ik_scrollback_get_line_count(repl->scrollback);
    ck_assert_uint_eq(after_count, initial_count + 1);

    const char *line_text = NULL;
    size_t line_len = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, initial_count, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 10);
    ck_assert_mem_eq(line_text, "First part", 10);

    // Verify "Second part" is now in the buffer
    ck_assert_ptr_nonnull(repl->streaming_line_buffer);
    ck_assert_str_eq(repl->streaming_line_buffer, "Second part");

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
    repl->streaming_line_buffer = talloc_strdup(repl, "buffered content");
    ck_assert_ptr_nonnull(repl->streaming_line_buffer);

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
    ck_assert_ptr_null(repl->streaming_line_buffer);

    // Verify state transitioned to WAITING_FOR_LLM
    ck_assert_int_eq(repl->state, IK_REPL_STATE_WAITING_FOR_LLM);

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
    repl->openai_debug_pipe = res.ok;
    repl->debug_enabled = true;

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
    ck_assert_int_eq(repl->state, IK_REPL_STATE_WAITING_FOR_LLM);

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_streaming_suite(void)
{
    Suite *s = suite_create("REPL Streaming");

    TCase *tc_streaming = tcase_create("Streaming");
    tcase_set_timeout(tc_streaming, 30);
    tcase_add_test(tc_streaming, test_streaming_callback_appends_to_scrollback);
    tcase_add_test(tc_streaming, test_streaming_callback_accumulates_response);
    tcase_add_test(tc_streaming, test_streaming_callback_empty_chunk);
    tcase_add_test(tc_streaming, test_streaming_callback_content_ending_with_newline);
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
