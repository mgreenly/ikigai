#include "tests/test_constants.h"
/**
 * @file anthropic_request_system_blocks_test.c
 * @brief Unit tests for Anthropic system block serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_types.h"
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
    req->thinking.level = IK_THINKING_MIN;

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
 * Backward compatibility: system_prompt used when no blocks
 * ================================================================ */

START_TEST(test_fallback_to_system_prompt_when_no_blocks)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Legacy prompt");
    req->system_block_count = 0;
    req->system_blocks = NULL;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_str(system));
    ck_assert_str_eq(yyjson_get_str(system), "Legacy prompt");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Single system block without cache_control
 * ================================================================ */

START_TEST(test_single_system_block_not_cacheable)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "You are helpful.");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));
    ck_assert_uint_eq(yyjson_arr_size(system), 1);

    yyjson_val *block = yyjson_arr_get_first(system);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(block, "type")), "text");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(block, "text")), "You are helpful.");
    ck_assert_ptr_null(yyjson_obj_get(block, "cache_control"));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Single system block with cache_control
 * ================================================================ */

START_TEST(test_single_system_block_cacheable)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "Cache me.");
    req->system_blocks[0].cacheable = true;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));

    yyjson_val *block = yyjson_arr_get_first(system);
    yyjson_val *cc = yyjson_obj_get(block, "cache_control");
    ck_assert_ptr_nonnull(cc);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(cc, "type")), "ephemeral");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Multiple system blocks: order preserved, cache_control correct
 * ================================================================ */

START_TEST(test_multiple_system_blocks)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 3;
    req->system_blocks = talloc_array(req, ik_system_block_t, 3);
    req->system_blocks[0].text = talloc_strdup(req, "Block one.");
    req->system_blocks[0].cacheable = true;
    req->system_blocks[1].text = talloc_strdup(req, "Block two.");
    req->system_blocks[1].cacheable = false;
    req->system_blocks[2].text = talloc_strdup(req, "Block three.");
    req->system_blocks[2].cacheable = true;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));
    ck_assert_uint_eq(yyjson_arr_size(system), 3);

    yyjson_val *b0 = yyjson_arr_get(system, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b0, "text")), "Block one.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b0, "cache_control"));

    yyjson_val *b1 = yyjson_arr_get(system, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b1, "text")), "Block two.");
    ck_assert_ptr_null(yyjson_obj_get(b1, "cache_control"));

    yyjson_val *b2 = yyjson_arr_get(system, 2);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b2, "text")), "Block three.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b2, "cache_control"));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * system_blocks take priority over system_prompt when both set
 * ================================================================ */

START_TEST(test_system_blocks_override_system_prompt)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Legacy prompt");
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "New block.");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_system_blocks_suite(void)
{
    Suite *s = suite_create("Anthropic System Blocks");

    TCase *tc = tcase_create("SystemBlocks");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fallback_to_system_prompt_when_no_blocks);
    tcase_add_test(tc, test_single_system_block_not_cacheable);
    tcase_add_test(tc, test_single_system_block_cacheable);
    tcase_add_test(tc, test_multiple_system_blocks);
    tcase_add_test(tc, test_system_blocks_override_system_prompt);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = anthropic_system_blocks_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_request_system_blocks_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
