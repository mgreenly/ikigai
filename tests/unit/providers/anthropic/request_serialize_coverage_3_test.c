/**
 * @file request_serialize_coverage_test_3.c
 * @brief Coverage tests for Anthropic request serialization - Part 3: Specific Branch Coverage
 *
 * This test file targets specific uncovered branches in the serialization code,
 * particularly the second and subsequent field additions for each content type.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request_serialize.h"
#include "providers/provider.h"
#include "wrapper_json.h"

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
 * Mock Overrides with Call Counter
 * ================================================================ */

static int mock_yyjson_mut_obj_add_str_fail_on_call = -1;
static int mock_yyjson_mut_obj_add_str_call_count = 0;
static int mock_yyjson_mut_obj_add_bool_fail_on_call = -1;
static int mock_yyjson_mut_obj_add_bool_call_count = 0;

bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    mock_yyjson_mut_obj_add_str_call_count++;
    if (mock_yyjson_mut_obj_add_str_fail_on_call == mock_yyjson_mut_obj_add_str_call_count) {
        return false;
    }
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                              const char *key, bool val)
{
    mock_yyjson_mut_obj_add_bool_call_count++;
    if (mock_yyjson_mut_obj_add_bool_fail_on_call == mock_yyjson_mut_obj_add_bool_call_count) {
        return false;
    }
    return yyjson_mut_obj_add_bool(doc, obj, key, val);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void reset_mocks(void)
{
    mock_yyjson_mut_obj_add_str_fail_on_call = -1;
    mock_yyjson_mut_obj_add_str_call_count = 0;
    mock_yyjson_mut_obj_add_bool_fail_on_call = -1;
    mock_yyjson_mut_obj_add_bool_call_count = 0;
}

/* ================================================================
 * Content Block Serialization - Specific Field Failure Tests
 * ================================================================ */

START_TEST(test_serialize_content_block_text_text_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello");

    // Fail on the 2nd call (adding "text" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_thinking_thinking_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Thinking...");

    // Fail on the 2nd call (adding "thinking" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_id_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{}");

    // Fail on the 2nd call (adding "id" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_name_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{}");

    // Fail on the 3rd call (adding "name" field, after "type" and "id" succeed)
    mock_yyjson_mut_obj_add_str_fail_on_call = 3;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_tool_use_id_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    // Fail on the 2nd call (adding "tool_use_id" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_content_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    // Fail on the 3rd call (adding "content" field, after "type" and "tool_use_id" succeed)
    mock_yyjson_mut_obj_add_str_fail_on_call = 3;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_is_error_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    // Fail on the 1st bool call (adding "is_error" field)
    mock_yyjson_mut_obj_add_bool_fail_on_call = 1;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_thinking_signature_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Thinking...");
    block.data.thinking.signature = talloc_strdup(test_ctx, "sig123");

    // Fail on the 3rd call (adding "signature" field, after "type" and "thinking" succeed)
    mock_yyjson_mut_obj_add_str_fail_on_call = 3;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_redacted_thinking_data_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_REDACTED_THINKING;
    block.data.redacted_thinking.data = talloc_strdup(test_ctx, "redacted_data");

    // Fail on the 2nd call (adding "data" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_invalid_json) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    // Invalid JSON - should fail to parse
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{invalid json");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    // Should return false for invalid JSON
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_invalid_type) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    // Use an invalid type value outside the enum range
    block.type = (ik_content_type_t)999;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    // Should return false for invalid type
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Message Content Serialization - Loop Failure Tests
 * ================================================================ */

START_TEST(test_serialize_message_content_block_fail_in_loop) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 3;
    ik_content_block_t blocks[3];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "First");
    blocks[1].type = IK_CONTENT_TEXT;
    blocks[1].data.text.text = talloc_strdup(test_ctx, "Second");
    blocks[2].type = IK_CONTENT_TEXT;
    blocks[2].data.text.text = talloc_strdup(test_ctx, "Third");
    message.content_blocks = blocks;

    // Fail on the 4th call to obj_add_str (second block's "text" field)
    // Calls: 1="type" for block[0], 2="text" for block[0], 3="type" for block[1], 4="text" for block[1] <- FAIL
    mock_yyjson_mut_obj_add_str_fail_on_call = 4;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Message Serialization - Loop Failure Tests
 * ================================================================ */

START_TEST(test_serialize_messages_content_fail_in_loop) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 3;
    ik_message_t messages[3];

    // First message
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks0[1];
    blocks0[0].type = IK_CONTENT_TEXT;
    blocks0[0].data.text.text = talloc_strdup(test_ctx, "First message");
    messages[0].content_blocks = blocks0;

    // Second message - this will fail
    messages[1].role = IK_ROLE_ASSISTANT;
    messages[1].content_count = 1;
    ik_content_block_t blocks1[1];
    blocks1[0].type = IK_CONTENT_TEXT;
    blocks1[0].data.text.text = talloc_strdup(test_ctx, "Second message");
    messages[1].content_blocks = blocks1;

    // Third message
    messages[2].role = IK_ROLE_USER;
    messages[2].content_count = 1;
    ik_content_block_t blocks2[1];
    blocks2[0].type = IK_CONTENT_TEXT;
    blocks2[0].data.text.text = talloc_strdup(test_ctx, "Third message");
    messages[2].content_blocks = blocks2;

    req.messages = messages;

    // Fail on the 4th call to obj_add_str (second message's "content" field)
    // Calls for msg[0]: 1="role", 2="content"
    // Calls for msg[1]: 3="role", 4="content" <- FAIL
    mock_yyjson_mut_obj_add_str_fail_on_call = 4;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_serialize_coverage_suite_3(void)
{
    Suite *s = suite_create("Anthropic Request Serialize Coverage - Part 3");

    TCase *tc_specific = tcase_create("Specific Field Failures");
    tcase_set_timeout(tc_specific, 30);
    tcase_add_unchecked_fixture(tc_specific, setup, teardown);
    tcase_add_test(tc_specific, test_serialize_content_block_text_text_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_thinking_thinking_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_thinking_signature_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_call_id_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_call_name_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_call_invalid_json);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_result_tool_use_id_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_result_content_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_result_is_error_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_redacted_thinking_data_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_invalid_type);
    suite_add_tcase(s, tc_specific);

    TCase *tc_loop = tcase_create("Loop Failure Coverage");
    tcase_set_timeout(tc_loop, 30);
    tcase_add_unchecked_fixture(tc_loop, setup, teardown);
    tcase_add_test(tc_loop, test_serialize_message_content_block_fail_in_loop);
    tcase_add_test(tc_loop, test_serialize_messages_content_fail_in_loop);
    suite_add_tcase(s, tc_loop);

    return s;
}

int main(void)
{
    Suite *s = request_serialize_coverage_suite_3();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
