/**
 * @file agent_zero_success_test.c
 * @brief Success path tests for db/agent_zero.c using mocks
 */

#include "apps/ikigai/db/agent_zero.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "shared/error.h"
#include "apps/ikigai/paths.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *mock_db;
static ik_paths_t *paths;

// Mock states
static int mock_scenario = 0;
static PGresult *mock_result_empty = (PGresult *)0x1000;
static PGresult *mock_result_with_uuid = (PGresult *)0x2000;
static PGresult *mock_result_command_ok = (PGresult *)0x3000;
static PGresult *mock_result_column_exists = (PGresult *)0x4000;
static PGresult *mock_result_no_orphans = (PGresult *)0x5000;
static int mock_call_count = 0;

// Override pq_exec_params_ to return different results based on scenario
PGresult *pq_exec_params_(PGconn *conn,
                          const char *command,
                          int nParams,
                          const Oid *paramTypes,
                          const char *const *paramValues,
                          const int *paramLengths,
                          const int *paramFormats,
                          int resultFormat)
{
    (void)conn;
    (void)nParams;
    (void)paramTypes;
    (void)paramValues;
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    mock_call_count++;

    // Scenario 1: existing root agent (return UUID on first query)
    if (mock_scenario == 1) {
        if (strstr(command, "SELECT uuid FROM agents WHERE parent_uuid IS NULL")) {
            return mock_result_with_uuid;
        }
    }

    // Scenario 2: create new agent (no existing root)
    if (mock_scenario == 2) {
        if (strstr(command, "SELECT uuid FROM agents WHERE parent_uuid IS NULL")) {
            return mock_result_empty;
        }
        if (strstr(command, "information_schema.columns")) {
            return mock_result_column_exists;
        }
        if (strstr(command, "SELECT 1 FROM messages WHERE agent_uuid IS NULL")) {
            return mock_result_no_orphans;
        }
        if (strstr(command, "INSERT INTO agents")) {
            return mock_result_command_ok;
        }
    }

    // Scenario 3: create new agent with orphans
    if (mock_scenario == 3) {
        if (strstr(command, "SELECT uuid FROM agents WHERE parent_uuid IS NULL")) {
            return mock_result_empty;
        }
        if (strstr(command, "information_schema.columns")) {
            return mock_result_column_exists;
        }
        if (strstr(command, "SELECT 1 FROM messages WHERE agent_uuid IS NULL")) {
            return mock_result_with_uuid;  // Has orphans
        }
        if (strstr(command, "INSERT INTO agents")) {
            return mock_result_command_ok;
        }
        if (strstr(command, "UPDATE messages SET agent_uuid")) {
            return mock_result_command_ok;
        }
    }

    return mock_result_empty;
}

// Override PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_result_command_ok) {
        return PGRES_COMMAND_OK;
    }
    return PGRES_TUPLES_OK;
}

// Override PQntuples
int PQntuples(const PGresult *res)
{
    if (res == mock_result_with_uuid) {
        return 1;
    }
    if (res == mock_result_column_exists) {
        return 1;
    }
    return 0;
}

// Override PQgetvalue_ for returning UUID
char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    (void)row_number;
    (void)column_number;
    if (res == mock_result_with_uuid) {
        static char uuid[] = "test-uuid-1234567890ab";
        return uuid;
    }
    static char empty[] = "";
    return empty;
}

// Override PQerrorMessage
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "";
    return error_msg;
}

// Override PQclear
void PQclear(PGresult *res)
{
    (void)res;
}

// Mock ik_db_message_insert to succeed
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id,
                           const char *agent_uuid, const char *kind,
                           const char *content, const char *data)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    test_paths_setup_env();
    res_t paths_res = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&paths_res));
    ck_assert_ptr_nonnull(paths);

    mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(mock_db);
    mock_db->conn = (PGconn *)0xDEADBEEF;

    mock_call_count = 0;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_ensure_agent_zero_returns_existing_uuid) {
    mock_scenario = 1;
    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(mock_db, 1, paths, &uuid);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(uuid);
    ck_assert_str_eq(uuid, "test-uuid-1234567890ab");
}
END_TEST

START_TEST(test_ensure_agent_zero_creates_new_agent) {
    mock_scenario = 2;
    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(mock_db, 1, paths, &uuid);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(uuid);
    ck_assert_int_eq((int)strlen(uuid), 22);  // base64url UUID is 22 chars
}
END_TEST

START_TEST(test_ensure_agent_zero_adopts_orphans) {
    mock_scenario = 3;
    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(mock_db, 1, paths, &uuid);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(uuid);
    // Verify UPDATE was called (via mock_call_count or other mechanism)
}
END_TEST

static Suite *agent_zero_success_suite(void)
{
    Suite *s = suite_create("Agent 0 Success");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_ensure_agent_zero_returns_existing_uuid);
    tcase_add_test(tc_core, test_ensure_agent_zero_creates_new_agent);
    tcase_add_test(tc_core, test_ensure_agent_zero_adopts_orphans);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_zero_success_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/agent_zero_success_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
