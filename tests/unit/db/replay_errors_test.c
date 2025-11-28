#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/pg_result.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <inttypes.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========
// Each test file gets its own database for parallel execution

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup: Create and migrate database (runs once)
static void suite_setup(void)
{
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

// Suite-level teardown: Destroy database (runs once)
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup: Connect, begin transaction, create session
static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    // Create a session for tests
    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Per-test teardown: Rollback and cleanup
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        if (db != NULL) {
            ik_test_db_rollback(db);
        }
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Tests ==========

// Test: Mark stack capacity expansion (line 60 in ensure_mark_stack_capacity)
// Initial capacity is 4, so insert 5+ marks to trigger geometric growth
START_TEST(test_mark_stack_capacity_expansion) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert 10 marks to trigger geometric growth
    // Initial mark stack capacity: 4
    // After 4 marks, capacity should double to 8
    // After 8 marks, capacity should double to 16
    for (int i = 0; i < 10; i++) {
        char *label = talloc_asprintf(test_ctx, "{\"label\":\"mark%d\"}", i);
        res = ik_db_message_insert(db, session_id, "mark", NULL, label);
        ck_assert(is_ok(&res));
    }

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 10); // 10 marks
    ck_assert_uint_eq(context->mark_stack.count, 10); // 10 marks on stack

    // Verify capacity grew geometrically
    // After 4: capacity = 8, after 8: capacity = 16
    ck_assert_uint_ge(context->mark_stack.capacity, 10);
    ck_assert_uint_eq(context->mark_stack.capacity, 16);
}
END_TEST
// Test: Malformed rewind event - missing data field (line 195)
START_TEST(test_rewind_missing_data)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert mark
    res = ik_db_message_insert(db, session_id, "mark", NULL, "{\"label\":\"checkpoint\"}");
    ck_assert(is_ok(&res));

    // Insert rewind with NULL data (malformed - should be logged and skipped)
    res = ik_db_message_insert(db, session_id, "rewind", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert another message after malformed rewind
    res = ik_db_message_insert(db, session_id, "user", "After rewind", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Rewind should be skipped due to missing data, so we have: user + mark + user
    ck_assert_uint_eq(context->count, 3);

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_str_eq(context->messages[2]->kind, "user");
}

END_TEST
// Test: Malformed rewind event - missing target_message_id field (line 208)
START_TEST(test_rewind_missing_target_message_id)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert mark
    res = ik_db_message_insert(db, session_id, "mark", NULL, "{\"label\":\"checkpoint\"}");
    ck_assert(is_ok(&res));

    // Insert rewind with valid JSON but missing target_message_id
    res = ik_db_message_insert(db, session_id, "rewind", NULL, "{\"other_field\":123}");
    ck_assert(is_ok(&res));

    // Insert another message
    res = ik_db_message_insert(db, session_id, "user", "After rewind", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Malformed rewind should be skipped
    ck_assert_uint_eq(context->count, 3);
}

END_TEST
// Test: Rewind with non-integer target_message_id (line 208)
START_TEST(test_rewind_invalid_target_message_id_type)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert mark
    res = ik_db_message_insert(db, session_id, "mark", NULL, "{\"label\":\"checkpoint\"}");
    ck_assert(is_ok(&res));

    // Insert rewind with string target_message_id (should be integer)
    res = ik_db_message_insert(db, session_id, "rewind", NULL, "{\"target_message_id\":\"not_an_int\"}");
    ck_assert(is_ok(&res));

    // Insert another message
    res = ik_db_message_insert(db, session_id, "user", "After rewind", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Malformed rewind should be skipped
    ck_assert_uint_eq(context->count, 3);
}

END_TEST
// Test: Rewind with target mark not found (line 219)
START_TEST(test_rewind_mark_not_found)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind with non-existent mark ID (999999)
    res = ik_db_message_insert(db, session_id, "rewind", NULL, "{\"target_message_id\":999999}");
    ck_assert(is_ok(&res));

    // Insert another message
    res = ik_db_message_insert(db, session_id, "user", "After rewind", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    // Rewind with non-existent mark should be skipped
    ck_assert_uint_eq(context->count, 2);
}

END_TEST

// Test: Database query failure (lines 283-287)
// We need to mock pq_exec_params_ to return an error
static bool mock_query_failure = false;

PGresult *pq_exec_params_(PGconn *conn, const char *command, int nParams,
                          const Oid *paramTypes, const char *const *paramValues,
                          const int *paramLengths, const int *paramFormats, int resultFormat)
{
    if (mock_query_failure) {
        // Return a failed result
        PGresult *res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
        return res;
    }
    // Call real function
    return PQexecParams(conn, command, nParams, paramTypes, paramValues,
                        paramLengths, paramFormats, resultFormat);
}

START_TEST(test_database_query_failure) {
    SKIP_IF_NO_DB();

    // Enable mock query failure
    mock_query_failure = true;

    // Try to load messages - should fail with database error
    res_t res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_IO);

    // Disable mock
    mock_query_failure = false;
}
END_TEST

