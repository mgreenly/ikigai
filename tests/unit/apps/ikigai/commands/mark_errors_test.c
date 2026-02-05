/**
 * @file mark_errors_test.c
 * @brief Tests for mark/rewind command error paths with mocked DB failures
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_mark.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/marks.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

// Mock state
static PGresult *mock_query_result = (PGresult *)2;   // For SELECT queries
static PGresult *mock_insert_result = (PGresult *)3;  // For INSERT queries
static PGresult *mock_failed_result = (PGresult *)1;  // For failures
static bool mock_query_should_fail = false;
static bool mock_insert_should_fail = false;
static int mock_ntuples = 0;  // Number of query result rows
static const char *mock_query_value = NULL;  // Value returned by PQgetvalue

// Mock pq_exec_params_ to distinguish SELECT vs INSERT
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

    // Distinguish between SELECT (queries) and INSERT (persistence)
    bool is_select = (command != NULL && strncmp(command, "SELECT", 6) == 0);

    if (is_select) {
        if (mock_query_should_fail) {
            return mock_failed_result;
        }
        return mock_query_result;
    } else {
        // INSERT/UPDATE/DELETE
        if (mock_insert_should_fail) {
            return mock_failed_result;
        }
        return mock_insert_result;
    }
}

// Mock PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    if (res == mock_query_result) {
        return PGRES_TUPLES_OK;
    }
    if (res == mock_insert_result) {
        return PGRES_COMMAND_OK;
    }
    return PGRES_FATAL_ERROR;
}

// Mock PQntuples
int PQntuples(const PGresult *res)
{
    if (res == mock_query_result) {
        return mock_ntuples;
    }
    return 0;
}

// Mock PQgetvalue
char *PQgetvalue(const PGresult *res, int row_number, int column_number)
{
    (void)row_number;
    (void)column_number;
    static char empty[] = "";
    if (res == mock_query_result && mock_query_value != NULL) {
        // Safe: PostgreSQL's PQgetvalue returns non-const, caller doesn't modify
        return (char *)(uintptr_t)mock_query_value;
    }
    return empty;
}

// Mock PQclear (no-op)
void PQclear(PGresult *res)
{
    (void)res;
}

// Mock PQerrorMessage
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock DB error";
    return error_msg;
}

/**
 * Create a REPL context with conversation for testing
 */
static ik_repl_ctx_t *create_test_repl_with_conversation(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    r->current = agent;

    r->shared = shared;
    r->current->marks = NULL;
    r->current->mark_count = 0;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_conversation(ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock state
    mock_query_should_fail = false;
    mock_insert_should_fail = false;
    mock_ntuples = 0;
    mock_query_value = NULL;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Mark with unlabeled DB insert error
START_TEST(test_mark_unlabeled_db_error) {
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Mock INSERT to fail
    mock_insert_should_fail = true;

    // Create unlabeled mark - should succeed in memory despite DB error
    res_t res = ik_cmd_mark(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->mark_count, 1);
}

END_TEST
// Test: Rewind to non-existent mark (error path lines 113-120)
START_TEST(test_rewind_mark_not_found) {
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Mock query to return 0 rows (mark not found)
    mock_ntuples = 0;

    // Try to rewind to non-existent mark
    res_t res = ik_cmd_rewind(ctx, repl, "nonexistent");
    ck_assert(is_ok(&res));  // Command doesn't propagate error

    // Verify error was appended to scrollback
    ck_assert(repl->current->scrollback->count > 0);
}

END_TEST
// Test: Rewind with DB insert error
START_TEST(test_rewind_db_error) {
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create a mark in memory
    res_t mark_res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&mark_res));

    // Add a message to conversation
    ik_message_t *msg_created = ik_message_create_text(ctx, IK_ROLE_USER, "test");
    // removed assertion
    ik_agent_add_message(repl->current, msg_created);
    // removed assertion

    // Mock: SELECT succeeds (finds mark), INSERT fails
    mock_ntuples = 1;
    mock_query_value = "123";  // Fake message ID
    mock_insert_should_fail = true;

    // Rewind should succeed in memory despite DB error
    res_t res = ik_cmd_rewind(ctx, repl, "checkpoint");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Mark with db_ctx set but session_id = 0 (line 76 branches)
START_TEST(test_mark_with_db_ctx_but_no_session) {
    // Set up DB context but invalid session
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 0;  // Invalid session

    // Create mark - should not attempt DB operations
    res_t res = ik_cmd_mark(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->mark_count, 1);
}

END_TEST
// Test: Rewind with db_ctx but session_id = 0 (line 142 branches)
START_TEST(test_rewind_with_db_ctx_but_no_session) {
    // Set up DB context but invalid session
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 0;  // Invalid session

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    ik_message_t *msg_created = ik_message_create_text(ctx, IK_ROLE_USER, "msg");
    // removed assertion
    ik_agent_add_message(repl->current, msg_created);
    // removed assertion

    // Rewind - should not attempt DB operations
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Rewind with valid DB but target_message_id = 0 (line 142 branches)
START_TEST(test_rewind_with_zero_message_id) {
    // Set up valid DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    ik_message_t *msg_created = ik_message_create_text(ctx, IK_ROLE_USER, "msg");
    // removed assertion
    ik_agent_add_message(repl->current, msg_created);
    // removed assertion

    // Mock: Query returns 0 rows (target_message_id will be 0)
    mock_ntuples = 0;

    // Rewind - should not persist to DB (target_message_id == 0)
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST

// Suite definition
static Suite *commands_mark_errors_suite(void)
{
    Suite *s = suite_create("Commands: Mark/Rewind Errors");
    TCase *tc = tcase_create("DB Errors");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_mark_unlabeled_db_error);
    tcase_add_test(tc, test_rewind_mark_not_found);
    tcase_add_test(tc, test_rewind_db_error);
    tcase_add_test(tc, test_mark_with_db_ctx_but_no_session);
    tcase_add_test(tc, test_rewind_with_db_ctx_but_no_session);
    tcase_add_test(tc, test_rewind_with_zero_message_id);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_mark_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/mark_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
