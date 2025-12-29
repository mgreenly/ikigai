/**
 * @file anthropic_request_test_3.c
 * @brief Unit tests for Anthropic request serialization - Part 3: Role and Thinking tests
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
 * Role Mapping Tests
 * ================================================================ */

START_TEST(test_role_user) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_USER;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");

    ck_assert_str_eq(yyjson_get_str(role), "user");

    yyjson_doc_free(doc);
}
END_TEST START_TEST(test_role_assistant)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_ASSISTANT;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");

    ck_assert_str_eq(yyjson_get_str(role), "assistant");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_role_tool_mapped_to_user)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_TOOL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");

    // IK_ROLE_TOOL maps to "user" in Anthropic
    ck_assert_str_eq(yyjson_get_str(role), "user");

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Thinking Configuration Tests
 * ================================================================ */

START_TEST(test_thinking_none)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->thinking.level = IK_THINKING_NONE;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    // Should not have thinking field
    ck_assert_ptr_null(thinking);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_thinking_low)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_LOW;
    req->max_output_tokens = 32768;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    ck_assert_ptr_nonnull(thinking);
    yyjson_val *type = yyjson_obj_get(thinking, "type");
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");

    ck_assert_str_eq(yyjson_get_str(type), "enabled");
    // min=1024, max=64000, range=62976, LOW = 1024 + 62976/3 = 22016
    ck_assert_int_eq(yyjson_get_int(budget), 22016);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_thinking_med)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_MED;
    req->max_output_tokens = 65536;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    ck_assert_ptr_nonnull(thinking);
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    // min=1024, max=64000, range=62976, MED = 1024 + 2*62976/3 = 43008
    ck_assert_int_eq(yyjson_get_int(budget), 43008);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_thinking_high)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_HIGH;
    req->max_output_tokens = 128000;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    ck_assert_ptr_nonnull(thinking);
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    ck_assert_int_eq(yyjson_get_int(budget), 64000);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_thinking_adjusts_max_tokens)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_LOW;
    req->max_output_tokens = 512; // Less than thinking budget
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");

    // Should be adjusted to budget + 4096 = 22016 + 4096 = 26112
    ck_assert_int_eq(yyjson_get_int(max_tokens), 22016 + 4096);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_thinking_unsupported_model)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "gpt-4");
    req->thinking.level = IK_THINKING_LOW;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    // Should skip thinking for non-Claude model
    ck_assert_ptr_null(thinking);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_3(void)
{
    Suite *s = suite_create("Anthropic Request - Part 3");

    TCase *tc_role = tcase_create("Role Mapping");
    tcase_set_timeout(tc_role, 30);
    tcase_add_unchecked_fixture(tc_role, setup, teardown);
    tcase_add_test(tc_role, test_role_user);
    tcase_add_test(tc_role, test_role_assistant);
    tcase_add_test(tc_role, test_role_tool_mapped_to_user);
    suite_add_tcase(s, tc_role);

    TCase *tc_thinking = tcase_create("Thinking Configuration");
    tcase_set_timeout(tc_thinking, 30);
    tcase_add_unchecked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_thinking_none);
    tcase_add_test(tc_thinking, test_thinking_low);
    tcase_add_test(tc_thinking, test_thinking_med);
    tcase_add_test(tc_thinking, test_thinking_high);
    tcase_add_test(tc_thinking, test_thinking_adjusts_max_tokens);
    tcase_add_test(tc_thinking, test_thinking_unsupported_model);
    suite_add_tcase(s, tc_thinking);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_3();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
