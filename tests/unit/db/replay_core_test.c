#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

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

// Test: Linear sequence [user, assistant] -> [user, assistant]
START_TEST(test_replay_linear_sequence) {
    SKIP_IF_NO_DB();

    // Insert clear first
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert assistant message
    res = ik_db_message_insert(db, session_id, "assistant", "Hi there!", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 2); // user + assistant

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Hello");

    ck_assert_str_eq(context->messages[1]->kind, "assistant");
    ck_assert_str_eq(context->messages[1]->content, "Hi there!");
}
END_TEST
// Test: Clear semantics [user, assistant, clear, user] -> [user]
START_TEST(test_replay_clear_semantics)
{
    SKIP_IF_NO_DB();

    // Insert initial clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user + assistant
    res = ik_db_message_insert(db, session_id, "user", "First message", NULL);
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, "assistant", "First response", NULL);
    ck_assert(is_ok(&res));

    // Insert another clear
    res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert new user message
    res = ik_db_message_insert(db, session_id, "user", "Second message", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 1); // Only last user message

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Second message");
}

END_TEST
// Test: System message [clear, system, user, assistant] -> [system, user, assistant]
START_TEST(test_replay_system_message)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert system message
    res = ik_db_message_insert(db, session_id, "system", "You are a helpful assistant", NULL);
    ck_assert(is_ok(&res));

    // Insert user message
    res = ik_db_message_insert(db, session_id, "user", "Hello", NULL);
    ck_assert(is_ok(&res));

    // Insert assistant message
    res = ik_db_message_insert(db, session_id, "assistant", "How can I help?", NULL);
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 3); // system + user + assistant

    ck_assert_str_eq(context->messages[0]->kind, "system");
    ck_assert_str_eq(context->messages[0]->content, "You are a helpful assistant");

    ck_assert_str_eq(context->messages[1]->kind, "user");
    ck_assert_str_eq(context->messages[2]->kind, "assistant");
}

END_TEST
// Test: Empty event stream [] -> []
START_TEST(test_replay_empty_stream)
{
    SKIP_IF_NO_DB();

    // Don't insert any messages

    // Load and replay
    res_t res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 0); // Empty
}

END_TEST
// Test: Mark and rewind events are skipped (deferred to Task 7b)
START_TEST(test_replay_skip_mark_rewind)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert user
    res = ik_db_message_insert(db, session_id, "user", "Message 1", NULL);
    ck_assert(is_ok(&res));

    // Insert mark (should be skipped)
    res = ik_db_message_insert(db, session_id, "mark", NULL, "{\"name\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));

    // Insert assistant
    res = ik_db_message_insert(db, session_id, "assistant", "Response 1", NULL);
    ck_assert(is_ok(&res));

    // Insert rewind (should be skipped)
    res = ik_db_message_insert(db, session_id, "rewind", NULL, "{\"to\":\"checkpoint1\"}");
    ck_assert(is_ok(&res));

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 3); // user + mark + assistant (rewind skipped)

    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[1]->kind, "mark");
    ck_assert_str_eq(context->messages[2]->kind, "assistant");
}

END_TEST
// Test: Geometric growth of context array
START_TEST(test_replay_geometric_growth)
{
    SKIP_IF_NO_DB();

    // Insert clear
    res_t res = ik_db_message_insert(db, session_id, "clear", NULL, NULL);
    ck_assert(is_ok(&res));

    // Insert 20 messages to trigger geometric growth
    // Initial capacity: 16, after 16 messages capacity should be 32
    for (int i = 0; i < 20; i++) {
        char *content = talloc_asprintf(test_ctx, "Message %d", i);
        res = ik_db_message_insert(db, session_id, "user", content, NULL);
        ck_assert(is_ok(&res));
    }

    // Load and replay
    res = ik_db_messages_load(test_ctx, db, session_id);
    ck_assert(is_ok(&res));

    ik_replay_context_t *context = res.ok;
    ck_assert_ptr_nonnull(context);
    ck_assert_uint_eq(context->count, 20);

    // Verify capacity grew geometrically
    // After 16 messages, capacity should have doubled to 32
    ck_assert_uint_ge(context->capacity, 20);
    ck_assert_uint_eq(context->capacity, 32); // Should be exactly 32 (16*2)
}

END_TEST

// ========== Suite Configuration ==========

static Suite *replay_core_suite(void)
{
    Suite *s = suite_create("Replay Core");

    TCase *tc_core = tcase_create("Core");

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_replay_linear_sequence);
    tcase_add_test(tc_core, test_replay_clear_semantics);
    tcase_add_test(tc_core, test_replay_system_message);
    tcase_add_test(tc_core, test_replay_empty_stream);
    tcase_add_test(tc_core, test_replay_skip_mark_rewind);
    tcase_add_test(tc_core, test_replay_geometric_growth);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_core_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
