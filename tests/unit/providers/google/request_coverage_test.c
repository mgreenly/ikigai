// Coverage tests for Google request serialization
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
static void setup(void) { test_ctx = talloc_new(NULL); }
static void teardown(void) { talloc_free(test_ctx); }

START_TEST(test_serialize_multiple_messages)
{
    ik_message_t msgs[2];
    msgs[0].role = IK_ROLE_USER;
    msgs[0].content_count = 1;
    ik_content_block_t block1 = {0};
    block1.type = IK_CONTENT_TEXT;
    block1.data.text.text = (char *)"First";
    msgs[0].content_blocks = &block1;
    msgs[1].role = IK_ROLE_ASSISTANT;
    msgs[1].content_count = 1;
    ik_content_block_t block2 = {0};
    block2.type = IK_CONTENT_TEXT;
    block2.data.text.text = (char *)"Second";
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
    ik_message_t msgs[3];
    msgs[0].role = IK_ROLE_USER;
    msgs[0].content_count = 1;
    ik_content_block_t block1 = {0};
    block1.type = IK_CONTENT_TEXT;
    block1.data.text.text = (char *)"User";
    msgs[0].content_blocks = &block1;
    msgs[1].role = IK_ROLE_ASSISTANT;
    msgs[1].content_count = 1;
    ik_content_block_t block2 = {0};
    block2.type = IK_CONTENT_TEXT;
    block2.data.text.text = (char *)"First";
    msgs[1].content_blocks = &block2;
    msgs[2].role = IK_ROLE_ASSISTANT;
    msgs[2].content_count = 1;
    ik_content_block_t block3 = {0};
    block3.type = IK_CONTENT_TEXT;
    block3.data.text.text = (char *)"Second";
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
    ik_message_t msgs[2];
    msgs[0].role = IK_ROLE_ASSISTANT;
    msgs[0].content_count = 1;
    ik_content_block_t block1 = {0};
    block1.type = IK_CONTENT_TEXT;
    block1.data.text.text = (char *)"Asst";
    msgs[0].content_blocks = &block1;
    msgs[1].role = IK_ROLE_USER;
    msgs[1].content_count = 1;
    ik_content_block_t block2 = {0};
    block2.type = IK_CONTENT_TEXT;
    block2.data.text.text = (char *)"User";
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
    ik_tool_def_t tools[3];
    tools[0].name = (char *)"t1";
    tools[0].description = (char *)"T1";
    tools[0].parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";
    tools[1].name = (char *)"t2";
    tools[1].description = (char *)"T2";
    tools[1].parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";
    tools[2].name = (char *)"t3";
    tools[2].description = (char *)"T3";
    tools[2].parameters = (char *)"{\"type\":\"object\",\"properties\":{}}";
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.0-flash";
    req.message_count = 0;
    req.tools = tools;
    req.tool_count = 3;
    req.tool_choice_mode = 0;
    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);
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
    ik_request_t req = {0};
    req.model = (char *)"gemini-3.0-flash";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_NONE;
    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_null(gen_config);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_thinking_gemini_3_with_level)
{
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
    ik_request_t req = {0};
    req.model = (char *)"gemini-2.5-flash";
    req.message_count = 0;
    req.tool_count = 0;
    req.max_output_tokens = 0;
    req.thinking.level = IK_THINKING_NONE;
    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
    ck_assert_ptr_null(gen_config);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_build_url_non_streaming)
{
 // Test ik_google_build_url with streaming=false
 char *url = NULL;
 res_t r = ik_google_build_url(test_ctx, "https://api.example.com",
           "gemini-2.0-flash", "test_key_123", false, &url);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(url);
 ck_assert_str_eq(url, "https://api.example.com/models/gemini-2.0-flash:generateContent?key=test_key_123");
}
END_TEST
START_TEST(test_build_url_streaming)
{
 // Test ik_google_build_url with streaming=true
 char *url = NULL;
 res_t r = ik_google_build_url(test_ctx, "https://api.example.com",
           "gemini-2.0-flash", "test_key_123", true, &url);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(url);
 ck_assert_str_eq(url, "https://api.example.com/models/gemini-2.0-flash:streamGenerateContent?key=test_key_123&alt=sse");
}
END_TEST
START_TEST(test_build_headers_non_streaming)
{
 // Test ik_google_build_headers with streaming=false
 char **headers = NULL;
 res_t r = ik_google_build_headers(test_ctx, false, &headers);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(headers);
 ck_assert_str_eq(headers[0], "Content-Type: application/json");
 ck_assert_ptr_null(headers[1]);
}
END_TEST
START_TEST(test_build_headers_streaming)
{
 // Test ik_google_build_headers with streaming=true
 char **headers = NULL;
 res_t r = ik_google_build_headers(test_ctx, true, &headers);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(headers);
 ck_assert_str_eq(headers[0], "Content-Type: application/json");
 ck_assert_str_eq(headers[1], "Accept: text/event-stream");
 ck_assert_ptr_null(headers[2]);
}
END_TEST
static void test_tool_choice_mode_helper(int mode, const char *expected) {
 ik_tool_def_t tool = {.name = (char *)"t", .description = (char *)"T",
  .parameters = (char *)"{\"type\":\"object\",\"properties\":{}}"};
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .tools = &tool, .tool_count = 1,
  .tool_choice_mode = mode};
 char *json = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *m = yyjson_obj_get(yyjson_obj_get(yyjson_obj_get(
  yyjson_doc_get_root(doc), "toolConfig"), "functionCallingConfig"), "mode");
 ck_assert_str_eq(yyjson_get_str(m), expected);
 yyjson_doc_free(doc);
}
START_TEST(test_tool_choice_modes)
{
 test_tool_choice_mode_helper(1, "NONE");    // IK_TOOL_NONE
 test_tool_choice_mode_helper(2, "ANY");     // IK_TOOL_REQUIRED
 test_tool_choice_mode_helper(999, "AUTO");  // default case
}
END_TEST
START_TEST(test_thinking_gemini_25_positive_budget)
{
 // Test Gemini 2.5 with positive thinking budget
 ik_request_t req = {0};
 req.model = (char *)"gemini-2.5-flash";
 req.message_count = 0;
 req.tool_count = 0;
 req.max_output_tokens = 0;
 req.thinking.level = IK_THINKING_HIGH; // Should result in positive budget
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
 yyjson_val *budget = yyjson_obj_get(thinking_config, "thinkingBudget");
 ck_assert_ptr_nonnull(budget);
 ck_assert(yyjson_get_int(budget) > 0);
 yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_max_output_tokens_only)
{
 // Test max_output_tokens without thinking
 ik_request_t req = {0};
 req.model = (char *)"gemini-2.0-flash";
 req.message_count = 0;
 req.tool_count = 0;
 req.max_output_tokens = 2048;
 req.thinking.level = IK_THINKING_NONE;
 char *json = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(json);
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *root = yyjson_doc_get_root(doc);
 yyjson_val *gen_config = yyjson_obj_get(root, "generationConfig");
 ck_assert_ptr_nonnull(gen_config);
 yyjson_val *max_tokens = yyjson_obj_get(gen_config, "maxOutputTokens");
 ck_assert_ptr_nonnull(max_tokens);
 ck_assert_int_eq(yyjson_get_int(max_tokens), 2048);
 // Should not have thinking config
 yyjson_val *thinking_config = yyjson_obj_get(gen_config, "thinkingConfig");
 ck_assert_ptr_null(thinking_config);
 yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_system_instruction_non_empty)
{
 // Test system instruction with non-empty prompt
 ik_request_t req = {0};
 req.model = (char *)"gemini-2.0-flash";
 req.system_prompt = (char *)"You are a helpful assistant.";
 req.message_count = 0;
 req.tool_count = 0;
 char *json = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(json);
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *root = yyjson_doc_get_root(doc);
 yyjson_val *sys_inst = yyjson_obj_get(root, "systemInstruction");
 ck_assert_ptr_nonnull(sys_inst);
 yyjson_val *parts = yyjson_obj_get(sys_inst, "parts");
 ck_assert_ptr_nonnull(parts);
 ck_assert_uint_eq(yyjson_arr_size(parts), 1);
 yyjson_val *part = yyjson_arr_get_first(parts);
 yyjson_val *text = yyjson_obj_get(part, "text");
 ck_assert_str_eq(yyjson_get_str(text), "You are a helpful assistant.");
 yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_system_instruction_empty_string)
{
 // Test system instruction with empty string (should be skipped like NULL)
 ik_request_t req = {0};
 req.model = (char *)"gemini-2.0-flash";
 req.system_prompt = (char *)""; // Empty string
 req.message_count = 0;
 req.tool_count = 0;
 char *json = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 ck_assert_ptr_nonnull(json);
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *root = yyjson_doc_get_root(doc);
 // Should not have system instruction for empty string
 yyjson_val *sys_inst = yyjson_obj_get(root, "systemInstruction");
 ck_assert_ptr_null(sys_inst);
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
 tcase_set_timeout(tc_contents, 30);
 tcase_add_checked_fixture(tc_contents, setup, teardown);
 tcase_add_test(tc_contents, test_serialize_multiple_messages);
 tcase_add_test(tc_contents, test_serialize_first_assistant_message);
 tcase_add_test(tc_contents, test_serialize_assistant_then_user);
 suite_add_tcase(s, tc_contents);
 TCase *tc_tools = tcase_create("Tools Multiple");
 tcase_set_timeout(tc_tools, 30);
 tcase_add_checked_fixture(tc_tools, setup, teardown);
 tcase_add_test(tc_tools, test_serialize_multiple_tools);
 tcase_add_test(tc_tools, test_tool_choice_modes);
 suite_add_tcase(s, tc_tools);
 TCase *tc_thinking = tcase_create("Thinking Edge Cases");
 tcase_set_timeout(tc_thinking, 30);
 tcase_add_checked_fixture(tc_thinking, setup, teardown);
 tcase_add_test(tc_thinking, test_serialize_thinking_gemini_3_null_level);
 tcase_add_test(tc_thinking, test_serialize_thinking_gemini_3_with_level);
 tcase_add_test(tc_thinking, test_serialize_thinking_gemini_25_negative_budget);
 tcase_add_test(tc_thinking, test_thinking_gemini_25_positive_budget);
 suite_add_tcase(s, tc_thinking);
 TCase *tc_misc = tcase_create("Miscellaneous Coverage");
 tcase_set_timeout(tc_misc, 30);
 tcase_add_checked_fixture(tc_misc, setup, teardown);
 tcase_add_test(tc_misc, test_build_url_non_streaming);
 tcase_add_test(tc_misc, test_build_url_streaming);
 tcase_add_test(tc_misc, test_build_headers_non_streaming);
 tcase_add_test(tc_misc, test_build_headers_streaming);
 tcase_add_test(tc_misc, test_max_output_tokens_only);
 tcase_add_test(tc_misc, test_system_instruction_non_empty);
 tcase_add_test(tc_misc, test_system_instruction_empty_string);
 suite_add_tcase(s, tc_misc);
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
