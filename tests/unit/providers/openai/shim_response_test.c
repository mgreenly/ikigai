#include "providers/openai/shim.h"
#include "providers/provider.h"
#include "msg.h"
#include "error.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Helper to create legacy message */
static ik_msg_t *create_legacy_msg(TALLOC_CTX *ctx, const char *kind, const char *content, const char *data_json)
{
    ik_msg_t *msg = talloc_zero(ctx, ik_msg_t);
    msg->id = 0;
    msg->kind = talloc_strdup(msg, kind);
    msg->content = content ? talloc_strdup(msg, content) : NULL;
    msg->data_json = data_json ? talloc_strdup(msg, data_json) : NULL;
    return msg;
}

/* ================================================================
 * Response Transformation Tests
 * ================================================================ */

START_TEST(test_transform_response_text)
{
    /* Create legacy message with text response */
    ik_msg_t *legacy_msg = create_legacy_msg(test_ctx, "assistant", "Hello there", NULL);

    /* Transform to normalized format */
    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(response);
    ck_assert(response->content_count == 1);
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(response->content_blocks[0].data.text.text, "Hello there");
    ck_assert_int_eq(response->finish_reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_transform_response_tool_call)
{
    /* Create legacy message with tool call */
    const char *data_json = "{\"id\":\"call_123\",\"name\":\"read_file\",\"arguments\":\"{\\\"path\\\":\\\"/etc/hosts\\\"}\"}";
    ik_msg_t *legacy_msg = create_legacy_msg(test_ctx, "tool_call", "read_file(...)", data_json);

    /* Transform to normalized format */
    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(response);
    ck_assert(response->content_count == 1);
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.id, "call_123");
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.name, "read_file");
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.arguments, "{\"path\":\"/etc/hosts\"}");
    ck_assert_int_eq(response->finish_reason, IK_FINISH_TOOL_USE);
}
END_TEST

START_TEST(test_transform_response_tool_call_null_data_json)
{
    /* Tool call with NULL data_json should fail */
    ik_msg_t *legacy_msg = create_legacy_msg(test_ctx, "tool_call", "read_file(...)", NULL);

    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}
END_TEST

START_TEST(test_transform_response_tool_call_invalid_json)
{
    /* Tool call with invalid JSON should fail */
    ik_msg_t *legacy_msg = create_legacy_msg(test_ctx, "tool_call", "read_file(...)", "{invalid json");

    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}
END_TEST

START_TEST(test_transform_response_tool_call_missing_fields)
{
    /* Tool call with missing fields should fail */
    const char *data_json = "{\"id\":\"call_123\"}"; /* missing name and arguments */
    ik_msg_t *legacy_msg = create_legacy_msg(test_ctx, "tool_call", "read_file(...)", data_json);

    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}
END_TEST

