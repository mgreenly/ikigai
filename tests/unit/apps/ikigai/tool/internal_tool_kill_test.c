/**
 * @file internal_tool_kill_test.c
 * @brief Unit tests for kill tool handler
 */

#include "tests/test_constants.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/internal_tools.h"
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
res_t ik_db_agent_get(ik_db_ctx_t *db, void *ctx, const char *uuid, ik_db_agent_row_t **out);
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db, const char *uuid);

// Mock control flags
static bool mock_db_agent_get_fail = false;
static bool mock_db_mark_dead_fail = false;
static bool mock_agent_already_dead = false;
static bool mock_yyjson_read_fail = false;
static bool mock_target_is_root = false;
static bool mock_killing_parent = false;

// Mock: get agent
res_t ik_db_agent_get(ik_db_ctx_t *db, void *ctx, const char *uuid, ik_db_agent_row_t **out)
{
    if (mock_db_agent_get_fail) {
        return ERR(db, DB_CONNECT, "Mock agent get failure");
    }
    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    row->status = talloc_strdup(row, mock_agent_already_dead ? "dead" : "running");

    // For target agent (when testing root protection)
    if (strcmp(uuid, "target-uuid") == 0) {
        row->parent_uuid = mock_target_is_root ? NULL : talloc_strdup(row, "some-parent");
    }
    // For caller agent (when testing parent kill protection)
    else if (strcmp(uuid, "parent-uuid") == 0) {
        row->parent_uuid = mock_killing_parent ? talloc_strdup(row, "target-uuid") : NULL;
    }
    else {
        row->parent_uuid = talloc_strdup(row, "default-parent");
    }

    *out = row;
    return OK(NULL);
}

// Mock: mark agent dead
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db, const char *uuid)
{
    (void)uuid;
    if (mock_db_mark_dead_fail) {
        return ERR(db, DB_CONNECT, "Mock mark dead failure");
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
    mock_db_agent_get_fail = false;
    mock_db_mark_dead_fail = false;
    mock_agent_already_dead = false;
    mock_yyjson_read_fail = false;
    mock_target_is_root = false;
    mock_killing_parent = false;

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
    mock_db_agent_get_fail = false;
    mock_db_mark_dead_fail = false;
    mock_agent_already_dead = false;
    mock_yyjson_read_fail = false;
    mock_target_is_root = false;
    mock_killing_parent = false;
}

// Kill handler tests
START_TEST(test_kill_handler_success) {
    const char *args = "{\"uuid\":\"target-uuid\"}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success));

    yyjson_val *result_obj = yyjson_obj_get(root, "result");
    ck_assert_ptr_nonnull(result_obj);
    yyjson_val *killed = yyjson_obj_get(result_obj, "killed");
    ck_assert_ptr_nonnull(killed);
    ck_assert(yyjson_is_arr(killed));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_kill_handler_missing_uuid) {
    const char *args = "{}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
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

START_TEST(test_kill_handler_db_get_fail) {
    mock_db_agent_get_fail = true;
    const char *args = "{\"uuid\":\"target-uuid\"}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_kill_handler_db_mark_dead_fail) {
    mock_db_mark_dead_fail = true;
    const char *args = "{\"uuid\":\"target-uuid\"}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);

    yyjson_doc *doc = yyjson_read(result, strlen(result), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "tool_success");
    ck_assert(!yyjson_get_bool(success));
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_kill_handler_invalid_json) {
    mock_yyjson_read_fail = true;
    const char *args = "{bad json}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "PARSE_ERROR") != NULL);
    mock_yyjson_read_fail = false;
}
END_TEST

START_TEST(test_kill_handler_agent_already_dead) {
    mock_agent_already_dead = true;
    const char *args = "{\"uuid\":\"test-uuid\"}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "ALREADY_DEAD") != NULL);
    mock_agent_already_dead = false;
}
END_TEST

START_TEST(test_kill_on_complete_null_data) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    agent->tool_deferred_data = NULL;

    ik_internal_tool_kill_on_complete(repl, agent);

    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

START_TEST(test_kill_on_complete_with_agents) {
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    repl->agent_capacity = 3;
    repl->agent_count = 3;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 3);

    ik_agent_ctx_t *agent1 = talloc_zero(repl, ik_agent_ctx_t);
    agent1->uuid = talloc_strdup(agent1, "other-uuid-1");
    agent1->dead = false;
    repl->agents[0] = agent1;

    ik_agent_ctx_t *agent2 = talloc_zero(repl, ik_agent_ctx_t);
    agent2->uuid = talloc_strdup(agent2, "killed-uuid");
    agent2->dead = false;
    repl->agents[1] = agent2;

    ik_agent_ctx_t *agent3 = talloc_zero(repl, ik_agent_ctx_t);
    agent3->uuid = talloc_strdup(agent3, "other-uuid-2");
    agent3->dead = false;
    repl->agents[2] = agent3;

    char **killed_uuids = talloc_zero_array(agent, char *, 3);
    killed_uuids[0] = talloc_strdup(agent, "killed-uuid");
    killed_uuids[1] = talloc_strdup(agent, "non-existent-uuid");
    killed_uuids[2] = NULL;
    agent->tool_deferred_data = killed_uuids;

    ik_internal_tool_kill_on_complete(repl, agent);

    ck_assert(!agent1->dead);
    ck_assert(agent2->dead);
    ck_assert(!agent3->dead);
    ck_assert_ptr_null(agent->tool_deferred_data);
}
END_TEST

START_TEST(test_kill_handler_cannot_kill_root) {
    mock_target_is_root = true;
    const char *args = "{\"uuid\":\"target-uuid\"}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "CANNOT_KILL_ROOT") != NULL);
    ck_assert(strstr(result, "Cannot kill root agent") != NULL);
}
END_TEST

START_TEST(test_kill_handler_cannot_kill_parent) {
    mock_killing_parent = true;
    const char *args = "{\"uuid\":\"target-uuid\"}";
    char *result = ik_internal_tool_kill_handler(test_ctx, agent, args);
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "CANNOT_KILL_PARENT") != NULL);
    ck_assert(strstr(result, "Cannot kill parent agent") != NULL);
}
END_TEST

static Suite *internal_tool_kill_suite(void)
{
    Suite *s = suite_create("InternalToolKill");

    TCase *tc = tcase_create("Kill");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_kill_handler_success);
    tcase_add_test(tc, test_kill_handler_missing_uuid);
    tcase_add_test(tc, test_kill_handler_db_get_fail);
    tcase_add_test(tc, test_kill_handler_db_mark_dead_fail);
    tcase_add_test(tc, test_kill_handler_invalid_json);
    tcase_add_test(tc, test_kill_handler_agent_already_dead);
    tcase_add_test(tc, test_kill_handler_cannot_kill_root);
    tcase_add_test(tc, test_kill_handler_cannot_kill_parent);
    tcase_add_test(tc, test_kill_on_complete_null_data);
    tcase_add_test(tc, test_kill_on_complete_with_agents);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = internal_tool_kill_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/tool/internal_tool_kill_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
