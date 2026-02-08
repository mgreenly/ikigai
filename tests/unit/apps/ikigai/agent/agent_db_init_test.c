/**
 * @file agent_db_init_test.c
 * @brief Unit tests for agent database initialization error paths
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/paths.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
#include <stdbool.h>
#include <talloc.h>

// Forward declarations (ik_db_ctx_t comes from agent.h -> db/connection.h)
typedef struct {
    char *uuid;
    char *name;
    char *parent_uuid;
    char *fork_message_id;
    char *status;
    int64_t created_at;
} ik_db_agent_row_t;

res_t ik_db_init(TALLOC_CTX *mem_ctx, const char *conn_str, const char *data_dir, ik_db_ctx_t **out_ctx);
res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, const char *data_dir, void **out_ctx);

// Mock state for controlling DB init failure
static bool mock_db_init_should_fail = false;

// Suite-level setup
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

// Mock ik_db_init (without underscore)
res_t ik_db_init(TALLOC_CTX *mem_ctx, const char *conn_str, const char *data_dir, ik_db_ctx_t **out_ctx)
{
    (void)conn_str;
    (void)data_dir;

    if (mock_db_init_should_fail) {
        return ERR(mem_ctx, DB_CONNECT, "Mock database connection failure");
    }

    ik_db_ctx_t *dummy_ctx = talloc_zero(mem_ctx, ik_db_ctx_t);
    if (dummy_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        return ERR(mem_ctx, OUT_OF_MEMORY, "Out of memory");  // LCOV_EXCL_LINE
    }
    *out_ctx = dummy_ctx;
    return OK(dummy_ctx);
}

// Mock ik_db_init_ (with underscore)
res_t ik_db_init_(TALLOC_CTX *mem_ctx, const char *conn_str, const char *data_dir, void **out_ctx)
{
    (void)conn_str;
    (void)data_dir;

    if (mock_db_init_should_fail) {
        return ERR(mem_ctx, DB_CONNECT, "Mock database connection failure");
    }

    ik_db_ctx_t *dummy_ctx = talloc_zero(mem_ctx, ik_db_ctx_t);
    if (dummy_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        return ERR(mem_ctx, OUT_OF_MEMORY, "Out of memory");  // LCOV_EXCL_LINE
    }
    *out_ctx = (void *)dummy_ctx;
    return OK(dummy_ctx);
}

// Test: ik_agent_create with db_conn_str set and ik_db_init failure
START_TEST(test_agent_create_db_init_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Enable mock failure
    mock_db_init_should_fail = true;

    // Create shared context with db_conn_str set
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->db_conn_str = talloc_strdup(shared, "host=localhost port=5432");

    // Create paths for data_dir
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(shared, &paths);
        ck_assert(is_ok(&paths_res));
    }
    shared->paths = paths;

    // Attempt to create agent - should fail at ik_db_init
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(agent);
    ck_assert_int_eq(res.err->code, ERR_DB_CONNECT);

    // Cleanup mock state
    mock_db_init_should_fail = false;

    talloc_free(ctx);
}
END_TEST

// Test: ik_agent_restore with db_conn_str set and ik_db_init failure
START_TEST(test_agent_restore_db_init_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Enable mock failure
    mock_db_init_should_fail = true;

    // Create shared context with db_conn_str set
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->db_conn_str = talloc_strdup(shared, "host=localhost port=5432");

    // Create paths for data_dir
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(shared, &paths);
        ck_assert(is_ok(&paths_res));
    }
    shared->paths = paths;

    // Create a minimal DB row
    ik_db_agent_row_t *row = talloc_zero(ctx, ik_db_agent_row_t);
    ck_assert_ptr_nonnull(row);
    row->uuid = talloc_strdup(row, "test-uuid");
    row->name = talloc_strdup(row, "test-agent");
    row->parent_uuid = NULL;
    row->created_at = 123456789;
    row->fork_message_id = NULL;
    row->status = talloc_strdup(row, "running");

    // Attempt to restore agent - should fail at ik_db_init
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_restore(ctx, shared, row, &agent);

    // Verify failure
    ck_assert(is_err(&res));
    ck_assert_ptr_null(agent);
    ck_assert_int_eq(res.err->code, ERR_DB_CONNECT);

    // Cleanup mock state
    mock_db_init_should_fail = false;

    talloc_free(ctx);
}
END_TEST

static Suite *agent_db_init_suite(void)
{
    Suite *s = suite_create("Agent DB Init");

    TCase *tc = tcase_create("DB Failures");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_add_test(tc, test_agent_create_db_init_failure);
    tcase_add_test(tc, test_agent_restore_db_init_failure);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_db_init_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/agent/agent_db_init_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
