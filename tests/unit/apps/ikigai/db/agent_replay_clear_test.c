/**
 * @file agent_replay_clear_test.c
 * @brief Tests for agent replay clear mark detection
 *
 * Tests the find_clear function that locates clear marks in agent history.
 */

#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/agent_replay.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"
#include <check.h>
#include <string.h>
#include <time.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;
static ik_shared_ctx_t shared_ctx;

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

// Per-test setup: Connect and begin transaction
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
    res = ik_db_session_create(db, &session_id);
    if (is_err(&res)) {
        ik_test_db_rollback(db);
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
        return;
    }

    // Initialize minimal shared context with session_id
    shared_ctx.session_id = session_id;
}

// Per-test teardown: Rollback and disconnect
static void test_teardown(void)
{
    if (db != NULL) {
        ik_test_db_rollback(db);
    }
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    db = NULL;
}

// Skip macro for tests when DB not available
#define SKIP_IF_NO_DB() do { \
            if (!db_available) { \
                return; \
            } \
} while (0)

// Helper: Insert an agent into the registry
static void insert_agent(const char *uuid, const char *parent_uuid,
                         int64_t created_at, int64_t fork_message_id)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = parent_uuid ? talloc_strdup(test_ctx, parent_uuid) : NULL;
    agent.created_at = created_at;
    agent.fork_message_id = fork_message_id;
    agent.shared = &shared_ctx;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

// Helper: Insert a message
static void insert_message(const char *agent_uuid, const char *kind,
                           const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
}

// ========== find_clear Tests ==========

// Test: find_clear returns 0 when no clear exists
START_TEST(test_find_clear_no_clear) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("agent-no-clear", NULL, time(NULL), 0);

    // Insert some messages but no clear
    insert_message("agent-no-clear", "user", "Hello");
    insert_message("agent-no-clear", "assistant", "Hi");

    // Find clear - should return 0
    int64_t clear_id = -1;
    res_t res = ik_agent_find_clear(db, test_ctx, "agent-no-clear", 0, &clear_id);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(clear_id, 0);
}
END_TEST
// Test: find_clear returns correct message ID
START_TEST(test_find_clear_returns_id) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("agent-with-clear", NULL, time(NULL), 0);

    // Insert messages with a clear
    insert_message("agent-with-clear", "user", "Before clear");
    insert_message("agent-with-clear", "clear", NULL);
    insert_message("agent-with-clear", "user", "After clear");

    // Find clear - should return the clear's ID
    int64_t clear_id = 0;
    res_t res = ik_agent_find_clear(db, test_ctx, "agent-with-clear", 0, &clear_id);
    ck_assert(is_ok(&res));
    ck_assert(clear_id > 0);
}

END_TEST
// Test: find_clear respects max_id limit
START_TEST(test_find_clear_respects_max_id) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("agent-clear-limit", NULL, time(NULL), 0);

    // Insert messages: user, clear, user, clear, user
    insert_message("agent-clear-limit", "user", "First");
    insert_message("agent-clear-limit", "clear", NULL);  // This is the earlier clear
    insert_message("agent-clear-limit", "user", "Second");
    insert_message("agent-clear-limit", "clear", NULL);  // This is the later clear
    insert_message("agent-clear-limit", "user", "Third");

    // Find clear with no limit - should return later clear
    int64_t clear_id_all = 0;
    res_t res1 = ik_agent_find_clear(db, test_ctx, "agent-clear-limit", 0, &clear_id_all);
    ck_assert(is_ok(&res1));
    ck_assert(clear_id_all > 0);

    // Find clear with max_id = earlier clear id + 1
    // First get the earlier clear id by querying with a limit between them
    int64_t earlier_clear_id = 0;
    res_t res2 = ik_agent_find_clear(db, test_ctx, "agent-clear-limit", clear_id_all - 1, &earlier_clear_id);
    ck_assert(is_ok(&res2));

    // If earlier clear exists, it should be less than the later clear
    if (earlier_clear_id > 0) {
        ck_assert(earlier_clear_id < clear_id_all);
    }
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_replay_clear_suite(void)
{
    Suite *s = suite_create("Agent Replay Clear");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // find_clear tests
    tcase_add_test(tc_core, test_find_clear_no_clear);
    tcase_add_test(tc_core, test_find_clear_returns_id);
    tcase_add_test(tc_core, test_find_clear_respects_max_id);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_clear_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/agent_replay_clear_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
