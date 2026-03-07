#include "tests/test_constants.h"
// Unit tests for Google request message parts serialization
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/request_helpers.h"
#include "apps/ikigai/providers/provider.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_serialize_message_parts_basic) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_USER;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc,
                                                    content_obj,
                                                    &message,
                                                    NULL,
                                                    false,
                                                    "gemini-2.5-pro",
                                                    NULL,
                                                    0,
                                                    0);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_parts_with_thought_signature) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc,
                                                    content_obj,
                                                    &message,
                                                    "sig-123",
                                                    true,
                                                    "gemini-2.5-pro",
                                                    NULL,
                                                    0,
                                                    0);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 2);

    // Check thought signature is first
    yyjson_mut_val *first_part = yyjson_mut_arr_get_first(parts);
    yyjson_mut_val *sig_val = yyjson_mut_obj_get(first_part, "thoughtSignature");
    ck_assert_str_eq(yyjson_mut_get_str(sig_val), "sig-123");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_parts_thought_not_first) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";
    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;
    bool result = ik_google_serialize_message_parts(doc, obj, &message, "sig-123", false, "gemini-2.5-pro", NULL, 0, 0);
    ck_assert(result);
    yyjson_mut_val *parts = yyjson_mut_obj_get(obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);
    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_parts_with_tool_call) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc,
                                                    content_obj,
                                                    &message,
                                                    NULL,
                                                    false,
                                                    "gemini-2.5-pro",
                                                    NULL,
                                                    0,
                                                    0);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_parts_with_tool_result) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    ik_message_t message = {0};
    message.role = IK_ROLE_TOOL;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc,
                                                    content_obj,
                                                    &message,
                                                    NULL,
                                                    false,
                                                    "gemini-2.5-pro",
                                                    NULL,
                                                    0,
                                                    0);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_parts_invalid_block_stops) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t blocks[2];
    memset(blocks, 0, sizeof(blocks));

    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = (char *)"Hello";
    blocks[1].type = IK_CONTENT_TOOL_CALL;
    blocks[1].data.tool_call.id = (char *)"call_123";
    blocks[1].data.tool_call.name = (char *)"get_weather";
    blocks[1].data.tool_call.arguments = (char *)"invalid json";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = blocks;
    message.content_count = 2;

    bool result = ik_google_serialize_message_parts(doc,
                                                    content_obj,
                                                    &message,
                                                    NULL,
                                                    false,
                                                    "gemini-2.5-pro",
                                                    NULL,
                                                    0,
                                                    0);
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

static Suite *request_helpers_parts_suite(void)
{
    Suite *s = suite_create("Google Request Helpers Parts");

    TCase *tc_parts = tcase_create("Message Parts Serialization");
    tcase_set_timeout(tc_parts, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_serialize_message_parts_basic);
    tcase_add_test(tc_parts, test_serialize_message_parts_with_thought_signature);
    tcase_add_test(tc_parts, test_serialize_parts_thought_not_first);
    tcase_add_test(tc_parts, test_serialize_message_parts_with_tool_call);
    tcase_add_test(tc_parts, test_serialize_message_parts_with_tool_result);
    tcase_add_test(tc_parts, test_serialize_message_parts_invalid_block_stops);
    suite_add_tcase(s, tc_parts);

    return s;
}

int32_t main(void)
{
    Suite *s = request_helpers_parts_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/request_helpers_parts_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
