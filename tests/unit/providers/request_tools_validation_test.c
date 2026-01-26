/**
 * @file request_tools_validation_test.c
 * @brief Tests for model validation and request building (lines 239-289)
 */

#include "../../../src/providers/request.h"
#include "../../../src/agent.h"
#include "../../../src/doc_cache.h"
#include "../../../src/error.h"
#include "../../../src/paths.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static ik_shared_ctx_t *shared_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    shared_ctx = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared_ctx->cfg = ik_test_create_config(shared_ctx);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/**
 * Test error when model is NULL (line 239 true branch - first condition)
 */
START_TEST(test_null_model_error) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = NULL;
    agent->thinking_level = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/**
 * Test error when model is empty string (line 239 true branch - second condition)
 */
START_TEST(test_empty_model_error) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "");
    agent->thinking_level = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/**
 * Test success when model is valid (line 239 false branch, line 245 false branch)
 */
START_TEST(test_valid_model_success) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    // Line 245: is_err(&res) is false - request created successfully
    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req);
    ck_assert_str_eq(req->model, "gpt-4");
}
END_TEST

/**
 * Test with system message (now uses pinned documents instead of cfg)
 */
START_TEST(test_with_system_message) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    // Pinned documents system - no pinned paths means no system prompt
    agent->pinned_paths = NULL;
    agent->pinned_count = 0;
    agent->doc_cache = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    // With no pinned documents, system_prompt should be NULL
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test with NULL shared context (line 249 - first condition false)
 */
START_TEST(test_null_shared_context) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = NULL;  // NULL shared context
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test with NULL config (line 249 - second condition false)
 */
START_TEST(test_null_config) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->shared->cfg = NULL;  // NULL config
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test without system message (line 249 - third condition false)
 */
START_TEST(test_without_system_message) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    agent->shared->cfg->openai_system_message = NULL;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(req->system_prompt);
}
END_TEST

/**
 * Test with different thinking levels (line 249)
 */
START_TEST(test_different_thinking_levels) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "o1-preview");
    agent->thinking_level = 2;  // Extended thinking
    agent->messages = NULL;
    agent->message_count = 0;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->thinking.level, 2);
}
END_TEST

/**
 * Test with NULL message in array (line 260 continue)
 */
START_TEST(test_skip_null_message) {
    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;

    agent->message_count = 2;
    agent->messages = talloc_array(agent, ik_message_t *, 2);

    agent->messages[0] = talloc_zero(agent, ik_message_t);
    agent->messages[0]->role = IK_ROLE_USER;
    agent->messages[0]->content_count = 1;
    agent->messages[0]->content_blocks = talloc_array(agent->messages[0], ik_content_block_t, 1);
    agent->messages[0]->content_blocks[0].type = IK_CONTENT_TEXT;
    agent->messages[0]->content_blocks[0].data.text.text = talloc_strdup(agent->messages[0], "Hi");

    agent->messages[1] = NULL;  // Should be skipped

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)req->message_count, 1);  // Only 1 message copied
}
END_TEST

/**
 * Test with pinned documents - system prompt built from doc_cache
 */
START_TEST(test_with_pinned_documents) {
    // Create temp file with content
    char tmpfile[] = "/tmp/iktest_pinned_XXXXXX";
    int32_t fd = mkstemp(tmpfile);
    ck_assert_int_ge(fd, 0);
    const char *doc_content = "System prompt from pinned doc\n";
    ssize_t written = write(fd, doc_content, strlen(doc_content));
    ck_assert_int_eq(written, (ssize_t)strlen(doc_content));
    close(fd);

    ik_agent_ctx_t *agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared_ctx;
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;

    // Set up paths for doc_cache
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    res_t paths_res = ik_paths_init(agent, &paths);
    ck_assert(is_ok(&paths_res));
    ck_assert_ptr_nonnull(paths);

    // Set up doc_cache
    agent->doc_cache = ik_doc_cache_create(agent, paths);
    ck_assert_ptr_nonnull(agent->doc_cache);

    // Pin the temp file
    agent->pinned_paths = talloc_array(agent, char *, 1);
    agent->pinned_paths[0] = talloc_strdup(agent, tmpfile);
    agent->pinned_count = 1;

    ik_request_t *req = NULL;
    res_t result = ik_request_build_from_conversation(test_ctx, agent, NULL, &req);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(req->system_prompt);
    ck_assert(strstr(req->system_prompt, doc_content) != NULL);

    unlink(tmpfile);
}
END_TEST

static Suite *request_tools_validation_suite(void)
{
    Suite *s = suite_create("Request Tools Validation");

    TCase *tc_validation = tcase_create("Model Validation");
    tcase_set_timeout(tc_validation, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_validation, setup, teardown);
    tcase_add_test(tc_validation, test_null_model_error);
    tcase_add_test(tc_validation, test_empty_model_error);
    tcase_add_test(tc_validation, test_valid_model_success);
    suite_add_tcase(s, tc_validation);

    TCase *tc_system = tcase_create("System Message");
    tcase_set_timeout(tc_system, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_system, setup, teardown);
    tcase_add_test(tc_system, test_with_system_message);
    tcase_add_test(tc_system, test_null_shared_context);
    tcase_add_test(tc_system, test_null_config);
    tcase_add_test(tc_system, test_without_system_message);
    suite_add_tcase(s, tc_system);

    TCase *tc_messages = tcase_create("Message Array");
    tcase_set_timeout(tc_messages, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_messages, setup, teardown);
    tcase_add_test(tc_messages, test_different_thinking_levels);
    tcase_add_test(tc_messages, test_skip_null_message);
    suite_add_tcase(s, tc_messages);

    TCase *tc_pinned = tcase_create("Pinned Documents");
    tcase_set_timeout(tc_pinned, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_pinned, setup, teardown);
    tcase_add_test(tc_pinned, test_with_pinned_documents);
    suite_add_tcase(s, tc_pinned);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_tools_validation_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/request_tools_validation_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
