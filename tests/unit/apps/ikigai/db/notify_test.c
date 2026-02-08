/**
 * @file notify_test.c
 * @brief Tests for PostgreSQL LISTEN/NOTIFY infrastructure
 *
 * Covers all 4 functions in apps/ikigai/db/notify.c:
 * - ik_db_listen
 * - ik_db_notify
 * - ik_db_socket_fd
 * - ik_db_consume_notifications (with callback round-trip)
 */

#include "apps/ikigai/db/notify.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"
#include "tests/test_constants.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <string.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;

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

// Per-test setup: Connect (no transaction - LISTEN/NOTIFY requires autocommit)
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
    }
}

// Per-test teardown: Cleanup connection
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Notification Callback ==========

typedef struct {
    char channel[256];
    char payload[256];
    int count;
} notify_test_ctx_t;

static void test_notify_callback(void *ctx, const char *channel, const char *payload)
{
    notify_test_ctx_t *test = (notify_test_ctx_t *)ctx;
    if (channel != NULL) {
        strncpy(test->channel, channel, sizeof(test->channel) - 1);
        test->channel[sizeof(test->channel) - 1] = '\0';
    }
    if (payload != NULL) {
        strncpy(test->payload, payload, sizeof(test->payload) - 1);
        test->payload[sizeof(test->payload) - 1] = '\0';
    }
    test->count++;
}

// ========== Tests ==========

// Test: ik_db_listen succeeds
START_TEST(test_listen_succeeds) {
    SKIP_IF_NO_DB();

    res_t res = ik_db_listen(db, "test_channel");
    ck_assert(is_ok(&res));
}
END_TEST

// Test: ik_db_notify succeeds
START_TEST(test_notify_succeeds) {
    SKIP_IF_NO_DB();

    res_t res = ik_db_notify(db, "test_channel_n", "hello");
    ck_assert(is_ok(&res));
}
END_TEST

// Test: ik_db_socket_fd returns valid fd
START_TEST(test_socket_fd_valid) {
    SKIP_IF_NO_DB();

    int32_t fd = ik_db_socket_fd(db);
    ck_assert_int_ge(fd, 0);
}
END_TEST

// Test: round-trip listen, notify, consume
START_TEST(test_notify_round_trip) {
    SKIP_IF_NO_DB();

    // Listen on channel
    res_t res = ik_db_listen(db, "test_rt_channel");
    ck_assert(is_ok(&res));

    // Send notification
    res = ik_db_notify(db, "test_rt_channel", "test_payload");
    ck_assert(is_ok(&res));

    // Consume notifications
    notify_test_ctx_t nctx = {.channel = {0}, .payload = {0}, .count = 0};
    res = ik_db_consume_notifications(db, test_notify_callback, &nctx);
    ck_assert(is_ok(&res));

    // Verify callback was invoked
    ck_assert_int_eq(nctx.count, 1);
    ck_assert_str_eq(nctx.channel, "test_rt_channel");
    ck_assert_str_eq(nctx.payload, "test_payload");
}
END_TEST

// Test: consume with no pending notifications returns 0 count
START_TEST(test_consume_no_notifications) {
    SKIP_IF_NO_DB();

    notify_test_ctx_t nctx = {.channel = {0}, .payload = {0}, .count = 0};
    res_t res = ik_db_consume_notifications(db, test_notify_callback, &nctx);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(nctx.count, 0);
}
END_TEST

// Test: multiple notifications consumed
START_TEST(test_multiple_notifications) {
    SKIP_IF_NO_DB();

    res_t res = ik_db_listen(db, "test_multi_ch");
    ck_assert(is_ok(&res));

    // Send two notifications
    res = ik_db_notify(db, "test_multi_ch", "payload_1");
    ck_assert(is_ok(&res));
    res = ik_db_notify(db, "test_multi_ch", "payload_2");
    ck_assert(is_ok(&res));

    // Consume all
    notify_test_ctx_t nctx = {.channel = {0}, .payload = {0}, .count = 0};
    res = ik_db_consume_notifications(db, test_notify_callback, &nctx);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(nctx.count, 2);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *notify_suite(void)
{
    Suite *s = suite_create("DB Notify");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);

    tcase_add_test(tc, test_listen_succeeds);
    tcase_add_test(tc, test_notify_succeeds);
    tcase_add_test(tc, test_socket_fd_valid);
    tcase_add_test(tc, test_notify_round_trip);
    tcase_add_test(tc, test_consume_no_notifications);
    tcase_add_test(tc, test_multiple_notifications);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = notify_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/notify_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
