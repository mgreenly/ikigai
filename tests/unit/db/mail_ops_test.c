#include "../../../src/db/mail.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/mail/msg.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

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

    // Create a session for mail tests
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

// Test: Insert creates record
START_TEST(test_db_mail_insert_creates_record)
{
    SKIP_IF_NO_DB();

    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Hello!");
    ck_assert(msg != NULL);

    res_t res = ik_db_mail_insert(db, session_id, msg);
    ck_assert(is_ok(&res));

    // Verify record was created
    const char *query = "SELECT COUNT(*) FROM mail WHERE session_id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)session_id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);

    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "1");

    PQclear(result);
}
END_TEST

// Test: Insert sets msg->id
START_TEST(test_db_mail_insert_sets_msg_id)
{
    SKIP_IF_NO_DB();

    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Hello!");
    ck_assert(msg != NULL);
    ck_assert_int_eq(msg->id, 0);

    res_t res = ik_db_mail_insert(db, session_id, msg);
    ck_assert(is_ok(&res));

    // msg->id should be set to the database ID
    ck_assert(msg->id > 0);
}
END_TEST

// Test: Inbox returns messages for recipient only
START_TEST(test_db_mail_inbox_filters_by_recipient)
{
    SKIP_IF_NO_DB();

    // Insert messages to different recipients
    ik_mail_msg_t *msg1 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Message for agent-2");
    ik_mail_msg_t *msg2 = ik_mail_msg_create(test_ctx, "agent-1", "agent-3", "Message for agent-3");
    ik_mail_msg_t *msg3 = ik_mail_msg_create(test_ctx, "agent-2", "agent-2", "Another for agent-2");

    ik_db_mail_insert(db, session_id, msg1);
    ik_db_mail_insert(db, session_id, msg2);
    ik_db_mail_insert(db, session_id, msg3);

    // Query inbox for agent-2
    ik_mail_msg_t **inbox;
    size_t count;
    res_t res = ik_db_mail_inbox(db, test_ctx, session_id, "agent-2", &inbox, &count);
    ck_assert(is_ok(&res));

    // Should only get messages for agent-2
    ck_assert_int_eq((int)count, 2);
    ck_assert_str_eq(inbox[0]->to_uuid, "agent-2");
    ck_assert_str_eq(inbox[1]->to_uuid, "agent-2");
}
END_TEST

// Test: Inbox orders unread first
START_TEST(test_db_mail_inbox_orders_unread_first)
{
    SKIP_IF_NO_DB();

    // Insert messages
    ik_mail_msg_t *msg1 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "First");
    ik_mail_msg_t *msg2 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Second");
    ik_mail_msg_t *msg3 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Third");

    ik_db_mail_insert(db, session_id, msg1);
    ik_db_mail_insert(db, session_id, msg2);
    ik_db_mail_insert(db, session_id, msg3);

    // Mark first message as read
    ik_db_mail_mark_read(db, msg1->id);

    // Query inbox
    ik_mail_msg_t **inbox;
    size_t count;
    res_t res = ik_db_mail_inbox(db, test_ctx, session_id, "agent-2", &inbox, &count);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)count, 3);

    // Unread messages should come first
    ck_assert(!inbox[0]->read);  // Second or Third
    ck_assert(!inbox[1]->read);  // Second or Third
    ck_assert(inbox[2]->read);   // First (marked as read)
}
END_TEST

// Test: Inbox orders by timestamp desc
START_TEST(test_db_mail_inbox_orders_by_timestamp_desc)
{
    SKIP_IF_NO_DB();

    // Insert messages with different timestamps
    ik_mail_msg_t *msg1 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Old");
    msg1->timestamp = 1000;
    ik_mail_msg_t *msg2 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Middle");
    msg2->timestamp = 2000;
    ik_mail_msg_t *msg3 = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Recent");
    msg3->timestamp = 3000;

    ik_db_mail_insert(db, session_id, msg1);
    ik_db_mail_insert(db, session_id, msg2);
    ik_db_mail_insert(db, session_id, msg3);

    // Query inbox
    ik_mail_msg_t **inbox;
    size_t count;
    res_t res = ik_db_mail_inbox(db, test_ctx, session_id, "agent-2", &inbox, &count);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)count, 3);

    // Should be ordered by timestamp descending (newest first)
    ck_assert_int_eq(inbox[0]->timestamp, 3000);  // Recent
    ck_assert_int_eq(inbox[1]->timestamp, 2000);  // Middle
    ck_assert_int_eq(inbox[2]->timestamp, 1000);  // Old
}
END_TEST

// Test: Mark read updates read flag
START_TEST(test_db_mail_mark_read_updates_flag)
{
    SKIP_IF_NO_DB();

    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, "agent-1", "agent-2", "Test");
    ik_db_mail_insert(db, session_id, msg);

    // Verify initially unread
    const char *query = "SELECT read FROM mail WHERE id = $1";
    const char *params[] = {talloc_asprintf(test_ctx, "%lld", (long long)msg->id)};
    PGresult *result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "0");
    PQclear(result);

    // Mark as read
    res_t res = ik_db_mail_mark_read(db, msg->id);
    ck_assert(is_ok(&res));

    // Verify now read
    result = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "1");
    PQclear(result);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *mail_ops_suite(void)
{
    Suite *s = suite_create("Mail Operations");

    TCase *tc_insert = tcase_create("Insert");
    tcase_add_unchecked_fixture(tc_insert, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_insert, test_setup, test_teardown);
    tcase_add_test(tc_insert, test_db_mail_insert_creates_record);
    tcase_add_test(tc_insert, test_db_mail_insert_sets_msg_id);
    suite_add_tcase(s, tc_insert);

    TCase *tc_inbox = tcase_create("Inbox");
    tcase_add_unchecked_fixture(tc_inbox, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_inbox, test_setup, test_teardown);
    tcase_add_test(tc_inbox, test_db_mail_inbox_filters_by_recipient);
    tcase_add_test(tc_inbox, test_db_mail_inbox_orders_unread_first);
    tcase_add_test(tc_inbox, test_db_mail_inbox_orders_by_timestamp_desc);
    suite_add_tcase(s, tc_inbox);

    TCase *tc_mark = tcase_create("Mark Read");
    tcase_add_unchecked_fixture(tc_mark, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_mark, test_setup, test_teardown);
    tcase_add_test(tc_mark, test_db_mail_mark_read_updates_flag);
    suite_add_tcase(s, tc_mark);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = mail_ops_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
