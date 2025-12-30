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
 ik_content_block_t blocks[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"1st"},
  {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"2nd"}};
 ik_message_t msgs[2] = {{.role = IK_ROLE_USER, .content_count = 1, .content_blocks = &blocks[0]},
  {.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &blocks[1]}};
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .messages = msgs, .message_count = 2};
 char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json);
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 ck_assert_uint_eq(yyjson_arr_size(yyjson_obj_get(yyjson_doc_get_root(doc), "contents")), 2);
 yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_first_assistant_message)
{
 ik_content_block_t b[3] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"U"},
  {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"1"},
  {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"2"}};
 ik_message_t msgs[3] = {{.role = IK_ROLE_USER, .content_count = 1, .content_blocks = &b[0]},
  {.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &b[1]},
  {.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &b[2]}};
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .messages = msgs, .message_count = 3};
 char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json);
}
END_TEST

START_TEST(test_serialize_assistant_then_user)
{
 ik_content_block_t b[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"A"},
  {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"U"}};
 ik_message_t msgs[2] = {{.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &b[0]},
  {.role = IK_ROLE_USER, .content_count = 1, .content_blocks = &b[1]}};
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .messages = msgs, .message_count = 2};
 char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json);
}
END_TEST

START_TEST(test_serialize_multiple_tools)
{
 ik_tool_def_t tools[3] = {
  {.name = (char *)"t1", .description = (char *)"T1", .parameters = (char *)"{\"type\":\"object\"}"},
  {.name = (char *)"t2", .description = (char *)"T2", .parameters = (char *)"{\"type\":\"object\"}"},
  {.name = (char *)"t3", .description = (char *)"T3", .parameters = (char *)"{\"type\":\"object\"}"}};
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .tools = tools, .tool_count = 3};
 char *json = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json);
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0); ck_assert_ptr_nonnull(doc);
 yyjson_val *func_decls = yyjson_obj_get(yyjson_arr_get_first(
  yyjson_obj_get(yyjson_doc_get_root(doc), "tools")), "functionDeclarations");
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

