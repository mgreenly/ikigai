#include "tests/test_constants.h"
/**
 * @file tool_loop_continuation_test.c
 * @brief Regression test for tool loop continuation serialization
 *
 * Tests that the Anthropic serializer can handle the message sequence that
 * occurs during tool loop continuation: thinking + tool_call (assistant)
 * followed by tool_result (user).
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/request_serialize.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper_json.h"

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
 * Tool Loop Continuation - Success Path
 * ================================================================ */

START_TEST(test_serialize_tool_loop_continuation_with_thinking) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 3;
    ik_message_t messages[3];

    // Message 1: User prompt
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t user_blocks[1];
    user_blocks[0].type = IK_CONTENT_TEXT;
    user_blocks[0].data.text.text = talloc_strdup(test_ctx, "Please run noop");
    messages[0].content_blocks = user_blocks;

    // Message 2: Assistant with thinking + tool_call (this is what fails)
    messages[1].role = IK_ROLE_ASSISTANT;
    messages[1].content_count = 2;
    ik_content_block_t assistant_blocks[2];

    assistant_blocks[0].type = IK_CONTENT_THINKING;
    assistant_blocks[0].data.thinking.text = talloc_strdup(test_ctx, "I should run the noop tool");
    assistant_blocks[0].data.thinking.signature = NULL;

    assistant_blocks[1].type = IK_CONTENT_TOOL_CALL;
    assistant_blocks[1].data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    assistant_blocks[1].data.tool_call.name = talloc_strdup(test_ctx, "noop");
    // This is the key test case: empty arguments that should be treated as "{}"
    assistant_blocks[1].data.tool_call.arguments = talloc_strdup(test_ctx, "");

    messages[1].content_blocks = assistant_blocks;

    // Message 3: Tool result
    messages[2].role = IK_ROLE_TOOL;
    messages[2].content_count = 1;
    ik_content_block_t tool_blocks[1];
    tool_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    tool_blocks[0].data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    tool_blocks[0].data.tool_result.content = talloc_strdup(test_ctx, "{}");
    tool_blocks[0].data.tool_result.is_error = false;
    messages[2].content_blocks = tool_blocks;

    req.messages = messages;

    // This should succeed - empty arguments should be treated as "{}"
    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(result);

    // Verify messages array was added
    yyjson_mut_val *messages_arr = yyjson_mut_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages_arr);
    ck_assert(yyjson_mut_is_arr(messages_arr));
    ck_assert_int_eq((int)yyjson_mut_arr_size(messages_arr), 3);

    // Verify assistant message has tool_use with input
    yyjson_mut_val *assistant_msg = yyjson_mut_arr_get(messages_arr, 1);
    ck_assert_ptr_nonnull(assistant_msg);

    yyjson_mut_val *content = yyjson_mut_obj_get(assistant_msg, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_arr(content));
    ck_assert_int_eq((int)yyjson_mut_arr_size(content), 2);

    // First block should be thinking
    yyjson_mut_val *thinking_block = yyjson_mut_arr_get(content, 0);
    yyjson_mut_val *thinking_type = yyjson_mut_obj_get(thinking_block, "type");
    ck_assert_str_eq(yyjson_mut_get_str(thinking_type), "thinking");

    // Second block should be tool_use with input={}
    yyjson_mut_val *tool_block = yyjson_mut_arr_get(content, 1);
    yyjson_mut_val *tool_type = yyjson_mut_obj_get(tool_block, "type");
    ck_assert_str_eq(yyjson_mut_get_str(tool_type), "tool_use");

    yyjson_mut_val *input = yyjson_mut_obj_get(tool_block, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_mut_is_obj(input));

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_tool_loop_continuation_with_null_arguments) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 2;
    ik_message_t messages[2];

    // Message 1: User prompt
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t user_blocks[1];
    user_blocks[0].type = IK_CONTENT_TEXT;
    user_blocks[0].data.text.text = talloc_strdup(test_ctx, "Please run noop");
    messages[0].content_blocks = user_blocks;

    // Message 2: Assistant with tool_call with NULL arguments
    messages[1].role = IK_ROLE_ASSISTANT;
    messages[1].content_count = 1;
    ik_content_block_t assistant_blocks[1];

    assistant_blocks[0].type = IK_CONTENT_TOOL_CALL;
    assistant_blocks[0].data.tool_call.id = talloc_strdup(test_ctx, "call_456");
    assistant_blocks[0].data.tool_call.name = talloc_strdup(test_ctx, "noop");
    assistant_blocks[0].data.tool_call.arguments = NULL;  // NULL should be treated as "{}"

    messages[1].content_blocks = assistant_blocks;

    req.messages = messages;

    // This should succeed - NULL arguments should be treated as "{}"
    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(result);

    // Verify the tool_use has input={}
    yyjson_mut_val *messages_arr = yyjson_mut_obj_get(root, "messages");
    yyjson_mut_val *assistant_msg = yyjson_mut_arr_get(messages_arr, 1);
    yyjson_mut_val *content = yyjson_mut_obj_get(assistant_msg, "content");

    // Content should be array format for tool_call
    ck_assert(yyjson_mut_is_arr(content));
    yyjson_mut_val *tool_block = yyjson_mut_arr_get(content, 0);
    yyjson_mut_val *input = yyjson_mut_obj_get(tool_block, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_mut_is_obj(input));

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *tool_loop_continuation_suite(void)
{
    Suite *s = suite_create("Tool Loop Continuation");

    TCase *tc_continuation = tcase_create("Continuation Serialization");
    tcase_set_timeout(tc_continuation, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_continuation, setup, teardown);
    tcase_add_test(tc_continuation, test_serialize_tool_loop_continuation_with_thinking);
    tcase_add_test(tc_continuation, test_serialize_tool_loop_continuation_with_null_arguments);
    suite_add_tcase(s, tc_continuation);

    return s;
}

int main(void)
{
    Suite *s = tool_loop_continuation_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/tool_loop_continuation_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = (int32_t)srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
