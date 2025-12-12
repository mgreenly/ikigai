/**
 * @file clear_system_message_test.c
 * @brief Integration test for /clear command with system message persistence (Bug 5)
 *
 * Uses per-file database isolation for parallel test execution.
 */

#include "../../../src/commands.h"
#include "../../../src/agent.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"
#include <check.h>
#include <inttypes.h>
#include <libpq-fe.h>
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

// Helper to count messages by kind for a session
static int64_t count_messages_by_kind(ik_db_ctx_t *db_ctx, int64_t sid, const char *kind)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *query = "SELECT COUNT(*) FROM messages WHERE session_id = $1 AND kind = $2";
    const char *params[] = {
        talloc_asprintf(ctx, "%" PRId64, sid),
        kind
    };

    PGresult *result = PQexecParams(db_ctx->conn, query, 2, NULL, params, NULL, NULL, 0);

    int64_t count = 0;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0) {
        count = strtoll(PQgetvalue(result, 0, 0), NULL, 10);
    }

    PQclear(result);
    talloc_free(ctx);
    return count;
}

// ========== Tests ==========

// Test: /clear command should persist system message event when system message is configured
START_TEST(test_clear_persists_system_message_event)
{
    SKIP_IF_NO_DB();

    // Create a REPL context with system message configured
    ik_cfg_t *cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant.");
    ck_assert_ptr_nonnull(cfg->openai_system_message);

    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    repl->shared = shared;
    repl->shared->db_ctx = db;
    repl->shared->session_id = session_id;

    // Create scrollback
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    // Create conversation
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(is_ok(&conv_res));
    repl->conversation = conv_res.ok;

    // Initialize marks
    repl->marks = NULL;
    repl->mark_count = 0;
    repl->shared->db_debug_pipe = NULL;

    // Verify no messages initially
    ck_assert_int_eq(count_messages_by_kind(db, session_id, "clear"), 0);
    ck_assert_int_eq(count_messages_by_kind(db, session_id, "system"), 0);

    // Execute /clear command
    res_t clear_res = ik_cmd_dispatch(test_ctx, repl, "/clear");
    ck_assert(is_ok(&clear_res));
    if (is_ok(&clear_res) && clear_res.ok != NULL) {
        talloc_free(clear_res.ok);
    }

    // Verify clear event was written
    ck_assert_int_eq(count_messages_by_kind(db, session_id, "clear"), 1);

    // Bug 5: Verify system message event was written (THIS IS THE FIX)
    // After /clear, if system message is configured, it should be written to database
    ck_assert_int_eq(count_messages_by_kind(db, session_id, "system"), 1);
}
END_TEST

// Test: /clear command should NOT persist system message when system message is NULL
START_TEST(test_clear_no_system_message_when_null)
{
    SKIP_IF_NO_DB();

    // Create a REPL context WITHOUT system message
    ik_cfg_t *cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;  // No system message

    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    repl->shared = shared;
    repl->shared->db_ctx = db;
    repl->shared->session_id = session_id;

    // Create scrollback
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    // Create conversation
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(is_ok(&conv_res));
    repl->conversation = conv_res.ok;

    // Initialize marks
    repl->marks = NULL;
    repl->mark_count = 0;
    repl->shared->db_debug_pipe = NULL;

    // Execute /clear command
    res_t clear_res = ik_cmd_dispatch(test_ctx, repl, "/clear");
    ck_assert(is_ok(&clear_res));
    if (is_ok(&clear_res) && clear_res.ok != NULL) {
        talloc_free(clear_res.ok);
    }

    // Verify clear event was written
    ck_assert_int_eq(count_messages_by_kind(db, session_id, "clear"), 1);

    // Verify NO system message event was written (since cfg->openai_system_message is NULL)
    ck_assert_int_eq(count_messages_by_kind(db, session_id, "system"), 0);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *clear_system_message_suite(void)
{
    Suite *s = suite_create("Clear System Message");

    TCase *tc_core = tcase_create("Core");

    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    tcase_add_test(tc_core, test_clear_persists_system_message_event);
    tcase_add_test(tc_core, test_clear_no_system_message_when_null);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = clear_system_message_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