// Test: Invalid JSON in rewind data_json (lines 201-202)
// Mock yyjson_read_ to return NULL, simulating invalid JSON
static bool mock_invalid_json = false;

yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    if (mock_invalid_json) {
        return NULL;  // Simulate invalid JSON
    }
    // Delegate to real function
    extern yyjson_doc *yyjson_read(const char *dat, size_t len, yyjson_read_flag flg);
    return yyjson_read(dat, len, flg);
}

START_TEST(test_rewind_invalid_json) {
    SKIP_IF_NO_DB();

    // Insert a mark first
    res_t mark_res = ik_db_message_insert(db, session_id, "mark", "test_mark",
                                          "{\"label\": \"mark1\"}");
    ck_assert(is_ok(&mark_res));

    // Get mark ID from database - just insert a rewind with some target
    // The key is that yyjson_read_ will fail, not the target validity
    res_t rewind_res = ik_db_message_insert(db, session_id, "rewind", NULL,
                                            "{\"target_message_id\": 999}");
    ck_assert(is_ok(&rewind_res));

    // Enable mock to simulate invalid JSON
    mock_invalid_json = true;

    // Load messages - should handle invalid JSON gracefully (log error, skip rewind)
    res_t res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));  // Should succeed despite invalid JSON
    ik_replay_context_t *context = res.ok;

    // Context should contain the mark but not process the rewind
    ck_assert_int_eq((int)context->count, 1);  // Only the mark

    // Disable mock
    mock_invalid_json = false;
}
END_TEST
// Test: Unknown event kind (lines 236-237)
START_TEST(test_unknown_event_kind)
{
    SKIP_IF_NO_DB();

    // Bypass the database CHECK constraint by directly inserting an unknown kind
    // We use a raw SQL INSERT to bypass the constraint
    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO messages (session_id, kind, content, data, created_at) "
             "VALUES (%" PRId64 ", 'unknown_kind', 'test', NULL, NOW())",
             session_id);
    PGresult *insert_res = PQexec(db->conn, query);

    // Check if constraint prevents this (it will, so we need to disable it temporarily)
    if (PQresultStatus(insert_res) != PGRES_COMMAND_OK) {
        PQclear(insert_res);
        // Disable CHECK constraint temporarily
        PQexec(db->conn, "ALTER TABLE messages DROP CONSTRAINT IF EXISTS messages_kind_check");
        insert_res = PQexec(db->conn, query);
        ck_assert_int_eq(PQresultStatus(insert_res), PGRES_COMMAND_OK);
    }
    PQclear(insert_res);

    // Load messages - should handle unknown kind gracefully (log error, skip)
    res_t res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));  // Should succeed despite unknown kind
    ik_replay_context_t *context = res.ok;

    // Context should be empty (unknown kind ignored)
    ck_assert_int_eq((int)context->count, 0);

    // Re-enable constraint
    ik_db_wrap_pg_result(test_ctx, PQexec(db->conn, "ALTER TABLE messages ADD CONSTRAINT messages_kind_check "
                                          "CHECK (kind IN ('system', 'user', 'assistant', 'clear', 'mark', 'rewind'))"));
}

END_TEST

// Test: sscanf failure when parsing message ID (lines 304-306)
// Mock PQgetvalue_ to return non-numeric string for ID field
static bool mock_invalid_id = false;
static char invalid_id_str[] = "not_a_number";

char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    if (mock_invalid_id && column_number == 0) {
        return invalid_id_str;  // Return invalid ID string
    }
    return PQgetvalue(res, row_number, column_number);
}

START_TEST(test_sscanf_parse_failure) {
    SKIP_IF_NO_DB();

    // Insert a message
    res_t insert_res = ik_db_message_insert(db, session_id, "user", "test", NULL);
    ck_assert(is_ok(&insert_res));

    // Enable mock to return invalid ID
    mock_invalid_id = true;

    // Load messages - should fail with PARSE error
    res_t res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_PARSE);

    // Disable mock
    mock_invalid_id = false;
}
END_TEST

// ========== Suite Configuration ==========

static Suite *replay_errors_suite(void)
{
    Suite *s = suite_create("Replay Errors");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_mark_stack_capacity_expansion);
    tcase_add_test(tc_core, test_rewind_missing_data);
    tcase_add_test(tc_core, test_rewind_missing_target_message_id);
    tcase_add_test(tc_core, test_rewind_invalid_target_message_id_type);
    tcase_add_test(tc_core, test_rewind_mark_not_found);
    tcase_add_test(tc_core, test_database_query_failure);
    tcase_add_test(tc_core, test_rewind_invalid_json);
    tcase_add_test(tc_core, test_unknown_event_kind);
    tcase_add_test(tc_core, test_sscanf_parse_failure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
