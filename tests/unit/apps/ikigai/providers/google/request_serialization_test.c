#include "tests/test_constants.h"
/**
 * @file request_serialization_test.c
 * @brief Unit tests for Google request serialization
 */

// Disable cast-qual for test literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/request.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "vendor/yyjson/yyjson.h"
#include "shared/error.h"

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
 * Request Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_request_missing_model) {
    ik_request_t req = {0};
    req.model = NULL;
    req.message_count = 0;
    req.tool_count = 0;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
}

END_TEST

START_TEST(test_serialize_request_minimal) {
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tool_count = 0;
    req.system_prompt = NULL;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Should have contents array
    yyjson_val *contents = yyjson_obj_get(root, "contents");
    ck_assert_ptr_nonnull(contents);
    ck_assert(yyjson_is_arr(contents));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_system_prompt) {
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.system_prompt = (char *)"You are helpful";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_nonnull(sys);

    yyjson_val *parts = yyjson_obj_get(sys, "parts");
    ck_assert_ptr_nonnull(parts);
    ck_assert(yyjson_is_arr(parts));
    ck_assert_uint_eq(yyjson_arr_size(parts), 1);

    yyjson_val *part = yyjson_arr_get_first(parts);
    yyjson_val *text = yyjson_obj_get(part, "text");
    ck_assert_str_eq(yyjson_get_str(text), "You are helpful");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_empty_system_prompt) {
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.system_prompt = (char *)"";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify - should not have system instruction
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_null(sys);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_messages) {
    ik_message_t msg = {0};
    msg.role = IK_ROLE_USER;
    msg.content_count = 1;

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";
    msg.content_blocks = &block;

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.messages = &msg;
    req.message_count = 1;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *contents = yyjson_obj_get(root, "contents");
    ck_assert_ptr_nonnull(contents);
    ck_assert_uint_eq(yyjson_arr_size(contents), 1);

    yyjson_val *content = yyjson_arr_get_first(contents);
    yyjson_val *role = yyjson_obj_get(content, "role");
    ck_assert_str_eq(yyjson_get_str(role), "user");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_tools) {
    ik_tool_def_t tool = {0};
    tool.name = (char *)"test_tool";
    tool.description = (char *)"A test tool";
    tool.parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tools = &tool;
    req.tool_count = 1;
    req.tool_choice_mode = 0; // IK_TOOL_AUTO
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Check tools array
    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));

    // Check toolConfig
    yyjson_val *tool_config = yyjson_obj_get(root, "toolConfig");
    ck_assert_ptr_nonnull(tool_config);

    yyjson_val *func_config = yyjson_obj_get(tool_config, "functionCallingConfig");
    ck_assert_ptr_nonnull(func_config);

    yyjson_val *mode = yyjson_obj_get(func_config, "mode");
    ck_assert_str_eq(yyjson_get_str(mode), "AUTO");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_tool_choice_none) {
    ik_tool_def_t tool = {0};
    tool.name = (char *)"test_tool";
    tool.description = (char *)"A test tool";
    tool.parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tools = &tool;
    req.tool_count = 1;
    req.tool_choice_mode = 1; // IK_TOOL_NONE
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_config = yyjson_obj_get(root, "toolConfig");
    yyjson_val *func_config = yyjson_obj_get(tool_config, "functionCallingConfig");
    yyjson_val *mode = yyjson_obj_get(func_config, "mode");
    ck_assert_str_eq(yyjson_get_str(mode), "NONE");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_tool_choice_required) {
    ik_tool_def_t tool = {0};
    tool.name = (char *)"test_tool";
    tool.description = (char *)"A test tool";
    tool.parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tools = &tool;
    req.tool_count = 1;
    req.tool_choice_mode = 2; // IK_TOOL_REQUIRED
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_config = yyjson_obj_get(root, "toolConfig");
    yyjson_val *func_config = yyjson_obj_get(tool_config, "functionCallingConfig");
    yyjson_val *mode = yyjson_obj_get(func_config, "mode");
    ck_assert_str_eq(yyjson_get_str(mode), "ANY");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_tool_choice_unknown) {
    ik_tool_def_t tool = {0};
    tool.name = (char *)"test_tool";
    tool.description = (char *)"A test tool";
    tool.parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";

    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tools = &tool;
    req.tool_count = 1;
    req.tool_choice_mode = 999; // Invalid
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify - should default to AUTO
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_config = yyjson_obj_get(root, "toolConfig");
    yyjson_val *func_config = yyjson_obj_get(tool_config, "functionCallingConfig");
    yyjson_val *mode = yyjson_obj_get(func_config, "mode");
    ck_assert_str_eq(yyjson_get_str(mode), "AUTO");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_max_tokens) {
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 1000;
    req.thinking.level = IK_THINKING_MIN;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_nonnull(gen_config);

    yyjson_val *max_tokens = yyjson_obj_get(gen_config, "maxOutputTokens");
    ck_assert_int_eq(yyjson_get_int(max_tokens), 1000);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_with_thinking_gemini_25) {
    ik_request_t req = {0}; char *json = NULL;
    req.model = (char *)"gemini-2.5-pro"; req.thinking.level = IK_THINKING_LOW;
    res_t r25 = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r25));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tc = yyjson_obj_get(yyjson_obj_get(root, "generationConfig"), "thinkingConfig");
    ck_assert_ptr_nonnull(tc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(tc, "includeThoughts")));
    ck_assert_ptr_nonnull(yyjson_obj_get(tc, "thinkingBudget"));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_with_thinking_gemini_3) {
    ik_request_t req = {0}; char *json = NULL;
    req.model = (char *)"gemini-3-flash-preview"; req.thinking.level = IK_THINKING_MED;
    res_t r3 = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r3));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_nonnull(gen_config);
    yyjson_val *tc = yyjson_obj_get(gen_config, "thinkingConfig");
    ck_assert_ptr_nonnull(tc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(tc, "includeThoughts")));
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc, "thinkingLevel")), "medium");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_with_thinking_gemini_3_none) {
    ik_request_t req = {0}; char *json = NULL;
    req.model = (char *)"gemini-3-flash-preview"; req.thinking.level = IK_THINKING_MIN;
    res_t r3n = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r3n));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tc = yyjson_obj_get(yyjson_obj_get(root, "generationConfig"), "thinkingConfig");
    ck_assert_ptr_nonnull(tc);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc, "thinkingLevel")), "minimal");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_with_thinking_gemini_31_pro) {
    ik_request_t req = {0}; char *json = NULL;
    req.model = (char *)"gemini-3.1-pro-preview"; req.thinking.level = IK_THINKING_MED;
    res_t r31 = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r31));
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tc = yyjson_obj_get(yyjson_obj_get(root, "generationConfig"), "thinkingConfig");
    ck_assert_ptr_nonnull(tc);
    ck_assert(yyjson_get_bool(yyjson_obj_get(tc, "includeThoughts")));
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(tc, "thinkingLevel")), "medium");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_no_thinking_config_when_unsupported) {
    ik_request_t req = {0};
    req.model = (char *)"gemini-1.5-pro"; // 1.5 doesn't support thinking
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_LOW;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify - should not have generation config since 1.5 doesn't support thinking
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

static Suite *request_serialization_suite(void)
{
    Suite *s = suite_create("Google Request Serialization");

    TCase *tc_request = tcase_create("Request Serialization");
    tcase_set_timeout(tc_request, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_serialize_request_missing_model);
    tcase_add_test(tc_request, test_serialize_request_minimal);
    tcase_add_test(tc_request, test_serialize_request_with_system_prompt);
    tcase_add_test(tc_request, test_serialize_request_empty_system_prompt);
    tcase_add_test(tc_request, test_serialize_request_with_messages);
    tcase_add_test(tc_request, test_serialize_request_with_tools);
    tcase_add_test(tc_request, test_serialize_request_tool_choice_none);
    tcase_add_test(tc_request, test_serialize_request_tool_choice_required);
    tcase_add_test(tc_request, test_serialize_request_tool_choice_unknown);
    tcase_add_test(tc_request, test_serialize_request_with_max_tokens);
    tcase_add_test(tc_request, test_serialize_request_with_thinking_gemini_25);
    tcase_add_test(tc_request, test_serialize_request_with_thinking_gemini_3);
    tcase_add_test(tc_request, test_serialize_request_with_thinking_gemini_3_none);
    tcase_add_test(tc_request, test_serialize_request_with_thinking_gemini_31_pro);
    tcase_add_test(tc_request, test_serialize_request_no_thinking_config_when_unsupported);
    suite_add_tcase(s, tc_request);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_serialization_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/request_serialization_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
