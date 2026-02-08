/**
 * @file agent_restore_replay_toolset_fork_test.c
 * @brief Tests for agent restore toolset replay via fork messages
 */

#include "apps/ikigai/repl/agent_restore_replay_toolset.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include "shared/error.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

static const char *DB_NAME;
static bool db_available = false;
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;
static ik_shared_ctx_t shared_ctx;

// Suite-level setup
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
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

// Suite-level teardown
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
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

// Per-test teardown
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

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// Helper: Create agent
static ik_agent_ctx_t *create_test_agent(const char *uuid)
{
    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    shared->db_ctx = db;
    shared->session_id = session_id;
    shared->cfg = ik_test_create_config(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(test_ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    if (uuid != NULL) {
        talloc_free(agent->uuid);
        agent->uuid = talloc_strdup(agent, uuid);
    }

    return agent;
}

// Helper: Insert agent
static void insert_agent(const char *uuid)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = NULL;
    agent.created_at = 1000;
    agent.fork_message_id = 0;
    agent.shared = &shared_ctx;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

// Helper: Insert message
static void insert_message(const char *agent_uuid, const char *kind,
                           const char *data_json)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind,
                                     NULL, data_json);
    ck_assert(is_ok(&res));
}

// Test: fork inheritance with toolset_filter (tests lines 186, 191, 197)
START_TEST(test_toolset_replay_fork_inherit) {
    SKIP_IF_NO_DB();

    const char *parent_uuid = "parent-agent";
    const char *child_uuid = "child-agent";

    // Create parent and child agents
    insert_agent(parent_uuid);
    insert_agent(child_uuid);

    // Create fork message with toolset_filter
    const char *fork_data = "{\"parent_uuid\":\"" "parent-agent" "\",\"toolset_filter\":[\"tool1\",\"tool2\"]}";
    insert_message(child_uuid, "fork", fork_data);

    ik_agent_ctx_t *agent = create_test_agent(child_uuid);
    agent->parent_uuid = talloc_strdup(agent, parent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Should inherit toolset from fork message
    ck_assert_int_eq((int)agent->toolset_count, 2);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
}
END_TEST

// Test: fork with non-array toolset_filter (tests line 194)
START_TEST(test_toolset_replay_fork_non_array) {
    SKIP_IF_NO_DB();

    const char *parent_uuid = "parent-non-array";
    const char *child_uuid = "child-non-array";

    insert_agent(parent_uuid);
    insert_agent(child_uuid);

    // Fork message with toolset_filter as string (not array)
    const char *fork_data = "{\"parent_uuid\":\"" "parent-non-array" "\",\"toolset_filter\":\"not_an_array\"}";
    insert_message(child_uuid, "fork", fork_data);

    ik_agent_ctx_t *agent = create_test_agent(child_uuid);
    agent->parent_uuid = talloc_strdup(agent, parent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty (non-array ignored)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: fork with empty array (tests line 85)
START_TEST(test_toolset_replay_fork_empty_array) {
    SKIP_IF_NO_DB();

    const char *parent_uuid = "parent-empty";
    const char *child_uuid = "child-empty";

    insert_agent(parent_uuid);
    insert_agent(child_uuid);

    // Fork message with empty toolset_filter array
    const char *fork_data = "{\"parent_uuid\":\"" "parent-empty" "\",\"toolset_filter\":[]}";
    insert_message(child_uuid, "fork", fork_data);

    ik_agent_ctx_t *agent = create_test_agent(child_uuid);
    agent->parent_uuid = talloc_strdup(agent, parent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty (empty array)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: fork with non-string elements (tests line 96)
START_TEST(test_toolset_replay_fork_non_string_elements) {
    SKIP_IF_NO_DB();

    const char *parent_uuid = "parent-nonstr";
    const char *child_uuid = "child-nonstr";

    insert_agent(parent_uuid);
    insert_agent(child_uuid);

    // Fork message with mixed types in array
    const char *fork_data = "{\"parent_uuid\":\"" "parent-nonstr" "\",\"toolset_filter\":[\"tool1\",123,\"tool2\",null,\"tool3\"]}";
    insert_message(child_uuid, "fork", fork_data);

    ik_agent_ctx_t *agent = create_test_agent(child_uuid);
    agent->parent_uuid = talloc_strdup(agent, parent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Should only include string elements
    ck_assert_int_eq((int)agent->toolset_count, 3);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
    ck_assert_str_eq(agent->toolset_filter[2], "tool3");
}
END_TEST

// Test: fork replaces existing toolset_filter (tests lines 76-79)
START_TEST(test_toolset_replay_fork_replaces_existing) {
    SKIP_IF_NO_DB();

    const char *parent_uuid = "parent-replace";
    const char *child_uuid = "child-replace";

    insert_agent(parent_uuid);
    insert_agent(child_uuid);

    // Fork message with new toolset
    const char *fork_data = "{\"parent_uuid\":\"" "parent-replace" "\",\"toolset_filter\":[\"new1\",\"new2\"]}";
    insert_message(child_uuid, "fork", fork_data);

    ik_agent_ctx_t *agent = create_test_agent(child_uuid);
    agent->parent_uuid = talloc_strdup(agent, parent_uuid);

    // Set existing toolset filter
    agent->toolset_count = 2;
    agent->toolset_filter = talloc_array(agent, char *, 2);
    agent->toolset_filter[0] = talloc_strdup(agent, "old1");
    agent->toolset_filter[1] = talloc_strdup(agent, "old2");

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Old filter should be replaced
    ck_assert_int_eq((int)agent->toolset_count, 2);
    ck_assert_str_eq(agent->toolset_filter[0], "new1");
    ck_assert_str_eq(agent->toolset_filter[1], "new2");
}
END_TEST

// Test: agent with parent but no fork message (tests line 186 false branch)
START_TEST(test_toolset_replay_no_fork_message) {
    SKIP_IF_NO_DB();

    const char *parent_uuid = "parent-nofork";
    const char *child_uuid = "child-nofork";

    insert_agent(parent_uuid);
    insert_agent(child_uuid);

    // No fork message inserted for child

    ik_agent_ctx_t *agent = create_test_agent(child_uuid);
    agent->parent_uuid = talloc_strdup(agent, parent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty (no fork message)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

static Suite *agent_restore_replay_toolset_fork_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay - Toolset Fork");

    TCase *tc = tcase_create("Fork Replay");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);

    tcase_add_test(tc, test_toolset_replay_fork_inherit);
    tcase_add_test(tc, test_toolset_replay_fork_non_array);
    tcase_add_test(tc, test_toolset_replay_fork_empty_array);
    tcase_add_test(tc, test_toolset_replay_fork_non_string_elements);
    tcase_add_test(tc, test_toolset_replay_fork_replaces_existing);
    tcase_add_test(tc, test_toolset_replay_no_fork_message);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_toolset_fork_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/agent_restore_replay_toolset_fork_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
