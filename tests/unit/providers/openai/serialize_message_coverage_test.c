/**
 * @file serialize_message_coverage_test.c
 * @brief Coverage tests for serialize.c message serialization
 *
 * Tests coverage gaps in ik_openai_serialize_message for tool results,
 * tool calls, and multi-block text concatenation.
 */

#include "providers/openai/serialize.h"
#include "providers/request.h"
#include "message.h"
#include "error.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

static void *ctx;
static yyjson_mut_doc *doc;

static void setup(void)
{
    ctx = talloc_new(NULL);
    doc = yyjson_mut_doc_new(NULL);
}

static void teardown(void)
{
    yyjson_mut_doc_free(doc);
    talloc_free(ctx);
}

/* Test: Serialize IK_ROLE_TOOL message with tool result */
START_TEST(test_tool_result_message) {
    ik_message_t *msg = ik_message_create_tool_result(ctx, "call_123", "Success", false);
    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_obj_get(result, "role")), "tool");
    ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_obj_get(result, "tool_call_id")), "call_123");
    ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_obj_get(result, "content")), "Success");
}
END_TEST

/* Test: Serialize assistant message with tool call */
START_TEST(test_assistant_tool_call) {
    ik_message_t *msg = ik_message_create_tool_call(ctx, "call_456", "get_weather", "{\"city\":\"Paris\"}");
    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_obj_get(result, "role")), "assistant");
    ck_assert(yyjson_mut_is_null(yyjson_mut_obj_get(result, "content")));

    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(result, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert_uint_eq(yyjson_mut_arr_size(tool_calls), 1);
}
END_TEST

/* Test: Multiple text blocks concatenated with \n\n */
START_TEST(test_multiple_text_blocks) {
    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 3;
    msg->content_blocks = talloc_array(ctx, ik_content_block_t, 3);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(ctx, "First");
    msg->content_blocks[1].type = IK_CONTENT_TEXT;
    msg->content_blocks[1].data.text.text = talloc_strdup(ctx, "Second");
    msg->content_blocks[2].type = IK_CONTENT_TEXT;
    msg->content_blocks[2].data.text.text = talloc_strdup(ctx, "Third");

    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);
    const char *content = yyjson_mut_get_str(yyjson_mut_obj_get(result, "content"));
    ck_assert_str_eq(content, "First\n\nSecond\n\nThird");
}
END_TEST

/* Test: Empty content (zero blocks) */
START_TEST(test_empty_content) {
    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 0;
    msg->content_blocks = NULL;

    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);
    const char *content = yyjson_mut_get_str(yyjson_mut_obj_get(result, "content"));
    ck_assert_str_eq(content, "");
}
END_TEST

/* Test: Multiple tool calls in one message */
START_TEST(test_multiple_tool_calls) {
    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 2;
    msg->content_blocks = talloc_array(ctx, ik_content_block_t, 2);

    msg->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[0].data.tool_call.id = talloc_strdup(ctx, "call_1");
    msg->content_blocks[0].data.tool_call.name = talloc_strdup(ctx, "tool1");
    msg->content_blocks[0].data.tool_call.arguments = talloc_strdup(ctx, "{}");

    msg->content_blocks[1].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[1].data.tool_call.id = talloc_strdup(ctx, "call_2");
    msg->content_blocks[1].data.tool_call.name = talloc_strdup(ctx, "tool2");
    msg->content_blocks[1].data.tool_call.arguments = talloc_strdup(ctx, "{}");

    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);
    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(result, "tool_calls");
    ck_assert_uint_eq(yyjson_mut_arr_size(tool_calls), 2);
}
END_TEST

/* Test: Mixed content blocks (text + thinking) to hit branch coverage */
START_TEST(test_mixed_content_types) {
    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 2;
    msg->content_blocks = talloc_array(ctx, ik_content_block_t, 2);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(ctx, "Text");
    msg->content_blocks[1].type = IK_CONTENT_THINKING;
    msg->content_blocks[1].data.thinking.text = talloc_strdup(ctx, "Think");

    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);
    const char *content = yyjson_mut_get_str(yyjson_mut_obj_get(result, "content"));
    ck_assert_str_eq(content, "Text"); /* Only text, not thinking */
}
END_TEST

/* Test: Tool role without tool_result content */
START_TEST(test_tool_role_no_result) {
    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(ctx, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(ctx, "Text");

    yyjson_mut_val *result = ik_openai_serialize_message(doc, msg);
    ck_assert_str_eq(yyjson_mut_get_str(yyjson_mut_obj_get(result, "role")), "tool");
}
END_TEST

static Suite *serialize_message_suite(void)
{
    Suite *s = suite_create("serialize_message_coverage");
    TCase *tc = tcase_create("core");
    tcase_set_timeout(tc, 30);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_tool_result_message);
    tcase_add_test(tc, test_assistant_tool_call);
    tcase_add_test(tc, test_multiple_text_blocks);
    tcase_add_test(tc, test_empty_content);
    tcase_add_test(tc, test_multiple_tool_calls);
    tcase_add_test(tc, test_mixed_content_types);
    tcase_add_test(tc, test_tool_role_no_result);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = serialize_message_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
