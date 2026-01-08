#include "../../test_constants.h"
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
 * Content Block Tests
 * ================================================================ */

START_TEST(test_content_block_text) {
    ik_content_block_t *block = ik_content_block_text(test_ctx, "Hello world");

    ck_assert_ptr_nonnull(block);
    ck_assert_int_eq(block->type, IK_CONTENT_TEXT);
    ck_assert_str_eq(block->data.text.text, "Hello world");
}
END_TEST

START_TEST(test_content_block_tool_call) {
    ik_content_block_t *block = ik_content_block_tool_call(test_ctx,
                                                           "call_123", "read_file", "{\"path\":\"/etc/hosts\"}");

    ck_assert_ptr_nonnull(block);
    ck_assert_int_eq(block->type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(block->data.tool_call.id, "call_123");
    ck_assert_str_eq(block->data.tool_call.name, "read_file");
    ck_assert_str_eq(block->data.tool_call.arguments, "{\"path\":\"/etc/hosts\"}");
}

END_TEST

START_TEST(test_content_block_tool_result) {
    ik_content_block_t *block = ik_content_block_tool_result(test_ctx,
                                                             "call_123", "File contents here", false);

    ck_assert_ptr_nonnull(block);
    ck_assert_int_eq(block->type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(block->data.tool_result.tool_call_id, "call_123");
    ck_assert_str_eq(block->data.tool_result.content, "File contents here");
    ck_assert(!block->data.tool_result.is_error);
}

END_TEST

START_TEST(test_content_block_tool_result_error) {
    ik_content_block_t *block = ik_content_block_tool_result(test_ctx,
                                                             "call_456", "File not found", true);

    ck_assert_ptr_nonnull(block);
    ck_assert_int_eq(block->type, IK_CONTENT_TOOL_RESULT);
    ck_assert_str_eq(block->data.tool_result.tool_call_id, "call_456");
    ck_assert_str_eq(block->data.tool_result.content, "File not found");
    ck_assert(block->data.tool_result.is_error);
}

END_TEST

/* ================================================================
 * Request Builder Tests
 * ================================================================ */

START_TEST(test_request_create) {
    ik_request_t *req = NULL;
    res_t result = ik_request_create(test_ctx, "claude-sonnet-4-5", &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_str_eq(req->model, "claude-sonnet-4-5");
    ck_assert_ptr_null(req->system_prompt);
    ck_assert_ptr_null(req->messages);
    ck_assert_int_eq((int)req->message_count, 0);
    ck_assert_ptr_null(req->tools);
    ck_assert_int_eq((int)req->tool_count, 0);
    ck_assert_int_eq(req->max_output_tokens, -1);
    ck_assert_int_eq(req->thinking.level, IK_THINKING_NONE);
    ck_assert(!req->thinking.include_summary);
    // During coexistence phase, tool_choice_mode is int (would be IK_TOOL_AUTO = 0)
    ck_assert_int_eq(req->tool_choice_mode, 0);
    ck_assert_ptr_null(req->tool_choice_name);
}

END_TEST

START_TEST(test_request_set_system) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "gpt-5-mini", &req);

    res_t result = ik_request_set_system(req, "You are a helpful assistant.");
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req->system_prompt);
    ck_assert_str_eq(req->system_prompt, "You are a helpful assistant.");
}

END_TEST

START_TEST(test_request_set_system_replace) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "gpt-5-mini", &req);

    ik_request_set_system(req, "First prompt");
    ik_request_set_system(req, "Second prompt");

    ck_assert_str_eq(req->system_prompt, "Second prompt");
}

END_TEST

START_TEST(test_request_add_message) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "gemini-3.0-flash", &req);

    res_t result = ik_request_add_message(req, IK_ROLE_USER, "Hello!");
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].role, IK_ROLE_USER);
    ck_assert_int_eq((int)req->messages[0].content_count, 1);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.text.text, "Hello!");
}

END_TEST

START_TEST(test_request_add_multiple_messages) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "claude-sonnet-4-5", &req);

    ik_request_add_message(req, IK_ROLE_USER, "First message");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Second message");
    ik_request_add_message(req, IK_ROLE_USER, "Third message");

    ck_assert_int_eq((int)req->message_count, 3);
    ck_assert_int_eq(req->messages[0].role, IK_ROLE_USER);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.text.text, "First message");
    ck_assert_int_eq(req->messages[1].role, IK_ROLE_ASSISTANT);
    ck_assert_str_eq(req->messages[1].content_blocks[0].data.text.text, "Second message");
    ck_assert_int_eq(req->messages[2].role, IK_ROLE_USER);
    ck_assert_str_eq(req->messages[2].content_blocks[0].data.text.text, "Third message");
}

END_TEST

