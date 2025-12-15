/**
 * @file cmd_send_errors_test.c
 * @brief Coverage tests for /send command error paths
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/mail.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/mail/msg.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Test fixtures
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    res_t res = ik_openai_conversation_create(test_ctx);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;

    ik_cfg_t *cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;
    agent->conversation = conv;
    agent->uuid = talloc_strdup(agent, "sender-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    shared->session_id = 1;
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Insert sender agent into registry
    res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert sender agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup sender agent in registry");
    }
}

static bool suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to create database: %s\n", error_message(res.err));
        talloc_free(res.err);
        return false;
    }
    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to migrate database: %s\n", error_message(res.err));
        talloc_free(res.err);
        ik_test_db_destroy(DB_NAME);
        return false;
    }
    return true;
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    res_t db_res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to connect to database: %s\n", error_message(db_res.err));
        ck_abort_msg("Database connection failed");
    }
    ck_assert_ptr_nonnull(db);
    ck_assert_ptr_nonnull(db->conn);

    // Begin transaction for test isolation
    db_res = ik_test_db_begin(db);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to begin transaction: %s\n", error_message(db_res.err));
        ck_abort_msg("Begin transaction failed");
    }

    // Create session for mail tests
    int64_t session_id = 0;
    db_res = ik_db_session_create(db, &session_id);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to create session: %s\n", error_message(db_res.err));
        ck_abort_msg("Session creation failed");
    }

    setup_repl();

    // Update shared context with actual session_id
    repl->shared->session_id = session_id;
}

static void teardown(void)
{
    // Rollback transaction to discard test changes
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_rollback(db);
    }

    // Free everything
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: missing args shows error
START_TEST(test_send_missing_args)
{
    res_t res = cmd_send(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: empty args shows error
START_TEST(test_send_empty_args)
{
    res_t res = cmd_send(test_ctx, repl, "");
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: only whitespace shows error
START_TEST(test_send_only_whitespace)
{
    res_t res = cmd_send(test_ctx, repl, "   ");
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: missing message part shows error
START_TEST(test_send_missing_message)
{
    res_t res = cmd_send(test_ctx, repl, "some-uuid");
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: UUID too long shows error
START_TEST(test_send_uuid_too_long)
{
    char long_uuid[300];
    memset(long_uuid, 'x', sizeof(long_uuid) - 10);
    strcpy(long_uuid + sizeof(long_uuid) - 10, " \"msg\"");

    res_t res = cmd_send(test_ctx, repl, long_uuid);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: missing opening quote shows error
START_TEST(test_send_missing_opening_quote)
{
    res_t res = cmd_send(test_ctx, repl, "uuid-123 message\"");
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: missing closing quote shows error
START_TEST(test_send_missing_closing_quote)
{
    res_t res = cmd_send(test_ctx, repl, "uuid-123 \"message");
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: message too long shows error
START_TEST(test_send_message_too_long)
{
    char long_msg[5000];
    memset(long_msg, 'x', sizeof(long_msg) - 20);
    strcpy(long_msg, "uuid-123 \"");
    memset(long_msg + 10, 'x', sizeof(long_msg) - 12);
    long_msg[sizeof(long_msg) - 2] = '"';
    long_msg[sizeof(long_msg) - 1] = '\0';

    res_t res = cmd_send(test_ctx, repl, long_msg);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

static Suite *send_errors_suite(void)
{
    Suite *s = suite_create("Send Command Errors");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_send_missing_args);
    tcase_add_test(tc, test_send_empty_args);
    tcase_add_test(tc, test_send_only_whitespace);
    tcase_add_test(tc, test_send_missing_message);
    tcase_add_test(tc, test_send_uuid_too_long);
    tcase_add_test(tc, test_send_missing_opening_quote);
    tcase_add_test(tc, test_send_missing_closing_quote);
    tcase_add_test(tc, test_send_message_too_long);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = send_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
