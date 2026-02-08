/**
 * @file cmd_send_core_test.c
 * @brief Unit tests for ik_send_core with error_msg_out parameter
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/mail.h"
#include "apps/ikigai/db/session.h"
#include "apps/ikigai/mail/msg.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

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
static int64_t session_id;

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

    // Create session
    db_res = ik_db_session_create(db, &session_id);
    if (is_err(&db_res)) {
        fprintf(stderr, "Failed to create session: %s\n", error_message(db_res.err));
        ck_abort_msg("Session creation failed");
    }
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

// Test: error_msg_out populated when body is empty
START_TEST(test_send_core_empty_body_error_msg) {
    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id,
                             "sender-uuid", "recipient-uuid", "", &error_msg);
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(error_msg);
    ck_assert_str_eq(error_msg, "Message body cannot be empty");
    talloc_free(error_msg);
}
END_TEST

// Test: error_msg_out populated when recipient not found
START_TEST(test_send_core_recipient_not_found_error_msg) {
    char *error_msg = NULL;
    res_t res = ik_send_core(test_ctx, db, session_id,
                             "sender-uuid", "nonexistent-uuid", "Hello", &error_msg);
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(error_msg);
    // Error message should mention failed query
    ck_assert_ptr_nonnull(strstr(error_msg, "Failed to query recipient"));
    talloc_free(error_msg);
}
END_TEST

// Test: error_msg_out populated when recipient is dead
START_TEST(test_send_core_dead_recipient_error_msg) {
    // Create and register recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(test_ctx, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "dead-recipient-uuid");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567890;
    recipient->fork_message_id = 0;

    // Create minimal shared context for the agent
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->session_id = session_id;
    recipient->shared = shared;

    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Mark as dead
    res = ik_db_agent_mark_dead(db, recipient->uuid);
    ck_assert(is_ok(&res));

    // Call send_core with error_msg_out
    char *error_msg = NULL;
    res = ik_send_core(test_ctx, db, session_id,
                       "sender-uuid", recipient->uuid, "Hello", &error_msg);
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(error_msg);
    ck_assert_str_eq(error_msg, "Recipient agent is dead");
    talloc_free(error_msg);
}
END_TEST

// Test: ik_cmd_send displays error_msg when send_core fails
START_TEST(test_cmd_send_displays_error_msg) {
    // Create minimal REPL context
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    agent->uuid = talloc_strdup(agent, "sender-uuid");
    agent->scrollback = sb;
    agent->shared = shared;
    repl->current = agent;
    repl->shared = shared;
    shared->db_ctx = db;
    shared->session_id = session_id;

    // Create agent array with one agent (self)
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;

    // Call send with empty body (triggers error path with error_msg_out)
    size_t initial_lines = ik_scrollback_get_line_count(sb);
    res_t res = ik_cmd_send(test_ctx, repl, "sender-uuid \"\"");
    ck_assert(is_ok(&res));

    // Verify error message was displayed in scrollback
    size_t final_lines = ik_scrollback_get_line_count(sb);
    ck_assert_uint_gt(final_lines, initial_lines);
}
END_TEST

static Suite *send_core_suite(void)
{
    Suite *s = suite_create("Send Core Error Messages");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_send_core_empty_body_error_msg);
    tcase_add_test(tc, test_send_core_recipient_not_found_error_msg);
    tcase_add_test(tc, test_send_core_dead_recipient_error_msg);
    tcase_add_test(tc, test_cmd_send_displays_error_msg);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = send_core_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_send_core_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
