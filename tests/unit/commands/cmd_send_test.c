/**
 * @file cmd_send_test.c
 * @brief Unit tests for /send command
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
    res_t res = ik_db_agent_insert(db, agent);
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

// Test: /send creates mail record
START_TEST(test_send_creates_mail) {
    // Create recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "recipient-uuid-456");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567891;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Send mail
    res = ik_cmd_send(test_ctx, repl, "recipient-uuid-456 \"Hello, world!\"");
    ck_assert(is_ok(&res));

    // Verify mail exists in database
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           recipient->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
}
END_TEST
// Test: mail has correct from/to UUIDs
START_TEST(test_send_correct_uuids) {
    // Create recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "recipient-uuid-789");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567892;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Send mail
    res = ik_cmd_send(test_ctx, repl, "recipient-uuid-789 \"Test message\"");
    ck_assert(is_ok(&res));

    // Verify UUIDs
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           recipient->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_str_eq(inbox[0]->from_uuid, "sender-uuid-123");
    ck_assert_str_eq(inbox[0]->to_uuid, "recipient-uuid-789");
}

END_TEST
// Test: mail body stored correctly
START_TEST(test_send_body_stored) {
    // Create recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "recipient-uuid-abc");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567893;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Send mail with specific body
    res = ik_cmd_send(test_ctx, repl, "recipient-uuid-abc \"Test message body\"");
    ck_assert(is_ok(&res));

    // Verify body
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           recipient->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_str_eq(inbox[0]->body, "Test message body");
}

END_TEST
// Test: non-existent recipient shows error
START_TEST(test_send_nonexistent_recipient) {
    res_t res = ik_cmd_send(test_ctx, repl, "nonexistent-uuid \"Message\"");
    ck_assert(is_ok(&res));

    // Verify error message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}

END_TEST
// Test: dead recipient shows "Recipient agent is dead" error
START_TEST(test_send_dead_recipient_error) {
    // Create and register recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "dead-recipient-uuid");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567894;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Mark recipient as dead
    res = ik_db_agent_mark_dead(db, recipient->uuid);
    ck_assert(is_ok(&res));

    // Attempt to send mail
    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    res = ik_cmd_send(test_ctx, repl, "dead-recipient-uuid \"Message\"");
    ck_assert(is_ok(&res));

    // Verify error message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);
}

END_TEST
// Test: dead recipient does NOT create mail record
START_TEST(test_send_dead_recipient_no_mail) {
    // Create and register recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "dead-recipient-uuid2");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567895;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Mark recipient as dead
    res = ik_db_agent_mark_dead(db, recipient->uuid);
    ck_assert(is_ok(&res));

    // Attempt to send mail
    res = ik_cmd_send(test_ctx, repl, "dead-recipient-uuid2 \"Message\"");
    ck_assert(is_ok(&res));

    // Verify no mail was created
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           recipient->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 0);
}

END_TEST
// Test: self-mail is allowed (sender == recipient)
START_TEST(test_send_self_mail_allowed) {
    // Send mail to self
    res_t res = ik_cmd_send(test_ctx, repl, "sender-uuid-123 \"Note to self\"");
    ck_assert(is_ok(&res));

    // Verify mail was created
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
    ck_assert_str_eq(inbox[0]->from_uuid, "sender-uuid-123");
    ck_assert_str_eq(inbox[0]->to_uuid, "sender-uuid-123");
}

END_TEST
// Test: empty body shows error
START_TEST(test_send_empty_body) {
    // Create recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "recipient-uuid-empty");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567896;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Attempt to send with empty body
    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    res = ik_cmd_send(test_ctx, repl, "recipient-uuid-empty \"\"");
    ck_assert(is_ok(&res));

    // Verify error message appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);
}

END_TEST
// Test: confirmation displayed
START_TEST(test_send_confirmation) {
    // Create recipient agent
    ik_agent_ctx_t *recipient = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(recipient);
    recipient->uuid = talloc_strdup(recipient, "recipient-uuid-conf");
    recipient->name = NULL;
    recipient->parent_uuid = NULL;
    recipient->created_at = 1234567897;
    recipient->fork_message_id = 0;

    // Add to agent array
    repl->agents[repl->agent_count++] = recipient;

    // Insert recipient into registry
    res_t res = ik_db_agent_insert(db, recipient);
    ck_assert(is_ok(&res));

    // Send mail
    size_t initial_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    res = ik_cmd_send(test_ctx, repl, "recipient-uuid-conf \"Message\"");
    ck_assert(is_ok(&res));

    // Verify confirmation appeared
    size_t final_lines = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(final_lines, initial_lines);
}

END_TEST

static Suite *send_suite(void)
{
    Suite *s = suite_create("Send Command");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, 30);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_send_creates_mail);
    tcase_add_test(tc, test_send_correct_uuids);
    tcase_add_test(tc, test_send_body_stored);
    tcase_add_test(tc, test_send_nonexistent_recipient);
    tcase_add_test(tc, test_send_dead_recipient_error);
    tcase_add_test(tc, test_send_dead_recipient_no_mail);
    tcase_add_test(tc, test_send_self_mail_allowed);
    tcase_add_test(tc, test_send_empty_body);
    tcase_add_test(tc, test_send_confirmation);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = send_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
