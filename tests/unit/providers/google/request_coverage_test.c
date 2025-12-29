/**
 * @file request_coverage_test.c
 * @brief Coverage tests for uncovered branches in Google request serialization
 */

// Disable cast-qual for test literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/request.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "vendor/yyjson/yyjson.h"
#include "error.h"

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
 * Tests for serialize_contents with multiple messages
 * ================================================================ */

START_TEST(test_serialize_multiple_messages)
{
    // Create two messages to hit the loop continuation (line 86 branch true)
    ik_message_t msgs[2];

    // First message - user
    msgs[0].role = IK_ROLE_USER;
    msgs[0].content_count = 1;
    ik_content_block_t block1 = {0};
    block1.type = IK_CONTENT_TEXT;
    block1.data.text.text = (char *)"First message";
    msgs[0].content_blocks = &block1;

    // Second message - assistant
    msgs[1].role = IK_ROLE_ASSISTANT;
    msgs[1].content_count = 1;
    ik_content_block_t block2 = {0};
    block2.type = IK_CONTENT_TEXT;
    block2.data.text.text = (char *)"Second message";
    msgs[1].content_blocks = &block2;

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.messages = msgs;
    req.message_count = 2; // Multiple messages to test loop
    req.tool_count = 0;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Verify both messages are in the output
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *contents = yyjson_obj_get(root, "contents");
    ck_assert_ptr_nonnull(contents);
    ck_assert_uint_eq(yyjson_arr_size(contents), 2);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_first_assistant_message)
{
    // Test the is_first_assistant logic (line 98)
    ik_message_t msgs[3];

    // User message
    msgs[0].role = IK_ROLE_USER;
    msgs[0].content_count = 1;
    ik_content_block_t block1 = {0};
    block1.type = IK_CONTENT_TEXT;
    block1.data.text.text = (char *)"User msg";
    msgs[0].content_blocks = &block1;

    // First assistant message - should have is_first_assistant = true
    msgs[1].role = IK_ROLE_ASSISTANT;
    msgs[1].content_count = 1;
    ik_content_block_t block2 = {0};
    block2.type = IK_CONTENT_TEXT;
    block2.data.text.text = (char *)"First assistant";
    msgs[1].content_blocks = &block2;

    // Second assistant message - should have is_first_assistant = false
    msgs[2].role = IK_ROLE_ASSISTANT;
    msgs[2].content_count = 1;
    ik_content_block_t block3 = {0};
    block3.type = IK_CONTENT_TEXT;
    block3.data.text.text = (char *)"Second assistant";
    msgs[2].content_blocks = &block3;

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.messages = msgs;
    req.message_count = 3;
    req.tool_count = 0;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);
}

END_TEST

START_TEST(test_serialize_assistant_then_user)
{
    // Test seen_assistant flag behavior (line 99-100)
    ik_message_t msgs[2];

    // First message is assistant
    msgs[0].role = IK_ROLE_ASSISTANT;
    msgs[0].content_count = 1;
    ik_content_block_t block1 = {0};
    block1.type = IK_CONTENT_TEXT;
    block1.data.text.text = (char *)"Assistant first";
    msgs[0].content_blocks = &block1;

    // Second message is user
    msgs[1].role = IK_ROLE_USER;
    msgs[1].content_count = 1;
    ik_content_block_t block2 = {0};
    block2.type = IK_CONTENT_TEXT;
    block2.data.text.text = (char *)"User second";
    msgs[1].content_blocks = &block2;

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.messages = msgs;
    req.message_count = 2;
    req.tool_count = 0;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);
}

END_TEST

START_TEST(test_serialize_multiple_tools)
{
    // Test to hit the tool loop continuation (line 144)
    ik_tool_def_t tools[3];

    tools[0].name = (char *)"tool1";
    tools[0].description = (char *)"First tool";
    tools[0].parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    tools[1].name = (char *)"tool2";
    tools[1].description = (char *)"Second tool";
    tools[1].parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    tools[2].name = (char *)"tool3";
    tools[2].description = (char *)"Third tool";
    tools[2].parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tools = tools;
    req.tool_count = 3; // Multiple tools to test loop
    req.tool_choice_mode = 0;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Verify all tools are in the output
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools_val = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools_val);
    ck_assert(yyjson_is_arr(tools_val));

    yyjson_val *tool_obj = yyjson_arr_get_first(tools_val);
    yyjson_val *func_decls = yyjson_obj_get(tool_obj, "functionDeclarations");
    ck_assert_ptr_nonnull(func_decls);
    ck_assert_uint_eq(yyjson_arr_size(func_decls), 3);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_thinking_gemini_3_null_level)
{
    // Test Gemini 3 with thinking level that returns NULL string (line 295)
    ik_request_t req = {0};
    req.model = (char *)"gemini-3.0-flash";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_NONE; // This should result in NULL level string

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Should not have generation config since thinking is NONE
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_null(gen_config);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_thinking_gemini_3_with_level)
{
    // Test Gemini 3 with valid thinking level (lines 292-298)
    ik_request_t req = {0};
    req.model = (char *)"gemini-3.0-flash";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_HIGH;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_nonnull(gen_config);

    yyjson_val *thinking_config = yyjson_obj_get(gen_config, "thinkingConfig");
    ck_assert_ptr_nonnull(thinking_config);

    yyjson_val *level = yyjson_obj_get(thinking_config, "thinkingLevel");
    ck_assert_ptr_nonnull(level);
    ck_assert_str_eq(yyjson_get_str(level), "HIGH");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_thinking_gemini_25_negative_budget)
{
    // Test Gemini 2.5 with negative budget (line 287 branch)
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.5-flash"; // Model that might return negative budget
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_NONE; // Level that results in negative budget

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Should not have generation config
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_null(gen_config);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_coverage_suite(void)
{
    Suite *s = suite_create("Google Request Coverage");

    TCase *tc_contents = tcase_create("Contents Multiple Messages");
    tcase_add_checked_fixture(tc_contents, setup, teardown);
    tcase_add_test(tc_contents, test_serialize_multiple_messages);
    tcase_add_test(tc_contents, test_serialize_first_assistant_message);
    tcase_add_test(tc_contents, test_serialize_assistant_then_user);
    suite_add_tcase(s, tc_contents);

    TCase *tc_tools = tcase_create("Tools Multiple");
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_multiple_tools);
    suite_add_tcase(s, tc_tools);

    TCase *tc_thinking = tcase_create("Thinking Edge Cases");
    tcase_add_checked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_serialize_thinking_gemini_3_null_level);
    tcase_add_test(tc_thinking, test_serialize_thinking_gemini_3_with_level);
    tcase_add_test(tc_thinking, test_serialize_thinking_gemini_25_negative_budget);
    suite_add_tcase(s, tc_thinking);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
