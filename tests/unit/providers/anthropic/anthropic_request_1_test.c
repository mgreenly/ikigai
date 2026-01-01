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


START_TEST(test_serialize_request_stream) {
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

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_1(void)
{
    Suite *s = suite_create("Anthropic Request - Part 1");

    TCase *tc_basic = tcase_create("Basic Serialization");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_unchecked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_request_stream);
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
