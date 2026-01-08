/**
 * @file cmd_check_mail_coverage_test.c
 * @brief Coverage tests for /check-mail command edge cases
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
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

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

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;

    agent->uuid = talloc_strdup(agent, "recipient-uuid-123");
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

    // Insert recipient agent into registry
    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert recipient agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup recipient agent in registry");
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

// Test: check-mail with message exactly 59 seconds old (< 60)
START_TEST(test_check_mail_timestamp_seconds) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-abc");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567891;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with timestamp 59 seconds ago
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Recent message");
    ck_assert_ptr_nonnull(msg);
    msg->timestamp = (int64_t)time(NULL) - 59;
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST
// Test: check-mail with message 120 seconds old (< 3600)
START_TEST(test_check_mail_timestamp_minutes) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-def");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567892;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with timestamp 2 minutes ago
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Message from minutes ago");
    ck_assert_ptr_nonnull(msg);
    msg->timestamp = (int64_t)time(NULL) - 120;
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: check-mail with message 7200 seconds old (< 86400)
START_TEST(test_check_mail_timestamp_hours) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-ghi");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567893;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with timestamp 2 hours ago
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Message from hours ago");
    ck_assert_ptr_nonnull(msg);
    msg->timestamp = (int64_t)time(NULL) - 7200;
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: check-mail with message exactly 1 hour old (singular form)
START_TEST(test_check_mail_timestamp_1_hour) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-jkl");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567894;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with timestamp exactly 1 hour ago
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Message from 1 hour ago");
    ck_assert_ptr_nonnull(msg);
    msg->timestamp = (int64_t)time(NULL) - 3600;
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: check-mail with message >= 86400 seconds old (days)
START_TEST(test_check_mail_timestamp_days) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-mno");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567895;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with timestamp 2 days ago
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Message from days ago");
    ck_assert_ptr_nonnull(msg);
    msg->timestamp = (int64_t)time(NULL) - 172800;
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: check-mail with message exactly 1 day old (singular form)
START_TEST(test_check_mail_timestamp_1_day) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-pqr");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567896;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with timestamp exactly 1 day ago
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Message from 1 day ago");
    ck_assert_ptr_nonnull(msg);
    msg->timestamp = (int64_t)time(NULL) - 86400;
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: check-mail with short message (body <= 50 chars)
START_TEST(test_check_mail_short_body) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-stu");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567897;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create message with short body (exactly 50 chars)
    char short_msg[51];
    memset(short_msg, 'x', 50);
    short_msg[50] = '\0';

    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, short_msg);
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: check-mail with single message (singular form in summary)
START_TEST(test_check_mail_single_message) {
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-vwx");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567898;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create exactly one message
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                            repl->current->uuid, "Single message");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Check mail
    res = ik_cmd_check_mail(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST

static Suite *check_mail_coverage_suite(void)
{
    Suite *s = suite_create("Check Mail Command Coverage");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_check_mail_timestamp_seconds);
    tcase_add_test(tc, test_check_mail_timestamp_minutes);
    tcase_add_test(tc, test_check_mail_timestamp_hours);
    tcase_add_test(tc, test_check_mail_timestamp_1_hour);
    tcase_add_test(tc, test_check_mail_timestamp_days);
    tcase_add_test(tc, test_check_mail_timestamp_1_day);
    tcase_add_test(tc, test_check_mail_short_body);
    tcase_add_test(tc, test_check_mail_single_message);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = check_mail_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
