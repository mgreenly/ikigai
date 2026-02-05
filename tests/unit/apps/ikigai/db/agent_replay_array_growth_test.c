/**
 * @file agent_replay_array_growth_test.c
 * @brief Tests for array growth and NULL field handling in agent_replay.c
 *
 * Covers:
 * - Line 112-117: Array growth in ik_agent_build_replay_ranges
 * - Line 326: Array growth in ik_agent_replay_history
 * - Line 348, 355: NULL content/data_json in ik_agent_replay_history
 */

#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/agent_replay.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
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
                           const char *content, const char *data_json)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind, content, data_json);
    ck_assert(is_ok(&res));
}

// ========== Array Growth Tests ==========

// Test: Array growth in ik_agent_build_replay_ranges (line 112-117)
// Create a hierarchy deep enough to trigger reallocation (initial capacity = 8)
START_TEST(test_build_ranges_array_growth) {
    SKIP_IF_NO_DB();

    // Create a 10-level hierarchy to exceed initial capacity of 8
    const char *agents[] = {
        "agent-0", "agent-1", "agent-2", "agent-3", "agent-4",
        "agent-5", "agent-6", "agent-7", "agent-8", "agent-9"
    };
    const size_t num_agents = sizeof(agents) / sizeof(agents[0]);

    // Insert root
    insert_agent(agents[0], NULL, 1000, 0);
    insert_message(agents[0], "user", "Message 0", "{}");

    // Insert chain of children
    for (size_t i = 1; i < num_agents; i++) {
        int64_t fork_id = 0;
        res_t res = ik_db_agent_get_last_message_id(db, agents[i - 1], &fork_id);
        ck_assert(is_ok(&res));

        insert_agent(agents[i], agents[i - 1], 1000 + (int64_t)i * 100, fork_id);
        insert_message(agents[i], "user", "Message", "{}");
    }

    // Build ranges for leaf - should trigger array growth
    ik_replay_range_t *ranges = NULL;
    size_t count = 0;
    res_t res = ik_agent_build_replay_ranges(db, test_ctx, agents[num_agents - 1], &ranges, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, (int)num_agents);
}
END_TEST
// Test: Array growth in ik_agent_replay_history (line 326)
// Create enough messages to trigger reallocation (initial capacity = 16)
START_TEST(test_replay_history_array_growth) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("many-msgs-agent", NULL, time(NULL), 0);

    // Insert 20 messages to exceed initial capacity of 16
    for (int32_t i = 0; i < 20; i++) {
        char content[64];
        snprintf(content, sizeof(content), "Message %d", i);
        insert_message("many-msgs-agent", "user", content, "{}");
    }

    // Replay history - should trigger array growth
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "many-msgs-agent", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);
    ck_assert_int_eq((int)ctx->count, 20);
}

END_TEST
// ========== NULL Field Tests ==========

// Test: ik_agent_replay_history with NULL content (line 348)
START_TEST(test_replay_history_null_content) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("null-content-agent", NULL, time(NULL), 0);

    // Insert messages - mark has NULL content field
    insert_message("null-content-agent", "mark", NULL, "{\"label\":\"test\"}");
    insert_message("null-content-agent", "user", "After mark", "{}");

    // Replay history
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "null-content-agent", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);
    ck_assert_int_eq((int)ctx->count, 2);

    // First message should have NULL content
    ck_assert_ptr_null(ctx->messages[0]->content);
    ck_assert_str_eq(ctx->messages[1]->content, "After mark");
}

END_TEST
// Test: ik_agent_replay_history with NULL data_json (line 355)
START_TEST(test_replay_history_null_data) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("null-data-agent", NULL, time(NULL), 0);

    // Insert message with NULL data_json
    insert_message("null-data-agent", "user", "User message", NULL);
    insert_message("null-data-agent", "assistant", "Assistant message", "{}");

    // Replay history
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "null-data-agent", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);
    ck_assert_int_eq((int)ctx->count, 2);

    // First message should have NULL data_json
    ck_assert_ptr_null(ctx->messages[0]->data_json);
    ck_assert_str_eq(ctx->messages[1]->data_json, "{}");
}

END_TEST
// Test: ik_agent_replay_history with both NULL content and data_json
START_TEST(test_replay_history_both_null) {
    SKIP_IF_NO_DB();

    // Insert agent
    insert_agent("both-null-agent", NULL, time(NULL), 0);

    // Insert mark message with content=NULL and data_json=NULL
    insert_message("both-null-agent", "mark", NULL, NULL);
    insert_message("both-null-agent", "user", "After", "{}");

    // Replay history
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "both-null-agent", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);
    ck_assert_int_eq((int)ctx->count, 2);

    // First message should have both fields NULL
    ck_assert_ptr_null(ctx->messages[0]->content);
    ck_assert_ptr_null(ctx->messages[0]->data_json);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_replay_array_growth_suite(void)
{
    Suite *s = suite_create("Agent Replay Array Growth");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // Array growth tests
    tcase_add_test(tc_core, test_build_ranges_array_growth);
    tcase_add_test(tc_core, test_replay_history_array_growth);

    // NULL field tests
    tcase_add_test(tc_core, test_replay_history_null_content);
    tcase_add_test(tc_core, test_replay_history_null_data);
    tcase_add_test(tc_core, test_replay_history_both_null);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_array_growth_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/agent_replay_array_growth_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
