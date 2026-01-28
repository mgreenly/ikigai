/**
 * @file agent_replay_test.c
 * @brief Tests for agent startup replay functionality
 *
 * Tests the "walk backwards, play forwards" algorithm for reconstructing
 * agent history from the database.
 */

#include "../../../src/db/agent.h"
#include "../../../src/db/agent_replay.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"
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

// ========== Full Replay Tests ==========

// Test: full replay produces correct chronological order
START_TEST(test_replay_chronological_order) {
    SKIP_IF_NO_DB();

    // Insert root agent with messages
    insert_agent("replay-root", NULL, 1000, 0);
    insert_message("replay-root", "user", "Root-1");
    insert_message("replay-root", "assistant", "Root-2");

    int64_t fork_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "replay-root", &fork_id);
    ck_assert(is_ok(&res));

    // Insert child
    insert_agent("replay-child", "replay-root", 2000, fork_id);
    insert_message("replay-child", "user", "Child-1");
    insert_message("replay-child", "assistant", "Child-2");

    // Replay child's history
    ik_replay_context_t *ctx = NULL;
    res = ik_agent_replay_history(db, test_ctx, "replay-child", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);

    // Should have 4 messages in chronological order
    ck_assert_int_eq((int)ctx->count, 4);
    ck_assert_str_eq(ctx->messages[0]->content, "Root-1");
    ck_assert_str_eq(ctx->messages[1]->content, "Root-2");
    ck_assert_str_eq(ctx->messages[2]->content, "Child-1");
    ck_assert_str_eq(ctx->messages[3]->content, "Child-2");
}
END_TEST
// Test: replay handles agent with no history
START_TEST(test_replay_empty_history) {
    SKIP_IF_NO_DB();

    // Insert agent with no messages
    insert_agent("empty-agent", NULL, time(NULL), 0);

    // Replay should succeed with empty context
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "empty-agent", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);
    ck_assert_int_eq((int)ctx->count, 0);
}

END_TEST
// Test: replay handles deep ancestry (4+ levels)
START_TEST(test_replay_deep_ancestry) {
    SKIP_IF_NO_DB();

    // Build 4-level hierarchy: great-grandparent -> grandparent -> parent -> child
    insert_agent("ggp", NULL, 1000, 0);
    insert_message("ggp", "user", "GGP");

    int64_t fork1 = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "ggp", &fork1);
    ck_assert(is_ok(&res));

    insert_agent("gp-deep", "ggp", 2000, fork1);
    insert_message("gp-deep", "user", "GP");

    int64_t fork2 = 0;
    res = ik_db_agent_get_last_message_id(db, "gp-deep", &fork2);
    ck_assert(is_ok(&res));

    insert_agent("p-deep", "gp-deep", 3000, fork2);
    insert_message("p-deep", "user", "P");

    int64_t fork3 = 0;
    res = ik_db_agent_get_last_message_id(db, "p-deep", &fork3);
    ck_assert(is_ok(&res));

    insert_agent("c-deep", "p-deep", 4000, fork3);
    insert_message("c-deep", "user", "C");

    // Replay child's history
    ik_replay_context_t *ctx = NULL;
    res = ik_agent_replay_history(db, test_ctx, "c-deep", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);

    // Should have 4 messages from all 4 levels
    ck_assert_int_eq((int)ctx->count, 4);
    ck_assert_str_eq(ctx->messages[0]->content, "GGP");
    ck_assert_str_eq(ctx->messages[1]->content, "GP");
    ck_assert_str_eq(ctx->messages[2]->content, "P");
    ck_assert_str_eq(ctx->messages[3]->content, "C");
}

END_TEST

// Test: interrupted turns are filtered out during replay
START_TEST(test_replay_filters_interrupted_turns) {
    SKIP_IF_NO_DB();

    // Insert root agent
    insert_agent("interrupted-root", NULL, 1000, 0);

    // Insert a complete turn
    insert_message("interrupted-root", "user", "First question");
    insert_message("interrupted-root", "assistant", "First answer");

    // Insert an interrupted turn (user -> partial assistant -> interrupted)
    insert_message("interrupted-root", "user", "Second question");
    insert_message("interrupted-root", "assistant", "Partial ans");
    insert_message("interrupted-root", "interrupted", NULL);

    // Insert another complete turn after interruption
    insert_message("interrupted-root", "user", "Third question");
    insert_message("interrupted-root", "assistant", "Third answer");

    // Replay history
    ik_replay_context_t *ctx = NULL;
    res_t res = ik_agent_replay_history(db, test_ctx, "interrupted-root", &ctx);
    ck_assert(is_ok(&res));
    ck_assert(ctx != NULL);

    // Should have 4 messages: first complete turn + third complete turn
    // The interrupted turn (Second question + Partial ans + interrupted) should be gone
    ck_assert_int_eq((int)ctx->count, 4);
    ck_assert_str_eq(ctx->messages[0]->content, "First question");
    ck_assert_str_eq(ctx->messages[1]->content, "First answer");
    ck_assert_str_eq(ctx->messages[2]->content, "Third question");
    ck_assert_str_eq(ctx->messages[3]->content, "Third answer");
}

END_TEST

// ========== Suite Configuration ==========

static Suite *agent_replay_suite(void)
{
    Suite *s = suite_create("Agent Replay");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Use unchecked fixture for suite-level setup/teardown
    tcase_add_unchecked_fixture(tc_core, suite_setup, suite_teardown);

    // Use checked fixture for per-test setup/teardown
    tcase_add_checked_fixture(tc_core, test_setup, test_teardown);

    // Full replay tests
    tcase_add_test(tc_core, test_replay_chronological_order);
    tcase_add_test(tc_core, test_replay_empty_history);
    tcase_add_test(tc_core, test_replay_deep_ancestry);
    tcase_add_test(tc_core, test_replay_filters_interrupted_turns);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_replay_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/agent_replay_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
