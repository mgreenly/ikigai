/**
 * @file internal_tool_fork_test.c
 * @brief Unit tests for fork tool handler
 */

#include "tests/test_constants.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/internal_tool_fork.h"
#include "apps/ikigai/repl.h"
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
res_t ik_agent_create(void *parent, ik_shared_ctx_t *shared, const char *parent_uuid, ik_agent_ctx_t **out);
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db, const char *agent_uuid, int64_t *out);
res_t ik_db_agent_insert(ik_db_ctx_t *db, const ik_agent_ctx_t *agent);
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                           const char *kind, const char *content, const char *data_json);
res_t ik_repl_add_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

// Mock control flags
static bool mock_agent_create_fail = false;
static bool mock_db_get_last_message_fail = false;
static bool mock_db_agent_insert_fail = false;
static bool mock_yyjson_read_fail = false;

// Mock: create child agent
res_t ik_agent_create(void *parent, ik_shared_ctx_t *shared, const char *parent_uuid, ik_agent_ctx_t **out)
{
    (void)parent_uuid;
    if (mock_agent_create_fail) {
        return ERR(parent, DB_CONNECT, "Mock agent create failure");
    }
    ik_agent_ctx_t *agent = talloc_zero(parent, ik_agent_ctx_t);
    agent->uuid = talloc_strdup(agent, "child-uuid-123");
    agent->shared = shared;
    *out = agent;
    return OK(NULL);
}

// Mock: get last message ID
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db, const char *agent_uuid, int64_t *out)
{
    (void)db;
    (void)agent_uuid;
    if (mock_db_get_last_message_fail) {
        return ERR(db, DB_CONNECT, "Mock get last message failure");
    }
    *out = 999;
    return OK(NULL);
}

// Mock: insert agent
res_t ik_db_agent_insert(ik_db_ctx_t *db, const ik_agent_ctx_t *agent)
{
    (void)agent;
    if (mock_db_agent_insert_fail) {
        return ERR(db, DB_CONNECT, "Mock agent insert failure");
    }
    return OK(NULL);
}

// Mock: insert message
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid,
                           const char *kind, const char *content, const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;
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

// Mock: repl add agent
res_t ik_repl_add_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    (void)repl;
    (void)agent;
    return OK(NULL);
}

// Test fixture
static TALLOC_CTX *test_ctx;
static ik_agent_ctx_t *agent;
static ik_shared_ctx_t *shared;
static ik_db_ctx_t *db_ctx;

static void setup(void)
{
    mock_agent_create_fail = false;
    mock_db_get_last_message_fail = false;
    mock_db_agent_insert_fail = false;
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
    mock_agent_create_fail = false;
    mock_db_get_last_message_fail = false;
    mock_db_agent_insert_fail = false;
    mock_yyjson_read_fail = false;
}

// Fork handler tests
START_TEST(test_fork_handler_success) {
    const char *args = "{\"name\":\"worker\",\"prompt\":\"analyze data\"}";
    char *result = ik_fork_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    // Verify result is wrapped success
    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *child_uuid = yyjson_obj_get(result_obj, "child_uuid");
    ck_assert_ptr_nonnull(child_uuid);
    ck_assert_str_eq(yyjson_get_str(child_uuid), "child-uuid-123");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_fork_handler_missing_name) {
    const char *args = "{\"prompt\":\"analyze data\"}";
    char *result = ik_fork_handler(test_ctx, agent, args);
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

START_TEST(test_fork_handler_missing_prompt) {
    const char *args = "{\"name\":\"worker\"}";
    char *result = ik_fork_handler(test_ctx, agent, args);
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

START_TEST(test_fork_on_complete) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ik_agent_ctx_t *child = talloc_zero(agent->tool_thread_ctx, ik_agent_ctx_t);
    child->uuid = talloc_strdup(child, "child-uuid");
    agent->tool_deferred_data = child;

    ik_fork_on_complete(repl, agent);

    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

START_TEST(test_fork_on_complete_null_data) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    agent->tool_deferred_data = NULL;

    ik_fork_on_complete(repl, agent);

    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

START_TEST(test_fork_handler_agent_create_fail) {
    mock_agent_create_fail = true;
    const char *args = "{\"name\":\"worker\",\"prompt\":\"analyze data\"}";
    char *result = ik_fork_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_fork_handler_db_get_last_message_fail) {
    mock_db_get_last_message_fail = true;
    const char *args = "{\"name\":\"worker\",\"prompt\":\"analyze data\"}";
    char *result = ik_fork_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_fork_handler_db_agent_insert_fail) {
    mock_db_agent_insert_fail = true;
    const char *args = "{\"name\":\"worker\",\"prompt\":\"analyze data\"}";
    char *result = ik_fork_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_fork_handler_invalid_json) {
    mock_yyjson_read_fail = true;
    const char *args = "{bad json}";
    char *result = ik_fork_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "PARSE_ERROR") != NULL);
    mock_yyjson_read_fail = false;
}
END_TEST

static Suite *internal_tool_fork_suite(void)
{
    Suite *s = suite_create("InternalToolFork");

    TCase *tc = tcase_create("Fork");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_fork_handler_success);
    tcase_add_test(tc, test_fork_handler_missing_name);
    tcase_add_test(tc, test_fork_handler_missing_prompt);
    tcase_add_test(tc, test_fork_on_complete);
    tcase_add_test(tc, test_fork_on_complete_null_data);
    tcase_add_test(tc, test_fork_handler_agent_create_fail);
    tcase_add_test(tc, test_fork_handler_db_get_last_message_fail);
    tcase_add_test(tc, test_fork_handler_db_agent_insert_fail);
    tcase_add_test(tc, test_fork_handler_invalid_json);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_fork_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/internal_tool_fork_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