START_TEST(test_transform_response_unknown_kind)
{
    /* Unknown kind should be treated as text with empty content */
    ik_msg_t *legacy_msg = create_legacy_msg(test_ctx, "unknown_kind", "some content", NULL);

    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(response->content_blocks[0].data.text.text, "");
    ck_assert_int_eq(response->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * Finish Reason Mapping Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_stop)
{
    ik_finish_reason_t reason = ik_openai_shim_map_finish_reason("stop");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_map_finish_reason_length)
{
    ik_finish_reason_t reason = ik_openai_shim_map_finish_reason("length");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_map_finish_reason_tool_calls)
{
    ik_finish_reason_t reason = ik_openai_shim_map_finish_reason("tool_calls");
    ck_assert_int_eq(reason, IK_FINISH_TOOL_USE);
}
END_TEST

START_TEST(test_map_finish_reason_content_filter)
{
    ik_finish_reason_t reason = ik_openai_shim_map_finish_reason("content_filter");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_null)
{
    ik_finish_reason_t reason = ik_openai_shim_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_map_finish_reason_unknown)
{
    ik_finish_reason_t reason = ik_openai_shim_map_finish_reason("some_unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * Round-Trip Tests
 * ================================================================ */

START_TEST(test_roundtrip_text_message)
{
    /* Create normalized request with text message */
    ik_message_t msg_in = {0};
    msg_in.role = IK_ROLE_USER;
    msg_in.content_count = 1;

    ik_content_block_t block_in = {0};
    block_in.type = IK_CONTENT_TEXT;
    block_in.data.text.text = talloc_strdup(test_ctx, "test message");
    msg_in.content_blocks = &block_in;

    /* Transform to legacy */
    ik_msg_t *legacy_msg = NULL;
    res_t to_legacy = ik_openai_shim_transform_message(test_ctx, &msg_in, &legacy_msg);
    ck_assert(!is_err(&to_legacy));

    /* Simulate response (change kind to assistant) */
    talloc_free(legacy_msg->kind);
    legacy_msg->kind = talloc_strdup(legacy_msg, "assistant");

    /* Transform back to normalized */
    ik_response_t *response = NULL;
    res_t to_norm = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);
    ck_assert(!is_err(&to_norm));

    /* Verify content preserved */
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(response->content_blocks[0].data.text.text, "test message");
}
END_TEST

START_TEST(test_roundtrip_tool_call)
{
    /* Create normalized message with tool call */
    ik_message_t msg_in = {0};
    msg_in.role = IK_ROLE_ASSISTANT;
    msg_in.content_count = 1;

    ik_content_block_t block_in = {0};
    block_in.type = IK_CONTENT_TOOL_CALL;
    block_in.data.tool_call.id = talloc_strdup(test_ctx, "call_roundtrip");
    block_in.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block_in.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"arg\":\"value\"}");
    msg_in.content_blocks = &block_in;

    /* Transform to legacy */
    ik_msg_t *legacy_msg = NULL;
    res_t to_legacy = ik_openai_shim_transform_message(test_ctx, &msg_in, &legacy_msg);
    ck_assert(!is_err(&to_legacy));

    /* Transform back to normalized */
    ik_response_t *response = NULL;
    res_t to_norm = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);
    ck_assert(!is_err(&to_norm));

    /* Verify all fields preserved */
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.id, "call_roundtrip");
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.name, "test_tool");
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.arguments, "{\"arg\":\"value\"}");
}
END_TEST

/* ================================================================
 * Test Suite Definition
 * ================================================================ */

static Suite *shim_response_suite(void)
{
    Suite *s = suite_create("OpenAI Shim Response Transform");

    /* Response transformation tests */
    TCase *tc_resp = tcase_create("Response Transform");
    tcase_add_checked_fixture(tc_resp, setup, teardown);
    tcase_add_test(tc_resp, test_transform_response_text);
    tcase_add_test(tc_resp, test_transform_response_tool_call);
    tcase_add_test(tc_resp, test_transform_response_tool_call_null_data_json);
    tcase_add_test(tc_resp, test_transform_response_tool_call_invalid_json);
    tcase_add_test(tc_resp, test_transform_response_tool_call_missing_fields);
    tcase_add_test(tc_resp, test_transform_response_unknown_kind);
    suite_add_tcase(s, tc_resp);

    /* Finish reason mapping tests */
    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_add_test(tc_finish, test_map_finish_reason_stop);
    tcase_add_test(tc_finish, test_map_finish_reason_length);
    tcase_add_test(tc_finish, test_map_finish_reason_tool_calls);
    tcase_add_test(tc_finish, test_map_finish_reason_content_filter);
    tcase_add_test(tc_finish, test_map_finish_reason_null);
    tcase_add_test(tc_finish, test_map_finish_reason_unknown);
    suite_add_tcase(s, tc_finish);

    /* Round-trip tests */
    TCase *tc_roundtrip = tcase_create("Round-Trip");
    tcase_add_checked_fixture(tc_roundtrip, setup, teardown);
    tcase_add_test(tc_roundtrip, test_roundtrip_text_message);
    tcase_add_test(tc_roundtrip, test_roundtrip_tool_call);
    suite_add_tcase(s, tc_roundtrip);

    return s;
}

int main(void)
{
    int failed;
    Suite *s = shim_response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
