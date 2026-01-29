#include "../../../test_constants.h"
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

START_TEST(test_generation_config_combinations) {
    char *json = NULL; res_t r; yyjson_doc *doc; yyjson_val *gc;
    ik_request_t req = {.model = (char *)"gemini-2.5-flash", .max_output_tokens = 2048};
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
    ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(gc, "maxOutputTokens")), 2048);
    ck_assert_ptr_null(yyjson_obj_get(gc, "thinkingConfig")); yyjson_doc_free(doc);
    req.model = (char *)"gemini-3-flash-preview"; req.max_output_tokens = 1024;
    req.thinking.level = IK_THINKING_LOW;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
    ck_assert_int_eq(yyjson_get_int(yyjson_obj_get(gc, "maxOutputTokens")), 1024);
    ck_assert_ptr_nonnull(yyjson_obj_get(gc, "thinkingConfig")); yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_system_instruction_cases) {
    ik_request_t req = {.model = (char *)"gemini-2.5-flash"}; char *json = NULL; res_t r;
    yyjson_doc *doc; yyjson_val *sys_inst;
    req.system_prompt = (char *)"You are a helpful assistant.";
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    sys_inst = yyjson_obj_get(yyjson_doc_get_root(doc), "systemInstruction");
    ck_assert_ptr_nonnull(sys_inst); yyjson_doc_free(doc);
    req.system_prompt = (char *)"";
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_null(yyjson_obj_get(yyjson_doc_get_root(doc), "systemInstruction"));
    yyjson_doc_free(doc);
    req.system_prompt = NULL;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_null(yyjson_obj_get(yyjson_doc_get_root(doc), "systemInstruction"));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_edge_cases) {
    ik_request_t req = {0}; char *json = NULL; res_t r;
    req.model = NULL;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_err(&r));
    ik_message_t msgs[2] = {0};
    ik_content_block_t blocks[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"Hi"},
                                    {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"Bye"}};
    msgs[0].role = IK_ROLE_USER; msgs[0].content_blocks = &blocks[0]; msgs[0].content_count = 1;
    msgs[1].role = IK_ROLE_ASSISTANT; msgs[1].content_blocks = &blocks[1]; msgs[1].content_count = 1;
    msgs[1].provider_metadata = (char *)"{\"thought_signature\":\"sig\"}";
    req.model = (char *)"gemini-3-flash-preview"; req.messages = msgs; req.message_count = 2;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_content_blocks_and_errors) {
    char *json = NULL; res_t res;
    ik_content_block_t thinking = {.type = IK_CONTENT_THINKING, .data.thinking.text = (char *)"T"};
    ik_message_t m1 = {.role = IK_ROLE_ASSISTANT, .content_blocks = &thinking, .content_count = 1};
    ik_request_t r1 = {.model = (char *)"gemini-2.5-flash", .messages = &m1, .message_count = 1};
    res = ik_google_serialize_request(test_ctx, &r1, &json); ck_assert(is_ok(&res));
    ik_content_block_t tr = {.type = IK_CONTENT_TOOL_RESULT,
                             .data.tool_result = {.tool_call_id = (char *)"c", .content = (char *)"R",
                                                  .is_error = false}};
    ik_message_t m2 = {.role = IK_ROLE_USER, .content_blocks = &tr, .content_count = 1};
    ik_request_t r2 = {.model = (char *)"gemini-2.5-flash", .messages = &m2, .message_count = 1};
    res = ik_google_serialize_request(test_ctx, &r2, &json); ck_assert(is_ok(&res));
    ik_content_block_t bad = {.type = IK_CONTENT_TOOL_CALL,
                              .data.tool_call = {.id = (char *)"c", .name = (char *)"t", .arguments = (char *)"{bad}"}};
    ik_message_t m3 = {.role = IK_ROLE_ASSISTANT, .content_blocks = &bad, .content_count = 1};
    ik_request_t r3 = {.model = (char *)"gemini-2.5-flash", .messages = &m3, .message_count = 1};
    res = ik_google_serialize_request(test_ctx, &r3, &json); ck_assert(is_err(&res));
}
END_TEST

START_TEST(test_thinking_only_no_max_tokens) {
    ik_request_t req = {.model = (char *)"gemini-2.5-flash", .thinking.level = IK_THINKING_HIGH};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r)); yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
    ck_assert_ptr_nonnull(gc); ck_assert_ptr_null(yyjson_obj_get(gc, "maxOutputTokens"));
    ck_assert_ptr_nonnull(yyjson_obj_get(gc, "thinkingConfig")); yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_additional_properties_removed) {
    ik_tool_def_t tool = {.name = (char *)"t", .description = (char *)"T",
                          .parameters = (char *)"{\"type\":\"object\",\"additionalProperties\":false}"};
    ik_request_t req = {.model = (char *)"gemini-2.5-flash", .tools = &tool, .tool_count = 1};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r)); yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *params = yyjson_obj_get(yyjson_arr_get_first(yyjson_obj_get(
                                                                 yyjson_arr_get_first(yyjson_obj_get(
                                                                                          yyjson_doc_get_root(doc),
                                                                                          "tools")),
                                                                 "functionDeclarations")), "parameters");
    ck_assert_ptr_null(yyjson_obj_get(params, "additionalProperties")); yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thought_signature_doc_cleanup) {
    ik_content_block_t b[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"Q"},
                               {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"A"}};
    ik_message_t msgs[2] = {{.role = IK_ROLE_USER, .content_count = 1, .content_blocks = &b[0]},
                            {.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &b[1]}};
    msgs[1].provider_metadata = (char *)"{\"thought_signature\":\"test_sig_123\"}";
    ik_request_t req = {.model = (char *)"gemini-3-flash-preview-exp", .messages = msgs, .message_count = 2};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json);
}
END_TEST

START_TEST(test_invalid_tool_parameters_json) {
    ik_tool_def_t tool = {.name = (char *)"bad", .description = (char *)"T",
                          .parameters = (char *)"{bad}"};
    ik_request_t req = {.model = (char *)"gemini-2.5-flash", .tools = &tool, .tool_count = 1};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_err(&r)); ck_assert_int_eq(r.err->code, ERR_PARSE);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
static Suite *request_coverage_advanced_suite(void)
{
    Suite *s = suite_create("Google Request Coverage - Advanced");
    TCase *tc_misc = tcase_create("Advanced Coverage");
    tcase_set_timeout(tc_misc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_misc, setup, teardown);
    tcase_add_test(tc_misc, test_generation_config_combinations);
    tcase_add_test(tc_misc, test_system_instruction_cases);
    tcase_add_test(tc_misc, test_edge_cases);
    tcase_add_test(tc_misc, test_content_blocks_and_errors);
    tcase_add_test(tc_misc, test_thinking_only_no_max_tokens);
    tcase_add_test(tc_misc, test_tool_additional_properties_removed);
    tcase_add_test(tc_misc, test_thought_signature_doc_cleanup);
    tcase_add_test(tc_misc, test_invalid_tool_parameters_json);
    suite_add_tcase(s, tc_misc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_coverage_advanced_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/request_coverage_advanced_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
