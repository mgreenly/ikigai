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

START_TEST(test_fallback_to_system_prompt_when_no_blocks) {
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

START_TEST(test_single_system_block_not_cacheable) {
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

START_TEST(test_single_system_block_cacheable) {
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
 * Multiple blocks same category: consolidated into one block
 * ================================================================ */

START_TEST(test_multiple_system_blocks) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 3;
    req->system_blocks = talloc_array(req, ik_system_block_t, 3);
    /* All BASE_PROMPT type, two cacheable — consolidates into 1 block */
    req->system_blocks[0].text = talloc_strdup(req, "Block one.");
    req->system_blocks[0].cacheable = true;
    req->system_blocks[0].type = IK_SYSTEM_BLOCK_BASE_PROMPT;
    req->system_blocks[1].text = talloc_strdup(req, "Block two.");
    req->system_blocks[1].cacheable = false;
    req->system_blocks[1].type = IK_SYSTEM_BLOCK_BASE_PROMPT;
    req->system_blocks[2].text = talloc_strdup(req, "Block three.");
    req->system_blocks[2].cacheable = true;
    req->system_blocks[2].type = IK_SYSTEM_BLOCK_BASE_PROMPT;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));
    /* Three blocks of the same category → 1 consolidated block */
    ck_assert_uint_eq(yyjson_arr_size(system), 1);

    yyjson_val *b0 = yyjson_arr_get(system, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b0, "text")),
                     "Block one.\n\nBlock two.\n\nBlock three.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b0, "cache_control"));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * All categories present: at most 3 cacheable + 1 uncacheable
 * ================================================================ */

START_TEST(test_all_categories_consolidated) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 6;
    req->system_blocks = talloc_array(req, ik_system_block_t, 6);

    req->system_blocks[0].text = talloc_strdup(req, "Base prompt.");
    req->system_blocks[0].cacheable = true;
    req->system_blocks[0].type = IK_SYSTEM_BLOCK_BASE_PROMPT;

    req->system_blocks[1].text = talloc_strdup(req, "Pinned doc.");
    req->system_blocks[1].cacheable = true;
    req->system_blocks[1].type = IK_SYSTEM_BLOCK_PINNED_DOC;

    req->system_blocks[2].text = talloc_strdup(req, "Skill A.");
    req->system_blocks[2].cacheable = true;
    req->system_blocks[2].type = IK_SYSTEM_BLOCK_SKILL;

    req->system_blocks[3].text = talloc_strdup(req, "Catalog.");
    req->system_blocks[3].cacheable = true;
    req->system_blocks[3].type = IK_SYSTEM_BLOCK_SKILL_CATALOG;

    req->system_blocks[4].text = talloc_strdup(req, "Summary.");
    req->system_blocks[4].cacheable = true;
    req->system_blocks[4].type = IK_SYSTEM_BLOCK_SESSION_SUMMARY;

    req->system_blocks[5].text = talloc_strdup(req, "Recent.");
    req->system_blocks[5].cacheable = false;
    req->system_blocks[5].type = IK_SYSTEM_BLOCK_RECENT_SUMMARY;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));
    /* 6 input blocks → 4 output blocks (3 cacheable + 1 uncacheable) */
    ck_assert_uint_eq(yyjson_arr_size(system), 4);

    /* Block 0: BASE_PROMPT + PINNED_DOC, cacheable */
    yyjson_val *b0 = yyjson_arr_get(system, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b0, "text")),
                     "Base prompt.\n\nPinned doc.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b0, "cache_control"));

    /* Block 1: SKILL + SKILL_CATALOG, cacheable */
    yyjson_val *b1 = yyjson_arr_get(system, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b1, "text")),
                     "Skill A.\n\nCatalog.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b1, "cache_control"));

    /* Block 2: SESSION_SUMMARY, cacheable */
    yyjson_val *b2 = yyjson_arr_get(system, 2);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b2, "text")), "Summary.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b2, "cache_control"));

    /* Block 3: RECENT_SUMMARY, not cacheable */
    yyjson_val *b3 = yyjson_arr_get(system, 3);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b3, "text")), "Recent.");
    ck_assert_ptr_null(yyjson_obj_get(b3, "cache_control"));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Missing categories: empty groups are omitted
 * ================================================================ */

START_TEST(test_empty_categories_omitted) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 2;
    req->system_blocks = talloc_array(req, ik_system_block_t, 2);

    /* Only BASE_PROMPT and RECENT_SUMMARY — groups 1 and 2 are empty */
    req->system_blocks[0].text = talloc_strdup(req, "Base.");
    req->system_blocks[0].cacheable = true;
    req->system_blocks[0].type = IK_SYSTEM_BLOCK_BASE_PROMPT;

    req->system_blocks[1].text = talloc_strdup(req, "Recent.");
    req->system_blocks[1].cacheable = false;
    req->system_blocks[1].type = IK_SYSTEM_BLOCK_RECENT_SUMMARY;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));
    /* Groups 1 and 2 are empty → only 2 output blocks */
    ck_assert_uint_eq(yyjson_arr_size(system), 2);

    yyjson_val *b0 = yyjson_arr_get(system, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b0, "text")), "Base.");
    ck_assert_ptr_nonnull(yyjson_obj_get(b0, "cache_control"));

    yyjson_val *b1 = yyjson_arr_get(system, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b1, "text")), "Recent.");
    ck_assert_ptr_null(yyjson_obj_get(b1, "cache_control"));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * No cacheable blocks: emit individually as-is
 * ================================================================ */

START_TEST(test_no_cacheable_blocks_emitted_as_is) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 2;
    req->system_blocks = talloc_array(req, ik_system_block_t, 2);

    req->system_blocks[0].text = talloc_strdup(req, "Base.");
    req->system_blocks[0].cacheable = false;
    req->system_blocks[0].type = IK_SYSTEM_BLOCK_BASE_PROMPT;

    req->system_blocks[1].text = talloc_strdup(req, "Recent.");
    req->system_blocks[1].cacheable = false;
    req->system_blocks[1].type = IK_SYSTEM_BLOCK_RECENT_SUMMARY;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);
    ck_assert(yyjson_is_arr(system));
    /* No cacheable blocks → individual blocks, no consolidation */
    ck_assert_uint_eq(yyjson_arr_size(system), 2);

    yyjson_val *b0 = yyjson_arr_get(system, 0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b0, "text")), "Base.");
    ck_assert_ptr_null(yyjson_obj_get(b0, "cache_control"));

    yyjson_val *b1 = yyjson_arr_get(system, 1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(b1, "text")), "Recent.");
    ck_assert_ptr_null(yyjson_obj_get(b1, "cache_control"));

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * system_blocks take priority over system_prompt when both set
 * ================================================================ */

START_TEST(test_system_blocks_override_system_prompt) {
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
    tcase_add_test(tc, test_all_categories_consolidated);
    tcase_add_test(tc, test_empty_categories_omitted);
    tcase_add_test(tc, test_no_cacheable_blocks_emitted_as_is);
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
