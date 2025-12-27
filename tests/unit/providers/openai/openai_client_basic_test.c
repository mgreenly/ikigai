/**
 * @file openai_client_basic_test.c
 * @brief Basic unit tests for OpenAI request serialization
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
 * Basic Request Serialization Tests
 * ================================================================ */

START_TEST(test_build_request_with_system_and_user_messages) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant.");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *model = yyjson_obj_get(root, "model");
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(yyjson_get_str(model), "gpt-4");

    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    size_t msg_count = yyjson_arr_size(messages);
    ck_assert_int_ge((int)msg_count, 2);

    yyjson_doc_free(doc);
}
END_TEST START_TEST(test_build_request_for_o1_model_with_reasoning_effort)
{
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "o1-preview");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_HIGH;

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Solve this problem.");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *reasoning_effort = yyjson_obj_get(root, "reasoning_effort");
    if (reasoning_effort != NULL) {
        ck_assert_str_eq(yyjson_get_str(reasoning_effort), "high");
    }

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_build_request_for_gpt5_model_without_reasoning_effort)
{
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-5-mini");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_HIGH;

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *reasoning_effort = yyjson_obj_get(root, "reasoning_effort");
    ck_assert_ptr_null(reasoning_effort);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_build_request_with_tool_definitions)
{
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;

    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather for a location");
    req->tools[0].parameters = talloc_strdup(req,
                                             "{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\"}}}");

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "What's the weather?");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert_int_eq((int)yyjson_arr_size(tools), 1);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_build_request_without_optional_fields)
{
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    ck_assert_ptr_nonnull(yyjson_obj_get(root, "model"));
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "messages"));

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_build_request_with_streaming_enabled)
{
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 1024;

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_openai_serialize_chat_request(test_ctx, req, true, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream));

    yyjson_val *stream_options = yyjson_obj_get(root, "stream_options");
    ck_assert_ptr_nonnull(stream_options);

    yyjson_val *include_usage = yyjson_obj_get(stream_options, "include_usage");
    ck_assert_ptr_nonnull(include_usage);
    ck_assert(yyjson_get_bool(include_usage));

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_verify_json_structure_matches_chat_completions_api)
{
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gpt-4");
    req->max_output_tokens = 2048;
    req->system_prompt = talloc_strdup(req, "You are helpful.");

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

    ck_assert_ptr_nonnull(yyjson_obj_get(root, "model"));
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "messages"));

    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");
    yyjson_val *max_completion_tokens = yyjson_obj_get(root, "max_completion_tokens");
    ck_assert(max_tokens != NULL || max_completion_tokens != NULL);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_client_basic_suite(void)
{
    Suite *s = suite_create("OpenAI Client Basic");

    TCase *tc_basic = tcase_create("Basic Serialization");
    tcase_add_unchecked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_build_request_with_system_and_user_messages);
    tcase_add_test(tc_basic, test_build_request_for_o1_model_with_reasoning_effort);
    tcase_add_test(tc_basic, test_build_request_for_gpt5_model_without_reasoning_effort);
    tcase_add_test(tc_basic, test_build_request_with_tool_definitions);
    tcase_add_test(tc_basic, test_build_request_without_optional_fields);
    tcase_add_test(tc_basic, test_build_request_with_streaming_enabled);
    tcase_add_test(tc_basic, test_verify_json_structure_matches_chat_completions_api);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    Suite *s = openai_client_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
