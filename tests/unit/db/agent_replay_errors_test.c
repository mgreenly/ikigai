// Error path tests for db/agent_replay.c using mocks
#include "../../../src/db/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/msg.h"
#include "../../../src/wrapper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// Mock connection for error tests (no real DB needed)
static ik_db_ctx_t *create_mock_db_ctx(TALLOC_CTX *ctx)
{
    ik_db_ctx_t *db = talloc_zero(ctx, ik_db_ctx_t);
    db->conn = (PGconn *)0xDEADBEEF; // Non-NULL placeholder
    return db;
}

// Mock results - using pointer constants
static PGresult *mock_failed_result = (PGresult *)1;
static PGresult *mock_success_result = (PGresult *)2;
static PGresult *mock_parse_fail_result = (PGresult *)3;

// Mock control flags
static bool mock_query_fail = false;
static bool mock_parse_fail = false;
static char mock_invalid_value[32] = "";
static bool mock_null_content = false;
static bool mock_null_data = false;

// Override pq_exec_params_ to control failures
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
    (void)command;
    (void)nParams;
    (void)paramTypes;
    (void)paramValues;
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    if (mock_query_fail) {
        return mock_failed_result;
    }
    if (mock_parse_fail) {
        return mock_parse_fail_result;
    }
    return mock_success_result;
}

// Override PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    if (res == mock_parse_fail_result) {
        return PGRES_TUPLES_OK; // Query succeeds but data is invalid
    }
    return PGRES_TUPLES_OK;
}

// Override PQerrorMessage
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock database error";
    return error_msg;
}

// Override PQclear
void PQclear(PGresult *res)
{
    (void)res;
}

// Override PQntuples - return 1 for parse failure tests
int PQntuples(const PGresult *res)
{
    if (res == mock_parse_fail_result || mock_null_content || mock_null_data) {
        return 1; // Has data but it's invalid or has null fields
    }
    return 0;
}

// Override PQgetvalue_ - return invalid data for parse failure tests
char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;

    // Column 0 = id, Column 1 = kind, Column 2 = content, Column 3 = data_json
    if (mock_parse_fail && column_number == 0 && mock_invalid_value[0] != '\0') {
        return mock_invalid_value;
    }

    if (column_number == 0) {
        static char default_id[] = "1";
        return mock_invalid_value[0] != '\0' ? mock_invalid_value : default_id;
    }
    if (column_number == 1) {
        static char kind[] = "user";
        return kind;
    }

    static char valid[] = "content";
    return valid;
}

// Override PQgetisnull
int PQgetisnull(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;

    // Column 2 is content, column 3 is data_json
    if (column_number == 2 && mock_null_content) {
        return 1;
    }
    if (column_number == 3 && mock_null_data) {
        return 1;
    }
    return 0;
}

// Test: ik_agent_find_clear handles query failure (line 56)
START_TEST(test_find_clear_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    int64_t clear_id = 0;
    res_t res = ik_agent_find_clear(db, "test-uuid", 0, &clear_id);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to find clear") != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: ik_agent_find_clear handles parse failure (line 67)
START_TEST(test_find_clear_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    snprintf(mock_invalid_value, sizeof(mock_invalid_value), "not_a_number");

    int64_t clear_id = 0;
    res_t res = ik_agent_find_clear(db, "test-uuid", 0, &clear_id);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse clear ID") != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: ik_agent_query_range handles query failure (line 216)
START_TEST(test_query_range_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    char *uuid = talloc_strdup(ctx, "test-uuid");
    ik_replay_range_t range = {
        .agent_uuid = uuid,
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t count = 0;
    res_t res = ik_agent_query_range(db, ctx, &range, &messages, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to query range") != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: ik_agent_query_range handles message ID parse failure (line 241)
START_TEST(test_query_range_message_id_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    snprintf(mock_invalid_value, sizeof(mock_invalid_value), "not_a_number");

    char *uuid = talloc_strdup(ctx, "test-uuid");
    ik_replay_range_t range = {
        .agent_uuid = uuid,
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t count = 0;
    res_t res = ik_agent_query_range(db, ctx, &range, &messages, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse message ID") != NULL);

    talloc_free(ctx);
}
END_TEST

// Test: ik_agent_query_range with NULL content field (line 255)
START_TEST(test_query_range_null_content)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = false;
    mock_null_content = true;
    mock_null_data = false;
    snprintf(mock_invalid_value, sizeof(mock_invalid_value), "1"); // Valid ID

    char *uuid = talloc_strdup(ctx, "test-uuid");
    ik_replay_range_t range = {
        .agent_uuid = uuid,
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t count = 0;
    res_t res = ik_agent_query_range(db, ctx, &range, &messages, &count);

    // Should succeed with NULL content
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_ptr_null(messages[0]->content);

    mock_null_content = false;
    talloc_free(ctx);
}
END_TEST

// Test: ik_agent_query_range with NULL data_json field (line 263)
START_TEST(test_query_range_null_data)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = false;
    mock_null_content = false;
    mock_null_data = true;
    snprintf(mock_invalid_value, sizeof(mock_invalid_value), "1"); // Valid ID

    char *uuid = talloc_strdup(ctx, "test-uuid");
    ik_replay_range_t range = {
        .agent_uuid = uuid,
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t count = 0;
    res_t res = ik_agent_query_range(db, ctx, &range, &messages, &count);

    // Should succeed with NULL data_json
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_ptr_null(messages[0]->data_json);

    mock_null_data = false;
    talloc_free(ctx);
}
END_TEST

// Setup function to reset mock state before each test
static void setup(void)
{
    mock_query_fail = false;
    mock_parse_fail = false;
    mock_invalid_value[0] = '\0';
    mock_null_content = false;
    mock_null_data = false;
}

// Suite configuration
static Suite *agent_replay_errors_suite(void)
{
    Suite *s = suite_create("Agent Replay Errors");

    TCase *tc_errors = tcase_create("Errors");
    tcase_add_checked_fixture(tc_errors, setup, NULL);
    tcase_add_test(tc_errors, test_find_clear_query_failure);
    tcase_add_test(tc_errors, test_find_clear_parse_failure);
    tcase_add_test(tc_errors, test_query_range_query_failure);
    tcase_add_test(tc_errors, test_query_range_message_id_parse_failure);
    tcase_add_test(tc_errors, test_query_range_null_content);
    tcase_add_test(tc_errors, test_query_range_null_data);

    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
