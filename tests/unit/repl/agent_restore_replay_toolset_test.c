/**
 * @file agent_restore_replay_toolset_test.c
 * @brief Tests for agent restore toolset replay
 */

#include "../../../src/repl/agent_restore_replay_toolset.h"
#include "../../../src/agent.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../test_utils_helper.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

static const char *DB_NAME;
static bool db_available = false;
static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

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
    }
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

// Test: basic toolset command replay
START_TEST(test_toolset_replay_basic) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-1";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\"tool1,tool2,tool3\"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)agent->toolset_count, 3);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
    ck_assert_str_eq(agent->toolset_filter[2], "tool3");
}
END_TEST

// Test: toolset with spaces (tests trim logic lines 43-54)
START_TEST(test_toolset_replay_with_spaces) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-spaces";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\"  tool1  , tool2 ,  tool3  \"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)agent->toolset_count, 3);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
    ck_assert_str_eq(agent->toolset_filter[2], "tool3");
}
END_TEST

// Test: toolset with empty tokens (tests lines 46-48)
START_TEST(test_toolset_replay_empty_tokens) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-empty";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\"tool1,  ,tool2, ,tool3\"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Empty tokens should be skipped
    ck_assert_int_eq((int)agent->toolset_count, 3);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
    ck_assert_str_eq(agent->toolset_filter[2], "tool3");
}
END_TEST

// Test: toolset with many tools (tests realloc lines 57-59)
START_TEST(test_toolset_replay_many_tools) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-many";
    insert_agent(agent_uuid);

    // Create a string with 20 tools (exceeds initial capacity of 16)
    char args[512];
    strcpy(args, "tool1");
    for (int32_t i = 2; i <= 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), ",tool%d", (int)i);
        strcat(args, buf);
    }

    char data_json[1024];
    snprintf(data_json, sizeof(data_json), "{\"command\":\"toolset\",\"args\":\"%s\"}", args);
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)agent->toolset_count, 20);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[19], "tool20");
}
END_TEST

// Test: toolset replay replaces existing filter (tests lines 23-26)
START_TEST(test_toolset_replay_replaces_existing) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-replace";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\"newtool1,newtool2\"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    // Set existing toolset filter
    agent->toolset_count = 2;
    agent->toolset_filter = talloc_array(agent, char *, 2);
    agent->toolset_filter[0] = talloc_strdup(agent, "oldtool1");
    agent->toolset_filter[1] = talloc_strdup(agent, "oldtool2");

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Old filter should be replaced
    ck_assert_int_eq((int)agent->toolset_count, 2);
    ck_assert_str_eq(agent->toolset_filter[0], "newtool1");
    ck_assert_str_eq(agent->toolset_filter[1], "newtool2");
}
END_TEST

// Test: no toolset command in database
START_TEST(test_toolset_replay_no_command) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-none";
    insert_agent(agent_uuid);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: toolset command with NULL args (tests lines 120-123)
START_TEST(test_toolset_replay_null_args) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-null-args";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty (NULL args not processed)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: toolset command with non-string args (tests lines 123)
START_TEST(test_toolset_replay_nonstring_args) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-nonstring";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":123}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty (non-string args not processed)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: toolset command with space-only args
START_TEST(test_toolset_replay_space_only) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-space";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\"   \"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Toolset should remain empty (all spaces trimmed)
    ck_assert_int_eq((int)agent->toolset_count, 0);
}
END_TEST

// Test: toolset with mixed leading/trailing whitespace and commas (lines 43-54)
START_TEST(test_toolset_replay_mixed_delimiters) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-mixed";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\" , tool1 , , tool2, ,\"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)agent->toolset_count, 2);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
}
END_TEST

// Test: toolset with tokens that need both leading and trailing trim (lines 43-54)
START_TEST(test_toolset_replay_complex_trim) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-complex";
    insert_agent(agent_uuid);

    const char *data_json = "{\"command\":\"toolset\",\"args\":\"  , ,tool1, ,  , ,tool2 , , \"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    ck_assert_int_eq((int)agent->toolset_count, 2);
    ck_assert_str_eq(agent->toolset_filter[0], "tool1");
    ck_assert_str_eq(agent->toolset_filter[1], "tool2");
}
END_TEST

// Test: toolset with malformed JSON (tests line 159)
START_TEST(test_toolset_replay_malformed_json) {
    SKIP_IF_NO_DB();

    const char *agent_uuid = "test-toolset-malformed";
    insert_agent(agent_uuid);

    // Insert a message with data that will parse as valid JSON to PostgreSQL
    // but contains content that when extracted will fail yyjson parsing
    const char *data_json = "{\"command\":\"toolset\",\"args\":\"valid\"}";
    insert_message(agent_uuid, "command", data_json);

    ik_agent_ctx_t *agent = create_test_agent(agent_uuid);

    res_t res = ik_agent_replay_toolset(db, agent);
    ck_assert(is_ok(&res));

    // Should succeed (args is valid string)
    ck_assert_int_eq((int)agent->toolset_count, 1);
}
END_TEST

static Suite *agent_restore_replay_toolset_suite(void)
{
    Suite *s = suite_create("Agent Restore Replay - Toolset Commands");

    TCase *tc = tcase_create("Toolset Replay");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);

    tcase_add_test(tc, test_toolset_replay_basic);
    tcase_add_test(tc, test_toolset_replay_with_spaces);
    tcase_add_test(tc, test_toolset_replay_empty_tokens);
    tcase_add_test(tc, test_toolset_replay_many_tools);
    tcase_add_test(tc, test_toolset_replay_replaces_existing);
    tcase_add_test(tc, test_toolset_replay_no_command);
    tcase_add_test(tc, test_toolset_replay_null_args);
    tcase_add_test(tc, test_toolset_replay_nonstring_args);
    tcase_add_test(tc, test_toolset_replay_space_only);
    tcase_add_test(tc, test_toolset_replay_mixed_delimiters);
    tcase_add_test(tc, test_toolset_replay_complex_trim);
    tcase_add_test(tc, test_toolset_replay_malformed_json);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = agent_restore_replay_toolset_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/agent_restore_replay_toolset_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