START_TEST(test_build_url_and_headers)
{
 char *url = NULL; char **headers = NULL; res_t r;
 r = ik_google_build_url(test_ctx, "https://api.example.com", "gemini-2.0-flash", "k", false, &url);
 ck_assert(is_ok(&r)); ck_assert_str_eq(url, "https://api.example.com/models/gemini-2.0-flash:generateContent?key=k");
 r = ik_google_build_headers(test_ctx, false, &headers);
 ck_assert(is_ok(&r)); ck_assert_ptr_null(headers[1]);
 r = ik_google_build_url(test_ctx, "https://api.example.com", "gemini-2.0-flash", "k", true, &url);
 ck_assert(is_ok(&r)); ck_assert_str_eq(url, "https://api.example.com/models/gemini-2.0-flash:streamGenerateContent?key=k&alt=sse");
 r = ik_google_build_headers(test_ctx, true, &headers);
 ck_assert(is_ok(&r)); ck_assert_ptr_null(headers[2]);
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
 test_tool_choice_mode_helper(1, "NONE");   
 test_tool_choice_mode_helper(2, "ANY");    
 test_tool_choice_mode_helper(999, "AUTO"); 
}
END_TEST
START_TEST(test_thinking_model_variations)
{
 ik_request_t req = {0};
 char *json = NULL;

 req.model = (char *)"gemini-2.5-flash";
 req.thinking.level = IK_THINKING_HIGH;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *cfg = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
 ck_assert(yyjson_get_int(yyjson_obj_get(yyjson_obj_get(cfg, "thinkingConfig"), "thinkingBudget")) > 0);
 yyjson_doc_free(doc);

 req.model = (char *)"gemini-1.5-pro";
 r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 doc = yyjson_read(json, strlen(json), 0);
 ck_assert_ptr_null(yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig"));
 yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_generation_config_combinations)
{
 char *json = NULL; res_t r; yyjson_doc *doc; yyjson_val *gc;
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .max_output_tokens = 2048};
 r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
 doc = yyjson_read(json, strlen(json), 0);
 gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
 ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(gc, "maxOutputTokens")), 2048);
 ck_assert_ptr_null(yyjson_obj_get(gc, "thinkingConfig")); yyjson_doc_free(doc);
 req.model = (char *)"gemini-3.0-flash"; req.max_output_tokens = 1024;
 req.thinking.level = IK_THINKING_LOW;
 r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
 doc = yyjson_read(json, strlen(json), 0);
 gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
 ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(gc, "maxOutputTokens")), 1024);
 ck_assert_ptr_nonnull(yyjson_obj_get(gc, "thinkingConfig")); yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_system_instruction_cases)
{
 ik_request_t req = {0};
 req.model = (char *)"gemini-2.0-flash";
 req.message_count = 0;
 req.tool_count = 0;
 char *json = NULL;

 req.system_prompt = (char *)"You are a helpful assistant.";
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *sys_inst = yyjson_obj_get(yyjson_doc_get_root(doc), "systemInstruction");
 ck_assert_ptr_nonnull(sys_inst);
 yyjson_doc_free(doc);

 req.system_prompt = (char *)"";
 r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 doc = yyjson_read(json, strlen(json), 0);
 sys_inst = yyjson_obj_get(yyjson_doc_get_root(doc), "systemInstruction");
 ck_assert_ptr_null(sys_inst);
 yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_edge_cases)
{
 ik_request_t req = {0};
 char *json = NULL;

 req.model = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_err(&r));

 ik_message_t msgs[2] = {0};
 ik_content_block_t blocks[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"Hi"},
                                  {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"Bye"}};
 msgs[0].role = IK_ROLE_USER; msgs[0].content_blocks = &blocks[0]; msgs[0].content_count = 1;
 msgs[1].role = IK_ROLE_ASSISTANT; msgs[1].content_blocks = &blocks[1]; msgs[1].content_count = 1;
 msgs[1].provider_metadata = (char *)"{\"thought_signature\":\"sig\"}";
 req.model = (char *)"gemini-3.0-flash"; req.messages = msgs; req.message_count = 2;
 r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
}
END_TEST
START_TEST(test_content_blocks_and_errors)
{
 char *json = NULL; res_t res;
 ik_content_block_t thinking = {.type = IK_CONTENT_THINKING, .data.thinking.text = (char *)"T"};
 ik_message_t m1 = {.role = IK_ROLE_ASSISTANT, .content_blocks = &thinking, .content_count = 1};
 ik_request_t r1 = {.model = (char *)"gemini-2.0-flash", .messages = &m1, .message_count = 1};
 res = ik_google_serialize_request(test_ctx, &r1, &json); ck_assert(is_ok(&res));
 ik_content_block_t tool_res = {.type = IK_CONTENT_TOOL_RESULT, .data.tool_result = {.tool_call_id = (char *)"c", .content = (char *)"R", .is_error = false}};
 ik_message_t m2 = {.role = IK_ROLE_USER, .content_blocks = &tool_res, .content_count = 1};
 ik_request_t r2 = {.model = (char *)"gemini-2.0-flash", .messages = &m2, .message_count = 1};
 res = ik_google_serialize_request(test_ctx, &r2, &json); ck_assert(is_ok(&res));
 ik_content_block_t bad = {.type = IK_CONTENT_TOOL_CALL, .data.tool_call = {.id = (char *)"c", .name = (char *)"t", .arguments = (char *)"{bad}"}};
 ik_message_t m3 = {.role = IK_ROLE_ASSISTANT, .content_blocks = &bad, .content_count = 1};
 ik_request_t r3 = {.model = (char *)"gemini-2.0-flash", .messages = &m3, .message_count = 1};
 res = ik_google_serialize_request(test_ctx, &r3, &json); ck_assert(is_err(&res));
}
END_TEST
START_TEST(test_thinking_only_no_max_tokens)
{
 ik_request_t req = {.model = (char *)"gemini-2.5-flash", .thinking.level = IK_THINKING_HIGH};
 char *json = NULL;
 res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
 ck_assert_ptr_nonnull(gc);
 ck_assert_ptr_null(yyjson_obj_get(gc, "maxOutputTokens"));
 ck_assert_ptr_nonnull(yyjson_obj_get(gc, "thinkingConfig"));
 yyjson_doc_free(doc);
}
END_TEST
START_TEST(test_tool_additional_properties_removed)
{
 ik_tool_def_t tool = {.name = (char *)"t", .description = (char *)"T",
  .parameters = (char *)"{\"type\":\"object\",\"additionalProperties\":false}"};
 ik_request_t req = {.model = (char *)"gemini-2.0-flash", .tools = &tool, .tool_count = 1};
 char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
 ck_assert(is_ok(&r));
 yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
 yyjson_val *params = yyjson_obj_get(yyjson_arr_get_first(yyjson_obj_get(
  yyjson_arr_get_first(yyjson_obj_get(yyjson_doc_get_root(doc), "tools")),
  "functionDeclarations")), "parameters");
 ck_assert_ptr_null(yyjson_obj_get(params, "additionalProperties"));
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
 tcase_add_test(tc_thinking, test_thinking_model_variations);
 suite_add_tcase(s, tc_thinking);
 TCase *tc_misc = tcase_create("Miscellaneous Coverage");
 tcase_set_timeout(tc_misc, 30);
 tcase_add_checked_fixture(tc_misc, setup, teardown);
 tcase_add_test(tc_misc, test_build_url_and_headers);
 tcase_add_test(tc_misc, test_generation_config_combinations);
 tcase_add_test(tc_misc, test_system_instruction_cases);
 tcase_add_test(tc_misc, test_edge_cases);
 tcase_add_test(tc_misc, test_content_blocks_and_errors);
 tcase_add_test(tc_misc, test_thinking_only_no_max_tokens);
 tcase_add_test(tc_misc, test_tool_additional_properties_removed);
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
