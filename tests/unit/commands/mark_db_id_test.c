/**
 * @file mark_db_id_test.c
 * @brief Tests for get_mark_db_id and related edge cases
 */

#include "../../../src/agent.h"
#include "../../../src/commands_mark.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/db/connection.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/error.h"
#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

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

    res_t res = ik_openai_conversation_create(parent);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);

    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
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


    agent->conversation = conv;
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
    mock_ntuples = 0;
    mock_query_value = NULL;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: get_mark_db_id with query returning no results (line 48 branches)
START_TEST(test_mark_db_query_no_results) {
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "findme");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: Query succeeds but returns 0 rows
    mock_ntuples = 0;

    // Rewind - query returns no results
    res_t res = ik_cmd_rewind(ctx, repl, "findme");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}
END_TEST
// Test: get_mark_db_id with query failure (line 48 branches)
START_TEST(test_mark_db_query_failure)
{
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: Query fails
    mock_query_should_fail = true;

    // Rewind - query fails but rewind still succeeds in memory
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: get_mark_db_id with NULL db_ctx (line 24 branches)
START_TEST(test_mark_db_id_null_ctx)
{
    // No DB context
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Rewind - get_mark_db_id returns 0 due to NULL db_ctx
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: get_mark_db_id with session_id <= 0 (line 24 branches)
START_TEST(test_mark_db_id_invalid_session)
{
    // DB context set but invalid session
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = -1;  // Invalid

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Rewind - get_mark_db_id returns 0 due to invalid session_id
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: Rewind to unlabeled mark with DB query (NULL label path in get_mark_db_id)
START_TEST(test_rewind_unlabeled_mark_db_query)
{
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create an unlabeled mark in memory
    res_t mark_res = ik_mark_create(repl, NULL);
    ck_assert(is_ok(&mark_res));

    // Add a message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "test");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: SELECT succeeds with NULL label query
    mock_ntuples = 1;
    mock_query_value = "456";

    // Rewind to unlabeled mark (triggers lines 40-42)
    res_t res = ik_cmd_rewind(ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: sscanf fails with non-numeric string
START_TEST(test_mark_db_id_sscanf_non_numeric)
{
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: Query succeeds but returns non-numeric string
    mock_ntuples = 1;
    mock_query_value = "abc123";  // Invalid - contains letters

    // Rewind - sscanf fails, mark_id falls back to 0
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: sscanf fails with empty string
START_TEST(test_mark_db_id_sscanf_empty_string)
{
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: Query succeeds but returns empty string
    mock_ntuples = 1;
    mock_query_value = "";

    // Rewind - sscanf fails, mark_id falls back to 0
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: sscanf fails with special characters
START_TEST(test_mark_db_id_sscanf_special_chars)
{
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: Query succeeds but returns special characters
    mock_ntuples = 1;
    mock_query_value = "!@#$%";

    // Rewind - sscanf fails, mark_id falls back to 0
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: sscanf fails with text instead of number
START_TEST(test_mark_db_id_sscanf_text_only)
{
    // Set up DB context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    db_ctx->conn = (PGconn *)0x1234;
    repl->shared->db_ctx = db_ctx;
    repl->shared->session_id = 1;

    // Create mark in memory
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add message
    res_t msg_res = ik_openai_msg_create(repl->current->conversation, "user", "msg");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Mock: Query succeeds but returns text
    mock_ntuples = 1;
    mock_query_value = "not_a_number";

    // Rewind - sscanf fails, mark_id falls back to 0
    res_t res = ik_cmd_rewind(ctx, repl, "test");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST

// Suite definition
static Suite *commands_mark_db_id_suite(void)
{
    Suite *s = suite_create("Commands: Mark DB ID");
    TCase *tc = tcase_create("get_mark_db_id");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_mark_db_query_no_results);
    tcase_add_test(tc, test_mark_db_query_failure);
    tcase_add_test(tc, test_mark_db_id_null_ctx);
    tcase_add_test(tc, test_mark_db_id_invalid_session);
    tcase_add_test(tc, test_rewind_unlabeled_mark_db_query);
    tcase_add_test(tc, test_mark_db_id_sscanf_non_numeric);
    tcase_add_test(tc, test_mark_db_id_sscanf_empty_string);
    tcase_add_test(tc, test_mark_db_id_sscanf_special_chars);
    tcase_add_test(tc, test_mark_db_id_sscanf_text_only);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_mark_db_id_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
