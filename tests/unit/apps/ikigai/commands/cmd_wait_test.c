/**
 * @file cmd_wait_test.c
 * @brief Unit tests for /wait command
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/commands_wait_core.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/mail.h"
#include "apps/ikigai/db/session.h"
#include "shared/error.h"
#include "apps/ikigai/mail/msg.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"
#include "apps/ikigai/wrapper_pthread.h"

#include <check.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

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
static ik_db_ctx_t *worker_db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static int64_t session_id;

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

    agent->uuid = talloc_strdup(agent, "waiter-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    agent->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_complete = false;
    agent->tool_thread_running = false;
    agent->interrupt_requested = false;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    shared->worker_db_ctx = worker_db;
    shared->session_id = session_id;
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Insert agent into registry
    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup agent in registry");
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

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

static void test_setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        fprintf(stderr, "DB connect failed: %s\n", error_message(res.err));
        ck_abort_msg("Failed to connect to test database");
    }

    res = ik_test_db_connect(test_ctx, DB_NAME, &worker_db);
    if (is_err(&res)) {
        fprintf(stderr, "Worker DB connect failed: %s\n", error_message(res.err));
        ck_abort_msg("Failed to connect to test database");
    }

    res = ik_test_db_begin(db);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to begin transaction: %s\n", error_message(res.err));
        ck_abort_msg("Begin transaction failed");
    }

    session_id = 0;
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to create session: %s\n", error_message(res.err));
        ck_abort_msg("Session creation failed");
    }

    setup_repl();
}

static void test_teardown(void)
{
    if (repl != NULL && repl->current != NULL) {
        pthread_mutex_destroy(&repl->current->tool_thread_mutex);
    }

    if (db != NULL && test_ctx != NULL) {
        ik_test_db_rollback(db);
    }

    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;
    worker_db = NULL;
    repl = NULL;
}

// Test: /wait with no arguments shows usage
START_TEST(test_wait_no_args)
{
    res_t res = ik_cmd_wait(test_ctx, repl, "");
    ck_assert(is_ok(&res));

    // Verify usage message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: /wait with invalid timeout shows error
START_TEST(test_wait_invalid_timeout)
{
    res_t res = ik_cmd_wait(test_ctx, repl, "abc");
    ck_assert(is_ok(&res));

    // Verify error message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: /wait with negative timeout shows error
START_TEST(test_wait_negative_timeout)
{
    res_t res = ik_cmd_wait(test_ctx, repl, "-5");
    ck_assert(is_ok(&res));

    // Verify error message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: /wait with no DB returns error
START_TEST(test_wait_no_db)
{
    repl->shared->db_ctx = NULL;

    res_t res = ik_cmd_wait(test_ctx, repl, "5");
    ck_assert(is_ok(&res));

    // Verify error message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: /wait timeout=0 with no messages returns timeout
START_TEST(test_wait_instant_no_messages)
{
    res_t res = ik_cmd_wait(test_ctx, repl, "0");
    ck_assert(is_ok(&res));

    // Verify thread was spawned
    ck_assert(repl->current->tool_thread_running);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);

    // Wait for worker thread to complete
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        bool complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }

    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    ck_assert(repl->current->tool_thread_complete);
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    pthread_join_(repl->current->tool_thread, NULL);
    repl->current->tool_thread_running = false;

    // Invoke on_complete callback to render results
    if (repl->current->pending_on_complete != NULL) {
        repl->current->pending_on_complete(repl, repl->current);
    }

    // Verify result message in scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: /wait receives message that was already in inbox
START_TEST(test_wait_instant_with_message)
{
    // Create sender agent
    ik_agent_ctx_t *sender = talloc_zero(repl, ik_agent_ctx_t);
    sender->uuid = talloc_strdup(sender, "sender-uuid-456");
    sender->name = NULL;
    sender->parent_uuid = NULL;
    sender->created_at = 1234567890;
    sender->fork_message_id = 0;
    sender->shared = repl->shared;
    res_t res = ik_db_agent_insert(db, sender);
    ck_assert(is_ok(&res));

    // Insert a mail message
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, sender->uuid,
                                             repl->current->uuid, "Test message");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, session_id, msg);
    ck_assert(is_ok(&res));

    res = ik_cmd_wait(test_ctx, repl, "0");
    ck_assert(is_ok(&res));

    // Wait for worker thread to complete
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        bool complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }

    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    ck_assert(repl->current->tool_thread_complete);
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    pthread_join_(repl->current->tool_thread, NULL);
    repl->current->tool_thread_running = false;

    // Invoke on_complete callback to render results
    if (repl->current->pending_on_complete != NULL) {
        repl->current->pending_on_complete(repl, repl->current);
    }

    // Verify message was rendered to scrollback
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 1);
}
END_TEST

// Test: /wait fan-in mode with multiple targets
START_TEST(test_wait_fanin_mode)
{
    // Create two target agents
    ik_agent_ctx_t *target1 = talloc_zero(repl, ik_agent_ctx_t);
    target1->uuid = talloc_strdup(target1, "target1-uuid");
    target1->name = NULL;
    target1->parent_uuid = NULL;
    target1->created_at = 1234567890;
    target1->fork_message_id = 0;
    target1->shared = repl->shared;
    res_t res = ik_db_agent_insert(db, target1);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *target2 = talloc_zero(repl, ik_agent_ctx_t);
    target2->uuid = talloc_strdup(target2, "target2-uuid");
    target2->name = NULL;
    target2->parent_uuid = NULL;
    target2->created_at = 1234567890;
    target2->fork_message_id = 0;
    target2->shared = repl->shared;
    res = ik_db_agent_insert(db, target2);
    ck_assert(is_ok(&res));

    // Mark target1 as idle
    res = ik_db_agent_set_idle(db, target1->uuid, true);
    ck_assert(is_ok(&res));

    // Send a message from target2
    ik_mail_msg_t *msg = ik_mail_msg_create(test_ctx, target2->uuid,
                                             repl->current->uuid, "Fan-in message");
    ck_assert_ptr_nonnull(msg);
    res = ik_db_mail_insert(db, session_id, msg);
    ck_assert(is_ok(&res));

    res = ik_cmd_wait(test_ctx, repl, "0 target1-uuid target2-uuid");
    ck_assert(is_ok(&res));

    // Wait for worker thread to complete
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        bool complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }

    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    ck_assert(repl->current->tool_thread_complete);
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    pthread_join_(repl->current->tool_thread, NULL);
    repl->current->tool_thread_running = false;

    // Invoke on_complete callback to render results
    if (repl->current->pending_on_complete != NULL) {
        repl->current->pending_on_complete(repl, repl->current);
    }

    // Verify fan-in results in scrollback (header + 2 targets)
    ck_assert_uint_ge(ik_scrollback_get_line_count(repl->current->scrollback), 3);
}
END_TEST

static Suite *cmd_wait_suite(void)
{
    Suite *s = suite_create("cmd_wait");

    TCase *tc_basic = tcase_create("basic");
    tcase_add_unchecked_fixture(tc_basic, test_setup, test_teardown);
    tcase_add_test(tc_basic, test_wait_no_args);
    tcase_add_test(tc_basic, test_wait_invalid_timeout);
    tcase_add_test(tc_basic, test_wait_negative_timeout);
    tcase_add_test(tc_basic, test_wait_no_db);
    suite_add_tcase(s, tc_basic);

    TCase *tc_next = tcase_create("next_message");
    tcase_add_unchecked_fixture(tc_next, test_setup, test_teardown);
    tcase_add_test(tc_next, test_wait_instant_no_messages);
    tcase_add_test(tc_next, test_wait_instant_with_message);
    suite_add_tcase(s, tc_next);

    TCase *tc_fanin = tcase_create("fanin");
    tcase_add_unchecked_fixture(tc_fanin, test_setup, test_teardown);
    tcase_add_test(tc_fanin, test_wait_fanin_mode);
    suite_add_tcase(s, tc_fanin);

    return s;
}

int main(void)
{
    if (!suite_setup()) {
        return EXIT_FAILURE;
    }

    Suite *s = cmd_wait_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_wait_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
