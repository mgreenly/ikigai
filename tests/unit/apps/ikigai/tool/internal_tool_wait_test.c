/**
 * @file internal_tool_wait_test.c
 * @brief Unit tests for wait tool handler
 */

#include "tests/test_constants.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_wait_core.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/internal_tool_wait.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Forward declarations for mocks
void ik_wait_core_next_message(void *ctx, ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                                int32_t timeout_sec, _Bool *interrupt, ik_wait_result_t *out);
void ik_wait_core_fanin(void *ctx, ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                        int32_t timeout_sec, char **target_uuids, size_t target_count,
                        _Bool *interrupt, ik_wait_result_t *out);

// Mock control flags
static bool mock_wait_timeout = false;
static bool mock_wait_fanin_null_message = false;
static bool mock_yyjson_read_fail = false;

// Mock: yyjson_read
yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    if (mock_yyjson_read_fail) {
        return NULL;
    }
    return yyjson_read(dat, len, flg);
}

// Mock: wait core next message
void ik_wait_core_next_message(void *ctx, ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                                int32_t timeout_sec, _Bool *interrupt, ik_wait_result_t *out)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)timeout_sec;
    (void)interrupt;
    out->is_fanin = false;
    if (mock_wait_timeout) {
        out->from_uuid = NULL;
        out->message = NULL;
    } else {
        out->from_uuid = talloc_strdup(ctx, "sender-uuid");
        out->message = talloc_strdup(ctx, "hello");
    }
}

// Mock: wait core fanin
void ik_wait_core_fanin(void *ctx, ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                        int32_t timeout_sec, char **target_uuids, size_t target_count,
                        _Bool *interrupt, ik_wait_result_t *out)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)timeout_sec;
    (void)target_uuids;
    (void)interrupt;
    out->is_fanin = true;
    out->entry_count = target_count;
    out->entries = talloc_zero_array(ctx, ik_wait_fanin_entry_t, (unsigned int)target_count);
    for (size_t i = 0; i < target_count; i++) {
        out->entries[i].agent_uuid = talloc_strdup(ctx, target_uuids[i]);
        out->entries[i].agent_name = talloc_strdup(ctx, "agent-name");
        out->entries[i].status = talloc_strdup(ctx, "completed");
        if (mock_wait_fanin_null_message) {
            out->entries[i].message = NULL;
        } else {
            out->entries[i].message = talloc_strdup(ctx, "done");
        }
    }
}

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_agent_ctx_t *agent;
static ik_shared_ctx_t *shared;
static ik_db_ctx_t *db_ctx;

static void setup(void)
{
    mock_wait_timeout = false;
    mock_wait_fanin_null_message = false;
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
    mock_wait_timeout = false;
    mock_wait_fanin_null_message = false;
    mock_yyjson_read_fail = false;
}

// Wait handler tests
START_TEST(test_wait_handler_next_message) {
    const char *args = "{\"timeout\":5}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *from = yyjson_obj_get(result_obj, "from");
    ck_assert_ptr_nonnull(from);
    ck_assert_str_eq(yyjson_get_str(from), "sender-uuid");

    yyjson_val *message = yyjson_obj_get(result_obj, "message");
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(yyjson_get_str(message), "hello");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_wait_handler_fanin) {
    const char *args = "{\"timeout\":5,\"from_agents\":[\"agent-1\",\"agent-2\"]}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *results = yyjson_obj_get(result_obj, "results");
    ck_assert_ptr_nonnull(results);
    ck_assert(yyjson_is_arr(results));
    ck_assert_uint_eq(yyjson_arr_size(results), 2);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_wait_handler_fanin_null_message) {
    mock_wait_fanin_null_message = true;
    const char *args = "{\"timeout\":5,\"from_agents\":[\"agent-1\",\"agent-2\"]}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *results = yyjson_obj_get(result_obj, "results");
    ck_assert_ptr_nonnull(results);
    ck_assert(yyjson_is_arr(results));
    ck_assert_uint_eq(yyjson_arr_size(results), 2);

    yyjson_val *entry = yyjson_arr_get(results, 0);
    ck_assert_ptr_nonnull(entry);
    yyjson_val *message = yyjson_obj_get(entry, "message");
    ck_assert_ptr_null(message);

    yyjson_doc_free(doc);
    mock_wait_fanin_null_message = false;
}
END_TEST

START_TEST(test_wait_handler_missing_timeout) {
    const char *args = "{}";
    char *result = ik_wait_handler(test_ctx, agent, args);
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

START_TEST(test_wait_handler_invalid_json) {
    mock_yyjson_read_fail = true;
    const char *args = "{bad json}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "PARSE_ERROR") != NULL);
    mock_yyjson_read_fail = false;
}
END_TEST

START_TEST(test_wait_handler_timeout) {
    mock_wait_timeout = true;
    const char *args = "{\"timeout\":5}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *status = yyjson_obj_get(result_obj, "status");
    ck_assert_ptr_nonnull(status);
    ck_assert_str_eq(yyjson_get_str(status), "timeout");
    yyjson_doc_free(doc);
    mock_wait_timeout = false;
}
END_TEST

START_TEST(test_wait_handler_non_string_agent) {
    const char *args = "{\"timeout\":5,\"from_agents\":[123]}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(!yyjson_get_bool(success));
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    ck_assert(strstr(yyjson_get_str(error), "from_agents must contain strings") != NULL);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_wait_handler_from_agents_not_array) {
    const char *args = "{\"timeout\":5,\"from_agents\":\"not-an-array\"}";
    char *result = ik_wait_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_doc_free(doc);
}
END_TEST

static Suite *internal_tool_wait_suite(void)
{
    Suite *s = suite_create("InternalToolWait");

    TCase *tc = tcase_create("Wait");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_wait_handler_next_message);
    tcase_add_test(tc, test_wait_handler_fanin);
    tcase_add_test(tc, test_wait_handler_fanin_null_message);
    tcase_add_test(tc, test_wait_handler_missing_timeout);
    tcase_add_test(tc, test_wait_handler_invalid_json);
    tcase_add_test(tc, test_wait_handler_timeout);
    tcase_add_test(tc, test_wait_handler_non_string_agent);
    tcase_add_test(tc, test_wait_handler_from_agents_not_array);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_wait_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/internal_tool_wait_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
