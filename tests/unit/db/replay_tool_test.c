#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <inttypes.h>
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

// Test: Replay session with tool_call, verify in context
START_TEST(test_replay_tool_call_message) {
    SKIP_IF_NO_DB();

    // Insert clear first
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, NULL, "user", "Find all C files", NULL);
    ck_assert(is_ok(&res));

    // Insert tool_call message with data_json
    const char *tool_call_data =
        "{\"id\":\"call_abc123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_call", "glob(pattern=\"*.c\")", tool_call_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 2); // user + tool_call

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Find all C files");

    ck_assert_str_eq(context->messages[1]->kind, "tool_call");
    ck_assert_str_eq(context->messages[1]->content, "glob(pattern=\"*.c\")");
    // Verify data_json is preserved (PostgreSQL JSONB may reformat, so check key fields)
    ck_assert_ptr_nonnull(context->messages[1]->data_json);
    ck_assert(strstr(context->messages[1]->data_json, "call_abc123") != NULL);
    ck_assert(strstr(context->messages[1]->data_json, "glob") != NULL);
}
END_TEST
// Test: Replay session with tool_result, verify in context
START_TEST(test_replay_tool_result_message) {
    SKIP_IF_NO_DB();

    // Insert clear first
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert tool_result message with data_json
    const char *tool_result_data =
        "{\"tool_call_id\":\"call_abc123\",\"name\":\"glob\",\"output\":\"file1.c\\nfile2.c\",\"success\":true}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_result", "2 files found", tool_result_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 1); // tool_result

    ck_assert_str_eq(context->messages[0]->kind, "tool_result");
    ck_assert_str_eq(context->messages[0]->content, "2 files found");
    // Verify data_json is preserved (PostgreSQL JSONB may reformat, so check key fields)
    ck_assert_ptr_nonnull(context->messages[0]->data_json);
    ck_assert(strstr(context->messages[0]->data_json, "call_abc123") != NULL);
    ck_assert(strstr(context->messages[0]->data_json, "glob") != NULL);
}

END_TEST
// Test: User → tool_call → tool_result → assistant sequence
START_TEST(test_replay_full_tool_conversation) {
    SKIP_IF_NO_DB();

    // Insert clear first
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, NULL, "user", "Find all C files", NULL);
    ck_assert(is_ok(&res));

    // Insert tool_call
    const char *tool_call_data =
        "{\"id\":\"call_abc123\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_call", "glob(pattern=\"*.c\")", tool_call_data);
    ck_assert(is_ok(&res));

    // Insert tool_result
    const char *tool_result_data =
        "{\"tool_call_id\":\"call_abc123\",\"name\":\"glob\",\"output\":\"file1.c\\nfile2.c\",\"success\":true}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_result", "2 files found", tool_result_data);
    ck_assert(is_ok(&res));

    // Insert assistant response
    res = ik_db_message_insert(db, session_id, NULL, "assistant", "I found 2 C files: file1.c and file2.c", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 4); // user + tool_call + tool_result + assistant

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "tool_call");
    ck_assert_str_eq(context->messages[2]->kind, "tool_result");
    ck_assert_str_eq(context->messages[3]->kind, "assistant");
}

END_TEST
// Test: Verify data_json is preserved for serialization
START_TEST(test_replay_tool_message_preserves_data_json) {
    SKIP_IF_NO_DB();

    // Insert clear first
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert tool_call with complex data_json
    const char *complex_data =
        "{\"id\":\"call_xyz\",\"type\":\"function\",\"function\":{\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\":\\\"TODO\\\",\\\"path\\\":\\\"src/\\\"}\"}}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_call", "grep(pattern=\"TODO\", path=\"src/\")",
                               complex_data);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 1);

    // Verify data_json is preserved (PostgreSQL JSONB may reformat, so check key fields)
    ck_assert_ptr_nonnull(context->messages[0]->data_json);
    ck_assert(strstr(context->messages[0]->data_json, "call_xyz") != NULL);
    ck_assert(strstr(context->messages[0]->data_json, "grep") != NULL);
    ck_assert(strstr(context->messages[0]->data_json, "TODO") != NULL);
}

END_TEST
// Test: Session with multiple tool call/result pairs
START_TEST(test_replay_multiple_tool_calls) {
    SKIP_IF_NO_DB();

    // Insert clear first
    res_t res = ik_db_message_insert(db, session_id, NULL, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // First tool call/result pair
    const char *tool_call_data_1 =
        "{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_call", "glob(pattern=\"*.c\")", tool_call_data_1);
    ck_assert(is_ok(&res));

    const char *tool_result_data_1 =
        "{\"tool_call_id\":\"call_1\",\"name\":\"glob\",\"output\":\"file1.c\",\"success\":true}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_result", "1 file found", tool_result_data_1);
    ck_assert(is_ok(&res));

    // Second tool call/result pair
    const char *tool_call_data_2 =
        "{\"id\":\"call_2\",\"type\":\"function\",\"function\":{\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\":\\\"TODO\\\"}\"}}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_call", "grep(pattern=\"TODO\")", tool_call_data_2);
    ck_assert(is_ok(&res));

    const char *tool_result_data_2 =
        "{\"tool_call_id\":\"call_2\",\"name\":\"grep\",\"output\":\"src/main.c:10: TODO\",\"success\":true}";
    res = ik_db_message_insert(db, session_id, NULL, "tool_result", "1 match found", tool_result_data_2);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id, NULL);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 4); // 2 tool_calls + 2 tool_results

    // Verify order and data preservation (PostgreSQL JSONB may reformat, so check key fields)
    ck_assert_str_eq(context->messages[0]->kind, "tool_call");
    ck_assert_ptr_nonnull(context->messages[0]->data_json);
    ck_assert(strstr(context->messages[0]->data_json, "call_1") != NULL);

    ck_assert_str_eq(context->messages[1]->kind, "tool_result");
    ck_assert_ptr_nonnull(context->messages[1]->data_json);
    ck_assert(strstr(context->messages[1]->data_json, "call_1") != NULL);

    ck_assert_str_eq(context->messages[2]->kind, "tool_call");
    ck_assert_ptr_nonnull(context->messages[2]->data_json);
    ck_assert(strstr(context->messages[2]->data_json, "call_2") != NULL);

    ck_assert_str_eq(context->messages[3]->kind, "tool_result");
    ck_assert_ptr_nonnull(context->messages[3]->data_json);
    ck_assert(strstr(context->messages[3]->data_json, "call_2") != NULL);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *replay_tool_suite(void)
{
    Suite *s = suite_create("Replay Tool Messages");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_replay_tool_call_message);
    tcase_add_test(tc_core, test_replay_tool_result_message);
    tcase_add_test(tc_core, test_replay_full_tool_conversation);
    tcase_add_test(tc_core, test_replay_tool_message_preserves_data_json);
    tcase_add_test(tc_core, test_replay_multiple_tool_calls);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/replay_tool_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