START_TEST(test_request_add_message_blocks) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "gpt-5", &req);

    /* Create multiple content blocks */
    ik_content_block_t *blocks = talloc_array(test_ctx, ik_content_block_t, 2);

    /* Inline thinking block creation (instead of calling ik_content_block_thinking) */
    ik_content_block_t *thinking_block = talloc_zero(test_ctx, ik_content_block_t);
    thinking_block->type = IK_CONTENT_THINKING;
    thinking_block->data.thinking.text = talloc_strdup(thinking_block, "Thinking...");
    thinking_block->data.thinking.signature = NULL;
    blocks[0] = *thinking_block;

    blocks[1] = *ik_content_block_text(test_ctx, "Answer");

    res_t result = ik_request_add_message_blocks(req, IK_ROLE_ASSISTANT, blocks, 2);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);
    ck_assert_int_eq(req->messages[0].role, IK_ROLE_ASSISTANT);
    ck_assert_int_eq((int)req->messages[0].content_count, 2);
    ck_assert_int_eq(req->messages[0].content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(req->messages[0].content_blocks[0].data.thinking.text, "Thinking...");
    ck_assert_int_eq(req->messages[0].content_blocks[1].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(req->messages[0].content_blocks[1].data.text.text, "Answer");
}

END_TEST

START_TEST(test_request_set_thinking) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "claude-sonnet-4-5", &req);

    ik_request_set_thinking(req, IK_THINKING_MED, true);

    ck_assert_int_eq(req->thinking.level, IK_THINKING_MED);
    ck_assert(req->thinking.include_summary);
}

END_TEST

START_TEST(test_request_add_tool) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "gpt-5-mini", &req);

    const char *params = "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}}}";
    res_t result = ik_request_add_tool(req, "read_file", "Read a file", params, false);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->tool_count, 1);
    ck_assert_str_eq(req->tools[0].name, "read_file");
    ck_assert_str_eq(req->tools[0].description, "Read a file");
    ck_assert_str_eq(req->tools[0].parameters, params);
    ck_assert(!req->tools[0].strict);
}

END_TEST

START_TEST(test_request_add_multiple_tools) {
    ik_request_t *req = NULL;
    ik_request_create(test_ctx, "gemini-3.0-pro", &req);

    const char *params1 = "{\"type\":\"object\"}";
    const char *params2 = "{\"type\":\"object\"}";

    ik_request_add_tool(req, "glob", "Find files", params1, false);
    ik_request_add_tool(req, "grep", "Search files", params2, true);

    ck_assert_int_eq((int)req->tool_count, 2);
    ck_assert_str_eq(req->tools[0].name, "glob");
    ck_assert(!req->tools[0].strict);
    ck_assert_str_eq(req->tools[1].name, "grep");
    ck_assert(req->tools[1].strict);
}

END_TEST

START_TEST(test_request_memory_lifecycle) {
    /* Test that freeing request frees all child allocations */
    TALLOC_CTX *temp_ctx = talloc_new(NULL);

    ik_request_t *req = NULL;
    ik_request_create(temp_ctx, "claude-sonnet-4-5", &req);

    ik_request_set_system(req, "System prompt");
    ik_request_add_message(req, IK_ROLE_USER, "Hello");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Hi");
    ik_request_add_tool(req, "tool1", "desc", "{}", false);

    /* Should not leak - talloc will verify */
    talloc_free(temp_ctx);

    ck_assert(1); /* If we get here, no crash */
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_suite(void)
{
    Suite *s = suite_create("Request Builders");

    TCase *tc_content_blocks = tcase_create("Content Blocks");
    tcase_set_timeout(tc_content_blocks, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_content_blocks, setup, teardown);
    tcase_add_test(tc_content_blocks, test_content_block_text);
    tcase_add_test(tc_content_blocks, test_content_block_tool_call);
    tcase_add_test(tc_content_blocks, test_content_block_tool_result);
    tcase_add_test(tc_content_blocks, test_content_block_tool_result_error);
    suite_add_tcase(s, tc_content_blocks);

    TCase *tc_request = tcase_create("Request Builders");
    tcase_set_timeout(tc_request, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_request_create);
    tcase_add_test(tc_request, test_request_set_system);
    tcase_add_test(tc_request, test_request_set_system_replace);
    tcase_add_test(tc_request, test_request_add_message);
    tcase_add_test(tc_request, test_request_add_multiple_messages);
    tcase_add_test(tc_request, test_request_add_message_blocks);
    tcase_add_test(tc_request, test_request_set_thinking);
    tcase_add_test(tc_request, test_request_add_tool);
    tcase_add_test(tc_request, test_request_add_multiple_tools);
    tcase_add_test(tc_request, test_request_memory_lifecycle);
    suite_add_tcase(s, tc_request);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
