#include "tests/test_constants.h"
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

START_TEST(test_serialize_multiple_messages) {
    ik_content_block_t blocks[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"1"},
                                    {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"2"}};
    ik_message_t msgs[2] = {{.role = IK_ROLE_USER, .content_count = 1, .content_blocks = &blocks[0]},
                            {.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &blocks[1]}};
    ik_request_t req = {.model = (char *)"gemini-2.0-flash", .messages = msgs, .message_count = 2};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json); yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_uint_eq(yyjson_arr_size(yyjson_obj_get(yyjson_doc_get_root(doc), "contents")), 2);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_first_assistant_message) {
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

START_TEST(test_serialize_assistant_then_user) {
    ik_content_block_t b[2] = {{.type = IK_CONTENT_TEXT, .data.text.text = (char *)"A"},
                               {.type = IK_CONTENT_TEXT, .data.text.text = (char *)"U"}};
    ik_message_t msgs[2] = {{.role = IK_ROLE_ASSISTANT, .content_count = 1, .content_blocks = &b[0]},
                            {.role = IK_ROLE_USER, .content_count = 1, .content_blocks = &b[1]}};
    ik_request_t req = {.model = (char *)"gemini-2.0-flash", .messages = msgs, .message_count = 2};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r));
}
END_TEST

START_TEST(test_serialize_multiple_tools) {
    ik_tool_def_t tools[3] = {
        {.name = (char *)"t1", .description = (char *)"T1", .parameters = (char *)"{\"type\":\"object\"}"},
        {.name = (char *)"t2", .description = (char *)"T2", .parameters = (char *)"{\"type\":\"object\"}"},
        {.name = (char *)"t3", .description = (char *)"T3", .parameters = (char *)"{\"type\":\"object\"}"}
    };
    ik_request_t req = {.model = (char *)"gemini-2.0-flash", .tools = tools, .tool_count = 3};
    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(json);
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0); ck_assert_ptr_nonnull(doc);
    yyjson_val *func_decls = yyjson_obj_get(yyjson_arr_get_first(
                                                yyjson_obj_get(yyjson_doc_get_root(doc), "tools")),
                                            "functionDeclarations");
    ck_assert_uint_eq(yyjson_arr_size(func_decls), 3);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_gemini_versions) {
    ik_request_t req = {0}; char *json = NULL; res_t r; yyjson_doc *doc; yyjson_val *gc;
    // Gemini 3 with NONE level - always emits thinkingConfig (NONE -> "minimal" for flash)
    req.model = (char *)"gemini-3-flash-preview"; req.thinking.level = IK_THINKING_MIN;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
    ck_assert_ptr_nonnull(gc);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(yyjson_obj_get(gc, "thinkingConfig"),
                                                   "thinkingLevel")), "minimal");
    yyjson_doc_free(doc);
    // Gemini 3 with HIGH level -> "high" (lowercase)
    req.thinking.level = IK_THINKING_HIGH;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    gc = yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(yyjson_obj_get(gc, "thinkingConfig"),
                                                   "thinkingLevel")), "high");
    yyjson_doc_free(doc);
    // Gemini 2.5 with NONE level - no generation config
    req.model = (char *)"gemini-2.5-flash"; req.thinking.level = IK_THINKING_MIN;
    r = ik_google_serialize_request(test_ctx, &req, &json); ck_assert(is_ok(&r));
    doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_null(yyjson_obj_get(yyjson_doc_get_root(doc), "generationConfig"));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_build_url_and_headers) {
    char *url = NULL; char **headers = NULL; res_t r;
    r = ik_google_build_url(test_ctx, "https://a.com", "gemini-2.0-flash", "k", false, &url);
    ck_assert(is_ok(&r)); ck_assert_str_eq(url, "https://a.com/models/gemini-2.0-flash:generateContent?key=k");
    r = ik_google_build_headers(test_ctx, false, &headers);
    ck_assert(is_ok(&r)); ck_assert_ptr_null(headers[1]);
    r = ik_google_build_url(test_ctx, "https://a.com", "gemini-2.0-flash", "k", true, &url);
    ck_assert(is_ok(&r)); ck_assert_str_eq(url,
                                           "https://a.com/models/gemini-2.0-flash:streamGenerateContent?key=k&alt=sse");
    r = ik_google_build_headers(test_ctx, true, &headers);
    ck_assert(is_ok(&r)); ck_assert_ptr_null(headers[2]);
}
END_TEST

static void test_tool_choice_mode_helper(int mode, const char *expected)
{
    ik_tool_def_t tool = {.name = (char *)"t", .description = (char *)"T",
                          .parameters = (char *)"{\"type\":\"object\"}"};
    ik_request_t req = {.model = (char *)"gemini-2.0-flash", .tools = &tool, .tool_count = 1,
                        .tool_choice_mode = mode};
    char *json = NULL; res_t r = ik_google_serialize_request(test_ctx, &req, &json);
    ck_assert(is_ok(&r)); yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *m = yyjson_obj_get(yyjson_obj_get(yyjson_obj_get(
                                                      yyjson_doc_get_root(doc), "toolConfig"), "functionCallingConfig"),
                                   "mode");
    ck_assert_str_eq(yyjson_get_str(m), expected); yyjson_doc_free(doc);
}

START_TEST(test_tool_choice_modes) {
    test_tool_choice_mode_helper(0, "AUTO");
    test_tool_choice_mode_helper(1, "NONE");
    test_tool_choice_mode_helper(2, "ANY");
    test_tool_choice_mode_helper(999, "AUTO");
}
END_TEST

START_TEST(test_thinking_model_variations) {
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

/* ================================================================
 * Test Suite Setup
 * ================================================================ */
static Suite *request_coverage_basic_suite(void)
{
    Suite *s = suite_create("Google Request Coverage - Basic");
    TCase *tc_contents = tcase_create("Contents Multiple Messages");
    tcase_set_timeout(tc_contents, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_contents, setup, teardown);
    tcase_add_test(tc_contents, test_serialize_multiple_messages);
    tcase_add_test(tc_contents, test_serialize_first_assistant_message);
    tcase_add_test(tc_contents, test_serialize_assistant_then_user);
    suite_add_tcase(s, tc_contents);
    TCase *tc_tools = tcase_create("Tools Multiple");
    tcase_set_timeout(tc_tools, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_multiple_tools);
    tcase_add_test(tc_tools, test_tool_choice_modes);
    suite_add_tcase(s, tc_tools);
    TCase *tc_thinking = tcase_create("Thinking Edge Cases");
    tcase_set_timeout(tc_thinking, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_thinking_gemini_versions);
    tcase_add_test(tc_thinking, test_thinking_model_variations);
    suite_add_tcase(s, tc_thinking);
    TCase *tc_misc = tcase_create("URL and Headers");
    tcase_set_timeout(tc_misc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_misc, setup, teardown);
    tcase_add_test(tc_misc, test_build_url_and_headers);
    suite_add_tcase(s, tc_misc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_coverage_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/request_coverage_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
