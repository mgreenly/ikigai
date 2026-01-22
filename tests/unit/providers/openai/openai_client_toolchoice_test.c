#include "../../../test_constants.h"
/**
 * @file openai_client_toolchoice_test.c
 * @brief Tool choice and validation tests for OpenAI request serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "vendor/yyjson/yyjson.h"
#include "providers/openai/request.h"
#include "providers/provider.h"

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
 * Tool Choice Tests
 * ================================================================ */

START_TEST(test_build_request_with_tool_choice_none) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;
    req->tool_choice_mode = 1; // IK_TOOL_NONE

    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "none");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_build_request_with_tool_choice_required) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;
    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "required");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_build_request_with_tool_choice_auto) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;
    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_build_request_with_tool_choice_unknown_defaults_to_auto) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;
    req->tool_choice_mode = 99; // Unknown value

    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(tool_choice);
    ck_assert_str_eq(yyjson_get_str(tool_choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Validation Tests
 * ================================================================ */

START_TEST(test_build_request_with_invalid_tool_parameters) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;

    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "invalid json{");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(is_err(&r));
}

END_TEST

START_TEST(test_build_request_with_null_model) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = NULL;
    req->max_output_tokens = 1024;

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(is_err(&r));
}

END_TEST

START_TEST(test_verify_correct_headers) {
    const char *api_key = "sk-test-key-12345";
    char **headers = NULL;

    res_t r = ik_openai_build_headers(test_ctx, api_key, &headers);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(headers);

    bool found_auth = false;
    bool found_content_type = false;
    for (int i = 0; headers[i] != NULL; i++) {
        if (strstr(headers[i], "Authorization: Bearer") != NULL) {
            found_auth = true;
            ck_assert(strstr(headers[i], api_key) != NULL);
        }
        if (strstr(headers[i], "Content-Type: application/json") != NULL) {
            found_content_type = true;
        }
    }

    ck_assert(found_auth);
    ck_assert(found_content_type);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_client_toolchoice_suite(void)
{
    Suite *s = suite_create("OpenAI Client Tool Choice");

    TCase *tc_toolchoice = tcase_create("Tool Choice");
    tcase_set_timeout(tc_toolchoice, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_toolchoice, setup, teardown);
    tcase_add_test(tc_toolchoice, test_build_request_with_tool_choice_none);
    tcase_add_test(tc_toolchoice, test_build_request_with_tool_choice_required);
    tcase_add_test(tc_toolchoice, test_build_request_with_tool_choice_auto);
    tcase_add_test(tc_toolchoice, test_build_request_with_tool_choice_unknown_defaults_to_auto);
    suite_add_tcase(s, tc_toolchoice);

    TCase *tc_validation = tcase_create("Validation");
    tcase_set_timeout(tc_validation, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_validation, setup, teardown);
    tcase_add_test(tc_validation, test_build_request_with_invalid_tool_parameters);
    tcase_add_test(tc_validation, test_build_request_with_null_model);
    tcase_add_test(tc_validation, test_verify_correct_headers);
    suite_add_tcase(s, tc_validation);

    return s;
}

int main(void)
{
    Suite *s = openai_client_toolchoice_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/openai_client_toolchoice_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
