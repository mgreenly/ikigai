/**
 * @file agent_replay_query_test.c
 * @brief Tests for agent replay message querying
 *
 * Tests the query_range function that retrieves messages for a given range.
 */

#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/agent_replay.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/replay.h"
#include "apps/ikigai/db/session.h"
#include "shared/error.h"
#include "apps/ikigai/msg.h"
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
    }
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

// ========== query_range Tests ==========

// Test: query_range returns correct message subset
START_TEST(test_query_range_subset) {
    SKIP_IF_NO_DB();

    // Insert agent with multiple messages
    insert_agent("query-test-agent", NULL, time(NULL), 0);
    insert_message("query-test-agent", "user", "Msg 1");
    insert_message("query-test-agent", "assistant", "Msg 2");
    insert_message("query-test-agent", "user", "Msg 3");
    insert_message("query-test-agent", "assistant", "Msg 4");

    // Get message IDs
    int64_t last_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "query-test-agent", &last_id);
    ck_assert(is_ok(&res));

    // Query all messages (start_id=0, end_id=0)
    ik_replay_range_t range = {
        .agent_uuid = talloc_strdup(test_ctx, "query-test-agent"),
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t msg_count = 0;
    res = ik_agent_query_range(db, test_ctx, &range, &messages, &msg_count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)msg_count, 4);
}
END_TEST
// Test: query_range with start_id=0 returns from beginning
START_TEST(test_query_range_from_beginning) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("query-begin-agent", NULL, time(NULL), 0);
    insert_message("query-begin-agent", "user", "First");
    insert_message("query-begin-agent", "assistant", "Second");

    // Query from beginning (start_id=0)
    ik_replay_range_t range = {
        .agent_uuid = talloc_strdup(test_ctx, "query-begin-agent"),
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t msg_count = 0;
    res_t res = ik_agent_query_range(db, test_ctx, &range, &messages, &msg_count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)msg_count, 2);

    // Verify first message
    ck_assert_str_eq(messages[0]->content, "First");
}

END_TEST
// Test: query_range with end_id=0 returns to end
START_TEST(test_query_range_to_end) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("query-end-agent", NULL, time(NULL), 0);
    insert_message("query-end-agent", "user", "One");
    insert_message("query-end-agent", "assistant", "Two");
    insert_message("query-end-agent", "user", "Three");

    // Get first message ID
    ik_replay_range_t range_all = {
        .agent_uuid = talloc_strdup(test_ctx, "query-end-agent"),
        .start_id = 0,
        .end_id = 0
    };

    ik_msg_t **all_msgs = NULL;
    size_t all_count = 0;
    res_t res = ik_agent_query_range(db, test_ctx, &range_all, &all_msgs, &all_count);
    ck_assert(is_ok(&res));
    ck_assert(all_count >= 3);

    // Query starting after first message with end_id=0 (to end)
    int64_t first_id = all_msgs[0]->id;
    ik_replay_range_t range = {
        .agent_uuid = talloc_strdup(test_ctx, "query-end-agent"),
        .start_id = first_id,
        .end_id = 0
    };

    ik_msg_t **messages = NULL;
    size_t msg_count = 0;
    res = ik_agent_query_range(db, test_ctx, &range, &messages, &msg_count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)msg_count, 2);  // Two and Three

    // Verify messages
    ck_assert_str_eq(messages[0]->content, "Two");
    ck_assert_str_eq(messages[1]->content, "Three");
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_replay_query_suite(void)
{
    Suite *s = suite_create("Agent Replay Query");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // query_range tests
    tcase_add_test(tc_core, test_query_range_subset);
    tcase_add_test(tc_core, test_query_range_from_beginning);
    tcase_add_test(tc_core, test_query_range_to_end);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_query_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/agent_replay_query_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
