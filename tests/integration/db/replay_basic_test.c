/**
 * @file replay_basic_test.c
 * @brief Integration tests for basic replay algorithm (linear sequences, clear)
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/db/message.h"
#include "../../../src/db/replay.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils.h"
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

// Suite-level setup
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

// Suite-level teardown
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
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

// Per-test teardown
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

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while(0)

// ========== Tests ==========

// Test 1: Empty session - no messages
START_TEST(test_replay_empty_session)
{
    SKIP_IF_NO_DB();

    // Replay empty session
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert(context != NULL);
    ck_assert_int_eq((int)context->count, 0);

    talloc_free(replay_ctx);
}
END_TEST

// Test 2: Single user message
START_TEST(test_replay_single_user_message)
{
    SKIP_IF_NO_DB();

    // Insert single user message
    ik_db_message_insert(db, session_id, NULL, "user", "Hello", "{}");

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 1);
    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "Hello");

    talloc_free(replay_ctx);
}
END_TEST

// Test 3: Conversation (user + assistant)
START_TEST(test_replay_conversation)
{
    SKIP_IF_NO_DB();

    // Insert conversation
    ik_db_message_insert(db, session_id, NULL, "user", "What is 2+2?", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "4", "{}");

    // Replay
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 2);
    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "What is 2+2?");
    ck_assert_str_eq(context->messages[1]->kind, "assistant");
    ck_assert_str_eq(context->messages[1]->content, "4");

    talloc_free(replay_ctx);
}
END_TEST

// Test 4: Clear event empties context
START_TEST(test_replay_clear_empties_context)
{
    SKIP_IF_NO_DB();

    // Insert messages then clear
    ik_db_message_insert(db, session_id, NULL, "user", "First", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "Response", "{}");
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");

    // Replay - context should be empty after clear
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 0);

    talloc_free(replay_ctx);
}
END_TEST

// Test 5: Messages after clear are preserved
START_TEST(test_replay_after_clear)
{
    SKIP_IF_NO_DB();

    // Insert messages, clear, then new message
    ik_db_message_insert(db, session_id, NULL, "user", "Old message", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "Old response", "{}");
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "New message", "{}");

    // Replay - should only have message after clear
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 1);
    ck_assert_str_eq(context->messages[0]->kind, "user");
    ck_assert_str_eq(context->messages[0]->content, "New message");

    talloc_free(replay_ctx);
}
END_TEST

// Test 6: System message handling
START_TEST(test_replay_system_message)
{
    SKIP_IF_NO_DB();

    // Insert clear, system, user (as per test spec)
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "system", "You are helpful", "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Hello", "{}");

    // Replay - should have system and user (clear doesn't persist)
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 2);
    ck_assert_str_eq(context->messages[0]->kind, "system");
    ck_assert_str_eq(context->messages[0]->content, "You are helpful");
    ck_assert_str_eq(context->messages[1]->kind, "user");
    ck_assert_str_eq(context->messages[1]->content, "Hello");

    talloc_free(replay_ctx);
}
END_TEST

// Test 7: Multiple clears
START_TEST(test_replay_multiple_clears)
{
    SKIP_IF_NO_DB();

    // Multiple clear cycles
    ik_db_message_insert(db, session_id, NULL, "user", "Msg1", "{}");
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Msg2", "{}");
    ik_db_message_insert(db, session_id, NULL, "clear", NULL, "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Msg3", "{}");

    // Replay - should only have last message
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 1);
    ck_assert_str_eq(context->messages[0]->content, "Msg3");

    talloc_free(replay_ctx);
}
END_TEST

// Test 8: Event ordering is preserved
START_TEST(test_replay_preserves_order)
{
    SKIP_IF_NO_DB();

    // Insert sequence
    ik_db_message_insert(db, session_id, NULL, "user", "Q1", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "A1", "{}");
    ik_db_message_insert(db, session_id, NULL, "user", "Q2", "{}");
    ik_db_message_insert(db, session_id, NULL, "assistant", "A2", "{}");

    // Replay and verify order
    TALLOC_CTX *replay_ctx = talloc_new(test_ctx);
    res_t replay_res = ik_db_messages_load(replay_ctx, db, session_id);
    ck_assert(is_ok(&replay_res));

    ik_replay_context_t *context = replay_res.ok;
    ck_assert_int_eq((int)context->count, 4);
    ck_assert_str_eq(context->messages[0]->content, "Q1");
    ck_assert_str_eq(context->messages[1]->content, "A1");
    ck_assert_str_eq(context->messages[2]->content, "Q2");
    ck_assert_str_eq(context->messages[3]->content, "A2");

    talloc_free(replay_ctx);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *replay_basic_suite(void)
{
    Suite *s = suite_create("Replay Basic");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_replay_empty_session);
    tcase_add_test(tc_core, test_replay_single_user_message);
    tcase_add_test(tc_core, test_replay_conversation);
    tcase_add_test(tc_core, test_replay_clear_empties_context);
    tcase_add_test(tc_core, test_replay_after_clear);
    tcase_add_test(tc_core, test_replay_system_message);
    tcase_add_test(tc_core, test_replay_multiple_clears);
    tcase_add_test(tc_core, test_replay_preserves_order);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = replay_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
