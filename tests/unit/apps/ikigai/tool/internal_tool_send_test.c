/**
 * @file internal_tool_send_test.c
 * @brief Unit tests for send tool handler
 */

#include "tests/test_constants.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/internal_tools.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Forward declarations for mocks
res_t ik_send_core(void *ctx, ik_db_ctx_t *db, int64_t session_id, const char *from_uuid,
                   const char *to_uuid, const char *message, char **error_msg);

// Mock control flags
static bool mock_send_core_fail = false;
static const char *mock_send_error_msg = NULL;
static bool mock_yyjson_read_fail = false;

// Mock: send core
res_t ik_send_core(void *ctx, ik_db_ctx_t *db, int64_t session_id, const char *from_uuid,
                   const char *to_uuid, const char *message, char **error_msg)
{
    (void)session_id;
    (void)from_uuid;
    (void)to_uuid;
    (void)message;
    if (mock_send_core_fail) {
        if (error_msg) {
            *error_msg = talloc_strdup(ctx, mock_send_error_msg);
        }
        return ERR(db, DB_CONNECT, "Mock send core failure");
    }
    return OK(NULL);
}

// Mock: yyjson_read
yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    if (mock_yyjson_read_fail) {
        return NULL;
    }
    return yyjson_read(dat, len, flg);
}

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_agent_ctx_t *agent;
static ik_shared_ctx_t *shared;
static ik_db_ctx_t *db_ctx;

static void setup(void)
{
    mock_send_core_fail = false;
    mock_send_error_msg = NULL;
    mock_yyjson_read_fail = false;

    test_ctx = talloc_new(NULL);
    shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->session_id = 123;
    db_ctx = talloc_zero(test_ctx, ik_db_ctx_t);

    agent = talloc_zero(test_ctx, ik_agent_ctx_t);
    agent->shared = shared;
    agent->worker_db_ctx = db_ctx;
    agent->uuid = talloc_strdup(agent, "parent-uuid");
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->thinking_level = 0;
    agent->tool_thread_ctx = talloc_new(agent);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    mock_send_core_fail = false;
    mock_send_error_msg = NULL;
    mock_yyjson_read_fail = false;
}

// Send handler tests
START_TEST(test_send_handler_success) {
    const char *args = "{\"to\":\"recipient-uuid\",\"message\":\"hello\"}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *status = yyjson_obj_get(result_obj, "status");
    ck_assert_ptr_nonnull(status);
    ck_assert_str_eq(yyjson_get_str(status), "sent");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_send_handler_missing_to) {
    const char *args = "{\"message\":\"hello\"}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_send_handler_missing_message) {
    const char *args = "{\"to\":\"recipient-uuid\"}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_send_handler_send_core_fail) {
    mock_send_core_fail = true;
    const char *args = "{\"to\":\"recipient-uuid\",\"message\":\"hello\"}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_send_handler_invalid_json) {
    mock_yyjson_read_fail = true;
    const char *args = "{bad json}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "PARSE_ERROR") != NULL);
    mock_yyjson_read_fail = false;
}
END_TEST

START_TEST(test_send_handler_error_msg_null) {
    mock_send_core_fail = true;
    mock_send_error_msg = NULL;
    const char *args = "{\"to\":\"recipient\",\"message\":\"test\"}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "SEND_FAILED") != NULL);
    mock_send_core_fail = false;
}
END_TEST

START_TEST(test_send_handler_error_msg_set) {
    mock_send_core_fail = true;
    mock_send_error_msg = "Custom error message";
    const char *args = "{\"to\":\"recipient\",\"message\":\"test\"}";
    char *result = ik_internal_tool_send_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "Custom error message") != NULL);
    mock_send_core_fail = false;
    mock_send_error_msg = NULL;
}
END_TEST

static Suite *internal_tool_send_suite(void)
{
    Suite *s = suite_create("InternalToolSend");

    TCase *tc = tcase_create("Send");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_send_handler_success);
    tcase_add_test(tc, test_send_handler_missing_to);
    tcase_add_test(tc, test_send_handler_missing_message);
    tcase_add_test(tc, test_send_handler_send_core_fail);
    tcase_add_test(tc, test_send_handler_invalid_json);
    tcase_add_test(tc, test_send_handler_error_msg_null);
    tcase_add_test(tc, test_send_handler_error_msg_set);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_send_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/internal_tool_send_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
