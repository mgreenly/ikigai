#include "tests/test_constants.h"
/**
 * @file google_request_system_blocks_test.c
 * @brief Unit tests for Google system block serialization
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/request.h"
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
    req->model = talloc_strdup(req, "gemini-2.0-flash");
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

START_TEST(test_fallback_to_system_prompt_when_no_blocks) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Legacy system prompt");
    req->system_block_count = 0;
    req->system_blocks = NULL;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_nonnull(sys);
    yyjson_val *parts = yyjson_obj_get(sys, "parts");
    ck_assert_ptr_nonnull(parts);
    ck_assert(yyjson_is_arr(parts));
    ck_assert_uint_eq(yyjson_arr_size(parts), 1);

    yyjson_val *part0 = yyjson_arr_get_first(parts);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(part0, "text")), "Legacy system prompt");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Single system block becomes a single parts entry
 * ================================================================ */

START_TEST(test_single_system_block) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "Single block text");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_nonnull(sys);
    yyjson_val *parts = yyjson_obj_get(sys, "parts");
    ck_assert(yyjson_is_arr(parts));
    ck_assert_uint_eq(yyjson_arr_size(parts), 1);

    yyjson_val *part0 = yyjson_arr_get_first(parts);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(part0, "text")), "Single block text");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Multiple system blocks become multiple parts entries
 * ================================================================ */

START_TEST(test_multiple_system_blocks_order) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 3;
    req->system_blocks = talloc_array(req, ik_system_block_t, 3);
    req->system_blocks[0].text = talloc_strdup(req, "Block one");
    req->system_blocks[0].cacheable = false;
    req->system_blocks[1].text = talloc_strdup(req, "Block two");
    req->system_blocks[1].cacheable = true;
    req->system_blocks[2].text = talloc_strdup(req, "Block three");
    req->system_blocks[2].cacheable = false;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_nonnull(sys);
    yyjson_val *parts = yyjson_obj_get(sys, "parts");
    ck_assert(yyjson_is_arr(parts));
    ck_assert_uint_eq(yyjson_arr_size(parts), 3);

    yyjson_arr_iter iter;
    yyjson_arr_iter_init(parts, &iter);

    yyjson_val *p0 = yyjson_arr_iter_next(&iter);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(p0, "text")), "Block one");

    yyjson_val *p1 = yyjson_arr_iter_next(&iter);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(p1, "text")), "Block two");

    yyjson_val *p2 = yyjson_arr_iter_next(&iter);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(p2, "text")), "Block three");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Blocks override system_prompt
 * ================================================================ */

START_TEST(test_blocks_override_system_prompt) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Should not appear");
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "Block wins");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_nonnull(sys);
    yyjson_val *parts = yyjson_obj_get(sys, "parts");
    ck_assert_uint_eq(yyjson_arr_size(parts), 1);

    yyjson_val *p0 = yyjson_arr_get_first(parts);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(p0, "text")), "Block wins");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * No system content: no systemInstruction field
 * ================================================================ */

START_TEST(test_no_system_content) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = NULL;
    req->system_block_count = 0;
    req->system_blocks = NULL;

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *sys = yyjson_obj_get(root, "systemInstruction");
    ck_assert_ptr_null(sys);

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *google_request_system_blocks_suite(void)
{
    Suite *s = suite_create("Google request system blocks");

    TCase *tc = tcase_create("system_blocks");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fallback_to_system_prompt_when_no_blocks);
    tcase_add_test(tc, test_single_system_block);
    tcase_add_test(tc, test_multiple_system_blocks_order);
    tcase_add_test(tc, test_blocks_override_system_prompt);
    tcase_add_test(tc, test_no_system_content);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = google_request_system_blocks_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_request_system_blocks_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}
