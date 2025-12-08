/* Integration tests for http_handler.c tool call handling */

#include "client_http_test_common.h"

/*
 * Test tool call handling in http_write_callback
 *
 * Covers:
 * - Lines 162-176: Tool call extraction and accumulation
 * - Line 287: Tool call transfer to response
 */

START_TEST(test_tool_call_single_chunk) {
    /* Set up mock response with tool call */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_abc\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\": \\\"*.c\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
    set_mock_response(response, strlen(response));

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Verify tool call was extracted and transferred to response */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;
    ck_assert_ptr_nonnull(msg->tool_call);
    ck_assert_str_eq(msg->tool_call->id, "call_abc");
    ck_assert_str_eq(msg->tool_call->name, "glob");
    ck_assert_str_eq(msg->tool_call->arguments, "{\"pattern\": \"*.c\"}");
    ck_assert_str_eq(msg->finish_reason, "tool_calls");
}
END_TEST

START_TEST(test_tool_call_streaming_chunks) {
    /* Set up mock response with tool call split across chunks */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_xyz\",\"type\":\"function\",\"function\":{\"name\":\"file_read\",\"arguments\":\"{\\\"pa\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"th\\\": \\\"tes\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"t.txt\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
    set_mock_response(response, strlen(response));

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Verify tool call arguments were accumulated */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;
    ck_assert_ptr_nonnull(msg->tool_call);
    ck_assert_str_eq(msg->tool_call->id, "call_xyz");
    ck_assert_str_eq(msg->tool_call->name, "file_read");
    ck_assert_str_eq(msg->tool_call->arguments, "{\"path\": \"test.txt\"}");
    ck_assert_str_eq(msg->finish_reason, "tool_calls");
}
END_TEST

START_TEST(test_tool_call_no_content) {
    /* Tool call with no text content */
    const char *response =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_grep\",\"type\":\"function\",\"function\":{\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\": \\\"TODO\\\"}\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n";
    set_mock_response(response, strlen(response));

    /* Execute request */
    res_t result = ik_openai_chat_create(ctx, cfg, conv, NULL, NULL);

    /* Verify tool call exists and content is empty */
    ck_assert(!result.is_err);
    ik_msg_t *msg = result.ok;
    ck_assert_ptr_nonnull(msg->tool_call);
    ck_assert_str_eq(msg->tool_call->id, "call_grep");
    ck_assert_str_eq(msg->tool_call->name, "grep");
    ck_assert_str_eq(msg->content, "");
}
END_TEST

static Suite *http_handler_tool_calls_suite(void) {
    Suite *s = suite_create("HTTP Handler Tool Calls");

    TCase *tc = tcase_create("Tool Calls");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_tool_call_single_chunk);
    tcase_add_test(tc, test_tool_call_streaming_chunks);
    tcase_add_test(tc, test_tool_call_no_content);
    suite_add_tcase(s, tc);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = http_handler_tool_calls_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
