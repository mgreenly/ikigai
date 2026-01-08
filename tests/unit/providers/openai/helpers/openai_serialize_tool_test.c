/**
 * @file openai_serialize_tool_test.c
 * @brief Unit tests for OpenAI tool message serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/serialize.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"
#include "openai_serialize_tool_test.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Tool Message Tests
 * ================================================================ */

START_TEST(test_serialize_tool_message) {
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    msg->content_blocks[0].data.tool_result.tool_call_id = talloc_strdup(msg, "call_123");
    msg->content_blocks[0].data.tool_result.content = talloc_strdup(msg, "Tool result");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "tool");

    yyjson_mut_val *tool_call_id = yyjson_mut_obj_get(val, "tool_call_id");
    ck_assert_ptr_nonnull(tool_call_id);
    ck_assert_str_eq(yyjson_mut_get_str(tool_call_id), "call_123");

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "Tool result");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_tool_message_empty_content) {
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 0;
    msg->content_blocks = NULL;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "tool");

    // Should not have tool_call_id or content fields
    yyjson_mut_val *tool_call_id = yyjson_mut_obj_get(val, "tool_call_id");
    ck_assert_ptr_null(tool_call_id);

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_tool_message_wrong_block_type) {
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;  // Wrong type
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Text");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "tool");

    // Should not have tool_call_id or content since block type is wrong
    yyjson_mut_val *tool_call_id = yyjson_mut_obj_get(val, "tool_call_id");
    ck_assert_ptr_null(tool_call_id);

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

Suite *openai_serialize_tool_suite(void)
{
    Suite *s = suite_create("OpenAI Serialize Tool Messages");

    TCase *tc_tool = tcase_create("Tool Messages");
    tcase_set_timeout(tc_tool, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_tool, setup, teardown);
    tcase_add_test(tc_tool, test_serialize_tool_message);
    tcase_add_test(tc_tool, test_serialize_tool_message_empty_content);
    tcase_add_test(tc_tool, test_serialize_tool_message_wrong_block_type);
    suite_add_tcase(s, tc_tool);

    return s;
}
