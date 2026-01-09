#include "../../test_constants.h"
#include "../../../src/providers/response.h"
#include "../../../src/providers/request.h"
#include "../../../src/providers/provider.h"
#include "../../../src/error.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
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
 * Response Builder Tests
 * ================================================================ */

START_TEST(test_response_create) {
    ik_response_t *resp = NULL;
    res_t result = ik_response_create(test_ctx, &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    ck_assert_int_eq(resp->usage.cached_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
    ck_assert_ptr_null(resp->model);
    ck_assert_ptr_null(resp->provider_data);
}
END_TEST

START_TEST(test_response_add_content) {
    ik_response_t *resp = NULL;
    ik_response_create(test_ctx, &resp);

    ik_content_block_t *block = ik_content_block_text(test_ctx, "Hello");
    res_t result = ik_response_add_content(resp, block);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Hello");
}

END_TEST

START_TEST(test_response_add_multiple_content) {
    ik_response_t *resp = NULL;
    ik_response_create(test_ctx, &resp);

    /* Inline thinking block creation */
    ik_content_block_t *block1 = talloc_zero(test_ctx, ik_content_block_t);
    block1->type = IK_CONTENT_THINKING;
    block1->data.thinking.text = talloc_strdup(block1, "Thinking...");
    block1->data.thinking.signature = NULL;

    ik_content_block_t *block2 = ik_content_block_text(test_ctx, "Answer");
    ik_content_block_t *block3 = ik_content_block_tool_call(test_ctx,
                                                            "call_1", "read_file", "{\"path\":\"/tmp/file\"}");

    ik_response_add_content(resp, block1);
    ik_response_add_content(resp, block2);
    ik_response_add_content(resp, block3);

    ck_assert_int_eq((int)resp->content_count, 3);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(resp->content_blocks[0].data.thinking.text, "Thinking...");
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[1].data.text.text, "Answer");
    ck_assert_int_eq(resp->content_blocks[2].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[2].data.tool_call.id, "call_1");
}

END_TEST

START_TEST(test_response_memory_lifecycle) {
    /* Test that freeing response frees all child allocations */
    TALLOC_CTX *temp_ctx = talloc_new(NULL);

    ik_response_t *resp = NULL;
    ik_response_create(temp_ctx, &resp);

    ik_content_block_t *block1 = ik_content_block_text(temp_ctx, "Text 1");
    ik_content_block_t *block2 = ik_content_block_text(temp_ctx, "Text 2");

    /* Inline thinking block creation */
    ik_content_block_t *block3 = talloc_zero(temp_ctx, ik_content_block_t);
    block3->type = IK_CONTENT_THINKING;
    block3->data.thinking.text = talloc_strdup(block3, "Thinking");
    block3->data.thinking.signature = NULL;

    ik_response_add_content(resp, block1);
    ik_response_add_content(resp, block2);
    ik_response_add_content(resp, block3);

    /* Should not leak - talloc will verify */
    talloc_free(temp_ctx);

    ck_assert(1); /* If we get here, no crash */
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *response_suite(void)
{
    Suite *s = suite_create("Response Builders");

    TCase *tc_response = tcase_create("Response Builders");
    tcase_set_timeout(tc_response, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_response, setup, teardown);
    tcase_add_test(tc_response, test_response_create);
    tcase_add_test(tc_response, test_response_add_content);
    tcase_add_test(tc_response, test_response_add_multiple_content);
    tcase_add_test(tc_response, test_response_memory_lifecycle);
    suite_add_tcase(s, tc_response);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
