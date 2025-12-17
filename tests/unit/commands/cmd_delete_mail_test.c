/**
 * @file cmd_delete_mail_test.c
 * @brief Unit tests for /delete-mail command
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
    res = ik_db_agent_insert(db, agent);
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

// Test: delete-mail removes message from database
START_TEST(test_delete_mail_removes_message)
{
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

    // Create message
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                             repl->current->uuid,
                                             "Message to be deleted");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));
    int64_t mail_id = msg->id;

    // Verify message exists
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);

    // Delete the message using position 1 (first in inbox)
    (void)mail_id;  // Suppress unused variable warning
    res = cmd_delete_mail(test_ctx, repl, "1");
    ck_assert(is_ok(&res));

    // Verify message is gone
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 0);
}
END_TEST

// Test: confirmation message displayed
START_TEST(test_delete_mail_shows_confirmation)
{
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

    // Create message
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                             repl->current->uuid,
                                             "Another message");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Delete the message using position 1
    res = cmd_delete_mail(test_ctx, repl, "1");
    ck_assert(is_ok(&res));

    // Verify confirmation in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: non-existent ID shows error
START_TEST(test_delete_mail_nonexistent_id)
{
    // Try to delete non-existent message
    res_t res = cmd_delete_mail(test_ctx, repl, "999999");
    ck_assert(is_ok(&res));

    // Verify error message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: ID from different agent (not recipient) shows error
START_TEST(test_delete_mail_different_agent)
{
    // Create sender and another recipient
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

    ik_agent_ctx_t *other = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(other);
    other->uuid = talloc_strdup(other, "other-uuid-jkl");
    other->name = NULL;
    other->parent_uuid = NULL;
    other->created_at = 1234567894;
    other->fork_message_id = 0;
    repl->agents[repl->agent_count++] = other;

    res = ik_db_agent_insert(db, other);
    ck_assert(is_ok(&res));

    // Send message to other agent
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                             other->uuid,
                                             "Message for other agent");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Try to delete it from current agent using position 1 (should fail - not in current agent's inbox)
    res = cmd_delete_mail(test_ctx, repl, "1");
    ck_assert(is_ok(&res));

    // Verify error message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Verify message still exists for the correct recipient
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           other->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
}
END_TEST

// Test: deleted mail no longer appears in /check-mail
START_TEST(test_delete_mail_not_in_check_mail)
{
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

    // Create two messages
    ik_mail_msg_t *msg1 = ik_mail_msg_create(test_ctx, sender->uuid,
                                              repl->current->uuid,
                                              "First message");
    ck_assert_ptr_nonnull(msg1);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg1);
    ck_assert(is_ok(&res));

    ik_mail_msg_t *msg2 = ik_mail_msg_create(test_ctx, sender->uuid,
                                              repl->current->uuid,
                                              "Second message");
    ck_assert_ptr_nonnull(msg2);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg2);
    ck_assert(is_ok(&res));

    // Verify both messages appear in inbox
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 2);

    // Delete position 1 (newest message)
    res = cmd_delete_mail(test_ctx, repl, "1");
    ck_assert(is_ok(&res));

    // Verify only one message remains
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);
}
END_TEST

// Test: deleted mail cannot be read
START_TEST(test_delete_mail_cannot_read_after)
{
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

    // Create message
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                             repl->current->uuid,
                                             "Message to delete and then try to read");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg);
    ck_assert(is_ok(&res));

    // Delete the message using position 1
    res = cmd_delete_mail(test_ctx, repl, "1");
    ck_assert(is_ok(&res));

    // Clear scrollback to check for read error
    ik_scrollback_clear(repl->current->scrollback);

    // Try to read deleted message (should fail)
    res = cmd_read_mail(test_ctx, repl, "1");
    ck_assert(is_ok(&res));

    // Verify error appears in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: delete by position with multiple messages
START_TEST(test_delete_by_position_multi_message)
{
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(sender);
    sender->uuid = talloc_strdup(sender, "sender-uuid-xyz");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567897;
    sender->fork_message_id = 0;
    repl->agents[repl->agent_count++] = sender;

    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Create first message
    ik_mail_msg_t *msg1 = ik_mail_msg_create(test_ctx, sender->uuid,
                                              repl->current->uuid,
                                              "First message");
    ck_assert_ptr_nonnull(msg1);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg1);
    ck_assert(is_ok(&res));

    // Create second message
    ik_mail_msg_t *msg2 = ik_mail_msg_create(test_ctx, sender->uuid,
                                              repl->current->uuid,
                                              "Second message");
    ck_assert_ptr_nonnull(msg2);
    res = ik_db_mail_insert(db, repl->shared->session_id, msg2);
    ck_assert(is_ok(&res));

    // Verify inbox has 2 messages
    ik_mail_msg_t **inbox = NULL;
    size_t count = 0;
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 2);

    // Get the message at position 2 before deletion
    const char *position_2_body = talloc_strdup(test_ctx, inbox[1]->body);

    // Delete position 2
    res = cmd_delete_mail(test_ctx, repl, "2");
    ck_assert(is_ok(&res));

    // Verify only one message remains
    res = ik_db_mail_inbox(db, test_ctx, repl->shared->session_id,
                           repl->current->uuid, &inbox, &count);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(count, 1);

    // Verify the remaining message is NOT the one we deleted
    ck_assert_str_ne(inbox[0]->body, position_2_body);
}
END_TEST

static Suite *delete_mail_suite(void)
{
    Suite *s = suite_create("Delete Mail Command");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_delete_mail_removes_message);
    tcase_add_test(tc, test_delete_mail_shows_confirmation);
    tcase_add_test(tc, test_delete_mail_nonexistent_id);
    tcase_add_test(tc, test_delete_mail_different_agent);
    tcase_add_test(tc, test_delete_mail_not_in_check_mail);
    tcase_add_test(tc, test_delete_mail_cannot_read_after);
    tcase_add_test(tc, test_delete_by_position_multi_message);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = delete_mail_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (failed == 0) ? 0 : 1;
}
