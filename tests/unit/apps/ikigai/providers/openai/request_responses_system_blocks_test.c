#include "tests/test_constants.h"
/**
 * @file request_responses_system_blocks_test.c
 * @brief Unit tests for OpenAI Responses API system block serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/request.h"
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
    req->model = talloc_strdup(req, "gpt-4o");
    req->max_output_tokens = 1024;

    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

/* Count items with a specific role in the input array */
static size_t count_input_items_with_role(yyjson_val *input, const char *role)
{
    size_t count = 0;
    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(input, idx, max, item) {
        yyjson_val *r = yyjson_obj_get(item, "role");
        if (r && yyjson_is_str(r) && strcmp(yyjson_get_str(r), role) == 0) {
            count++;
        }
    }
    return count;
}

/* Get Nth input item with a given role */
static yyjson_val *get_input_item_with_role(yyjson_val *input, const char *role, size_t n)
{
    size_t found = 0;
    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(input, idx, max, item) {
        yyjson_val *r = yyjson_obj_get(item, "role");
        if (r && yyjson_is_str(r) && strcmp(yyjson_get_str(r), role) == 0) {
            if (found == n) return item;
            found++;
        }
    }
    return NULL;
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
    res_t r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    /* instructions field set to legacy system_prompt */
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "Legacy prompt");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Single system block: block[0] goes in instructions
 * ================================================================ */

START_TEST(test_single_system_block) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "You are helpful.");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Block[0] in instructions */
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "You are helpful.");

    /* No developer messages in input (only one block) */
    yyjson_val *input = yyjson_obj_get(root, "input");
    if (input && yyjson_is_arr(input)) {
        ck_assert_uint_eq(count_input_items_with_role(input, "developer"), 0);
    }

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Multiple blocks: block[0] in instructions, rest as developer messages
 * ================================================================ */

START_TEST(test_multiple_system_blocks) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_block_count = 3;
    req->system_blocks = talloc_array(req, ik_system_block_t, 3);
    req->system_blocks[0].text = talloc_strdup(req, "Block zero.");
    req->system_blocks[0].cacheable = true;
    req->system_blocks[1].text = talloc_strdup(req, "Block one.");
    req->system_blocks[1].cacheable = false;
    req->system_blocks[2].text = talloc_strdup(req, "Block two.");
    req->system_blocks[2].cacheable = true;

    /* Use array input (multiple messages) so developer messages are in input */
    req->message_count = 2;
    req->messages = talloc_array(req, ik_message_t, 2);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");
    req->messages[1].role = IK_ROLE_ASSISTANT;
    req->messages[1].content_count = 1;
    req->messages[1].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[1].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[1].content_blocks[0].data.text.text = talloc_strdup(req, "Hi there");

    char *json = NULL;
    res_t r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Block[0] in instructions */
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "Block zero.");

    /* Blocks[1] and [2] become developer messages in input */
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_arr(input));

    ck_assert_uint_eq(count_input_items_with_role(input, "developer"), 2);

    yyjson_val *dev0 = get_input_item_with_role(input, "developer", 0);
    ck_assert_ptr_nonnull(dev0);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(dev0, "content")), "Block one.");

    yyjson_val *dev1 = get_input_item_with_role(input, "developer", 1);
    ck_assert_ptr_nonnull(dev1);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(dev1, "content")), "Block two.");

    /* Developer messages come before user/assistant messages */
    yyjson_val *first = yyjson_arr_get_first(input);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(first, "role")), "developer");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * system_blocks override system_prompt when both set
 * ================================================================ */

START_TEST(test_system_blocks_override_system_prompt) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "Legacy prompt");
    req->system_block_count = 1;
    req->system_blocks = talloc_array(req, ik_system_block_t, 1);
    req->system_blocks[0].text = talloc_strdup(req, "New block.");
    req->system_blocks[0].cacheable = false;

    char *json = NULL;
    res_t r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    /* instructions uses block text, not legacy prompt */
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "New block.");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * No system content: no instructions field
 * ================================================================ */

START_TEST(test_no_system_content) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = NULL;
    req->system_block_count = 0;
    req->system_blocks = NULL;

    char *json = NULL;
    res_t r = ik_openai_serialize_responses_request(test_ctx, req, false, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_responses_system_blocks_suite(void)
{
    Suite *s = suite_create("OpenAI Responses System Blocks");

    TCase *tc = tcase_create("SystemBlocks");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fallback_to_system_prompt_when_no_blocks);
    tcase_add_test(tc, test_single_system_block);
    tcase_add_test(tc, test_multiple_system_blocks);
    tcase_add_test(tc, test_system_blocks_override_system_prompt);
    tcase_add_test(tc, test_no_system_content);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_responses_system_blocks_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/request_responses_system_blocks_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
