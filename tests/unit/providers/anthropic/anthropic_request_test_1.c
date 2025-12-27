/**
 * @file anthropic_request_test_1.c
 * @brief Unit tests for Anthropic request serialization - Part 1: Basic tests
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request.h"
#include "providers/provider.h"
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

/* ================================================================
 * Helper Functions
 * ================================================================ */

static ik_request_t *create_basic_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-3-5-sonnet-20241022");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_NONE;

    // Add one simple message
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

/* ================================================================
 * Basic Request Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_request_basic)
{
    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Check model
    yyjson_val *model = yyjson_obj_get(root, "model");
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(yyjson_get_str(model), "claude-3-5-sonnet-20241022");

    // Check max_tokens
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq(yyjson_get_int(max_tokens), 1024);

    // Check messages
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    // Check stream is not present for non-streaming request
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_null(stream);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_stream)
{
    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Check stream is true
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream) == true);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_null_model)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = NULL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_serialize_request_default_max_tokens)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->max_output_tokens = 0;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");

    // Should default to 4096
    ck_assert_int_eq(yyjson_get_int(max_tokens), 4096);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_negative_max_tokens)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->max_output_tokens = -1;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");

    // Should default to 4096
    ck_assert_int_eq(yyjson_get_int(max_tokens), 4096);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_with_system_prompt)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant.");
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");

    ck_assert_ptr_nonnull(system);
    ck_assert_str_eq(yyjson_get_str(system), "You are a helpful assistant.");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_without_system_prompt)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = NULL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");

    // System should not be present
    ck_assert_ptr_null(system);

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_1(void)
{
    Suite *s = suite_create("Anthropic Request - Part 1");

    TCase *tc_basic = tcase_create("Basic Serialization");
    tcase_add_unchecked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_request_basic);
    tcase_add_test(tc_basic, test_serialize_request_stream);
    tcase_add_test(tc_basic, test_serialize_request_null_model);
    tcase_add_test(tc_basic, test_serialize_request_default_max_tokens);
    tcase_add_test(tc_basic, test_serialize_request_negative_max_tokens);
    tcase_add_test(tc_basic, test_serialize_request_with_system_prompt);
    tcase_add_test(tc_basic, test_serialize_request_without_system_prompt);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_1();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
