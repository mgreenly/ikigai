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

// Mock control flags (used by override functions defined later)
static bool mock_invalid_json_for_mark = false;
static int mock_call_count = 0;
static bool mock_null_label_str = false;
static int mock_get_str_count = 0;

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
    // Reset mock state
    mock_invalid_json_for_mark = false;
    mock_call_count = 0;
    mock_null_label_str = false;
    mock_get_str_count = 0;

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

// Test: Mark with NULL data_json (line 163, FALSE branch)
// This tests the case where a mark has no data field (no label)
START_TEST(test_mark_with_null_data) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, NULL, "user", "Before mark", NULL);
    ck_assert(is_ok(&res));

    // Insert mark with NULL data (no label)
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 2); // user + mark

    // Verify mark was added
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_uint_eq(context->mark_stack.count, 1);
    // Label should be NULL (no data field)
    ck_assert_ptr_null(context->mark_stack.marks[0].label);
}
END_TEST

// Test: Mark with invalid JSON in data_json (line 166, FALSE branch)
// Mock yyjson_read_ to return NULL for invalid JSON

yyjson_doc *yyjson_read_(const char *dat, size_t len, yyjson_read_flag flg)
{
    // Only fail on the first call (for mark), not subsequent calls
    if (mock_invalid_json_for_mark && mock_call_count == 0) {
        mock_call_count++;
        return NULL;  // Simulate invalid JSON
    }
    mock_call_count++;
    // Delegate to real function
    extern yyjson_doc *yyjson_read(const char *dat, size_t len, yyjson_read_flag flg);
    return yyjson_read(dat, len, flg);
}

START_TEST(test_mark_with_invalid_json_data) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert mark with data that will fail to parse
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"test\"}");
    ck_assert(is_ok(&res));

    // Enable mock to simulate invalid JSON parsing
    mock_invalid_json_for_mark = true;
    mock_call_count = 0;

    // Load and replay - should succeed but mark has no label
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 1); // mark

    // Verify mark was added with NULL label (JSON parse failed)
    ck_assert_str_eq(context->messages[0]->kind, "mark");
    ck_assert_uint_eq(context->mark_stack.count, 1);
    ck_assert_ptr_null(context->mark_stack.marks[0].label);

    // Disable mock
    mock_invalid_json_for_mark = false;
    mock_call_count = 0;
}
END_TEST
// Test: Mark with valid JSON but label is not a string (line 168, branch coverage)
// This tests the case where label field exists but is not a string type
START_TEST(test_mark_with_non_string_label)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert mark with label as number instead of string
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":123}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 1); // mark

    // Verify mark was added with NULL label (label was not a string)
    ck_assert_str_eq(context->messages[0]->kind, "mark");
    ck_assert_uint_eq(context->mark_stack.count, 1);
    ck_assert_ptr_null(context->mark_stack.marks[0].label);
}

END_TEST

// Test: Mark with valid JSON but yyjson_get_str_ returns NULL (line 170, FALSE branch)

const char *yyjson_get_str_(yyjson_val *val)
{
    if (mock_null_label_str && mock_get_str_count == 0) {
        mock_get_str_count++;
        return NULL;  // Simulate NULL string
    }
    mock_get_str_count++;
    // Delegate to real function
    extern const char *yyjson_get_str(yyjson_val *val);
    return yyjson_get_str(val);
}

START_TEST(test_mark_with_null_label_string) {
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert mark with valid label
    res = ik_db_message_insert(db, session_id, NULL, "mark", NULL, "{\"label\":\"test\"}");
    ck_assert(is_ok(&res));

    // Enable mock to return NULL from yyjson_get_str_
    mock_null_label_str = true;
    mock_get_str_count = 0;

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 1); // mark

    // Verify mark was added with NULL label
    ck_assert_str_eq(context->messages[0]->kind, "mark");
    ck_assert_uint_eq(context->mark_stack.count, 1);
    ck_assert_ptr_null(context->mark_stack.marks[0].label);

    // Disable mock
    mock_null_label_str = false;
    mock_get_str_count = 0;
}
END_TEST

// ========== Suite Configuration ==========

static Suite *replay_coverage_suite(void)
{
    Suite *s = suite_create("Replay Coverage");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_mark_with_null_data);
    tcase_add_test(tc_core, test_mark_with_invalid_json_data);
    tcase_add_test(tc_core, test_mark_with_non_string_label);
    tcase_add_test(tc_core, test_mark_with_null_label_string);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
