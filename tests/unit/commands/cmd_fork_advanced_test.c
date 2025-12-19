/**
 * @file cmd_fork_advanced_test.c
 * @brief Unit tests for /fork command - advanced features
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>

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

    ik_openai_conversation_t *conv = ik_openai_conversation_create(test_ctx);

    ik_cfg_t *cfg = talloc_zero(test_ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;
    agent->conversation = conv;
    agent->uuid = talloc_strdup(agent, "parent-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    atomic_init(&shared->fork_pending, false);
    repl->shared = shared;
    agent->shared = shared;

    // Initialize agent array
    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    // Insert parent agent into registry
    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Failed to insert parent agent: %s\n", error_message(res.err));
        ck_abort_msg("Failed to setup parent agent in registry");
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
    // Don't call ik_test_db_begin - ik_cmd_fork manages its own transactions

    setup_repl();
}

static void teardown(void)
{
    // Clean up database state for next test BEFORE freeing context
    // because db is allocated as child of test_ctx
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }

    // Now free everything (this also closes db connection via destructor)
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;  // Will be recreated in next setup
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: Fork records fork_message_id (parent has no messages)
START_TEST(test_fork_records_fork_message_id_no_messages)
{
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Child should have fork_message_id = 0 (parent has no messages)
    ik_agent_ctx_t *child = repl->current;
    ck_assert_int_eq(child->fork_message_id, 0);
}
END_TEST

// Test: Fork stores fork_message_id in registry
START_TEST(test_fork_registry_has_fork_message_id)
{
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Query registry for child
    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, repl->current->uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);
    ck_assert_ptr_nonnull(row->fork_message_id);
    // Should be "0" for parent with no messages
    ck_assert_str_eq(row->fork_message_id, "0");
}
END_TEST

// Test: Child inherits parent conversation
START_TEST(test_fork_child_inherits_conversation)
{
    // Add a message to parent's conversation before forking
    ik_msg_t *msg = ik_openai_msg_create(test_ctx, "user", "Test message from parent");

    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg);
    ck_assert(is_ok(&add_res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Child should inherit parent's conversation
    ik_agent_ctx_t *child = repl->current;
    ck_assert_ptr_nonnull(child->conversation);
    ck_assert_uint_eq(child->conversation->message_count, 1);

    // Verify the message content was copied
    ck_assert_str_eq(child->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(child->conversation->messages[0]->content, "Test message from parent");
}
END_TEST

// Test: Fork sync barrier - fork with no running tools proceeds immediately
START_TEST(test_fork_no_running_tools_proceeds)
{
    // Ensure no running tools
    ck_assert(!repl->current->tool_thread_running);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Fork should have succeeded
    ck_assert_uint_eq(repl->agent_count, 2);
}
END_TEST

// Test: Fork sync barrier - ik_agent_has_running_tools returns false when no tools
START_TEST(test_has_running_tools_false_when_idle)
{
    repl->current->tool_thread_running = false;
    ck_assert(!ik_agent_has_running_tools(repl->current));
}
END_TEST

// Test: Fork sync barrier - ik_agent_has_running_tools returns true when tool running
START_TEST(test_has_running_tools_true_when_running)
{
    repl->current->tool_thread_running = true;
    ck_assert(ik_agent_has_running_tools(repl->current));
    // Reset for cleanup
    repl->current->tool_thread_running = false;
}
END_TEST

// Test: Fork sync barrier - waiting message displayed when tools running
START_TEST(test_fork_waiting_message_when_tools_running)
{
    // Set up a running tool
    repl->current->tool_thread_running = true;
    repl->current->tool_thread_complete = false;

    // We can't test the full blocking behavior in unit tests
    // (that would require threading), but we can test that
    // the check function works
    ck_assert(ik_agent_has_running_tools(repl->current));

    // Reset for cleanup
    repl->current->tool_thread_running = false;
}
END_TEST

// Test: Fork sync barrier - tool_thread_complete is respected
START_TEST(test_has_running_tools_respects_complete_flag)
{
    // Thread running but not complete
    repl->current->tool_thread_running = true;
    repl->current->tool_thread_complete = false;
    ck_assert(ik_agent_has_running_tools(repl->current));

    // Thread no longer running
    repl->current->tool_thread_running = false;
    ck_assert(!ik_agent_has_running_tools(repl->current));
}
END_TEST

// Test: Child post-fork messages are separate from parent
START_TEST(test_fork_child_post_fork_messages_separate)
{
    // Add initial message to parent
    ik_msg_t *parent_msg = ik_openai_msg_create(test_ctx, "user", "Parent message before fork");
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, parent_msg);
    ck_assert(is_ok(&add_res));

    ik_agent_ctx_t *parent = repl->current;
    size_t parent_msg_count_before_fork = parent->conversation->message_count;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Add message to child's conversation (simulating post-fork message)
    ik_agent_ctx_t *child = repl->current;
    ik_msg_t *child_msg = ik_openai_msg_create(test_ctx, "user", "Child message after fork");
    res_t child_add_res = ik_openai_conversation_add_msg(child->conversation, child_msg);
    ck_assert(is_ok(&child_add_res));

    // Child should have the post-fork message
    ck_assert_uint_eq(child->conversation->message_count, 2);

    // Parent's conversation should remain unchanged
    ck_assert_uint_eq(parent->conversation->message_count, parent_msg_count_before_fork);
}
END_TEST

// Test: Fork persists parent-side fork event
START_TEST(test_fork_persists_parent_side_event)
{
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    // Query messages table directly for parent's fork event
    const char *query = "SELECT kind, content, data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id";
    char session_id_str[32];
    snprintf(session_id_str, sizeof(session_id_str), "%" PRId64, session_id);
    const char *params[2] = {session_id_str, parent_uuid};
    PGresult *pg_res = PQexecParams(db->conn, query, 2, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);

    int nrows = PQntuples(pg_res);
    ck_assert_int_ge(nrows, 1);

    // Check first fork event
    const char *kind = PQgetvalue(pg_res, 0, 0);
    const char *content = PQgetvalue(pg_res, 0, 1);
    const char *data = PQgetvalue(pg_res, 0, 2);

    ck_assert_str_eq(kind, "fork");
    ck_assert_ptr_nonnull(content);
    ck_assert(strstr(content, child_uuid) != NULL);
    ck_assert_ptr_nonnull(data);
    ck_assert(strstr(data, "\"child_uuid\"") != NULL);
    ck_assert(strstr(data, child_uuid) != NULL);
    ck_assert(strstr(data, "\"role\":\"parent\"") != NULL ||
             strstr(data, "\"role\": \"parent\"") != NULL);

    PQclear(pg_res);
}
END_TEST

// Test: Fork persists child-side fork event
START_TEST(test_fork_persists_child_side_event)
{
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    // Query messages table directly for child's fork event
    const char *query = "SELECT kind, content, data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id";
    char session_id_str[32];
    snprintf(session_id_str, sizeof(session_id_str), "%" PRId64, session_id);
    const char *params[2] = {session_id_str, child_uuid};
    PGresult *pg_res = PQexecParams(db->conn, query, 2, NULL, params, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);

    int nrows = PQntuples(pg_res);
    ck_assert_int_ge(nrows, 1);

    // Check first fork event
    const char *kind = PQgetvalue(pg_res, 0, 0);
    const char *content = PQgetvalue(pg_res, 0, 1);
    const char *data = PQgetvalue(pg_res, 0, 2);

    ck_assert_str_eq(kind, "fork");
    ck_assert_ptr_nonnull(content);
    ck_assert(strstr(content, parent_uuid) != NULL);
    ck_assert_ptr_nonnull(data);
    ck_assert(strstr(data, "\"parent_uuid\"") != NULL);
    ck_assert(strstr(data, parent_uuid) != NULL);
    ck_assert(strstr(data, "\"role\":\"child\"") != NULL ||
             strstr(data, "\"role\": \"child\"") != NULL);

    PQclear(pg_res);
}
END_TEST

// Test: Fork events link via fork_message_id
START_TEST(test_fork_events_linked_by_fork_message_id)
{
    // Create a session
    int64_t session_id = 0;
    res_t session_res = ik_db_session_create(db, &session_id);
    ck_assert(is_ok(&session_res));
    repl->shared->session_id = session_id;

    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    // Query parent's fork event
    const char *query1 = "SELECT data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id LIMIT 1";
    char session_id_str1[32];
    snprintf(session_id_str1, sizeof(session_id_str1), "%" PRId64, session_id);
    const char *params1[2] = {session_id_str1, parent_uuid};
    PGresult *parent_res = PQexecParams(db->conn, query1, 2, NULL, params1, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(parent_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(parent_res), 1);

    const char *parent_data = PQgetvalue(parent_res, 0, 0);
    ck_assert_ptr_nonnull(parent_data);
    const char *parent_fork_id_str = strstr(parent_data, "\"fork_message_id\"");
    ck_assert_ptr_nonnull(parent_fork_id_str);

    int64_t parent_fork_msg_id = -1;
    sscanf(parent_fork_id_str, "\"fork_message_id\": %" SCNd64, &parent_fork_msg_id);
    ck_assert_int_ge(parent_fork_msg_id, 0);
    PQclear(parent_res);

    // Query child's fork event
    const char *query2 = "SELECT data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id LIMIT 1";
    char session_id_str2[32];
    snprintf(session_id_str2, sizeof(session_id_str2), "%" PRId64, session_id);
    const char *params2[2] = {session_id_str2, child_uuid};
    PGresult *child_res = PQexecParams(db->conn, query2, 2, NULL, params2, NULL, NULL, 0);
    ck_assert_int_eq(PQresultStatus(child_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(child_res), 1);

    const char *child_data = PQgetvalue(child_res, 0, 0);
    ck_assert_ptr_nonnull(child_data);
    const char *child_fork_id_str = strstr(child_data, "\"fork_message_id\"");
    ck_assert_ptr_nonnull(child_fork_id_str);

    int64_t child_fork_msg_id = -1;
    sscanf(child_fork_id_str, "\"fork_message_id\": %" SCNd64, &child_fork_msg_id);
    ck_assert_int_ge(child_fork_msg_id, 0);
    PQclear(child_res);

    // They should match
    ck_assert_int_eq(parent_fork_msg_id, child_fork_msg_id);
}
END_TEST

// Test: Child inherits parent's scrollback
START_TEST(test_fork_child_inherits_scrollback)
{
    // Add some lines to parent's scrollback before forking
    res_t res1 = ik_scrollback_append_line(repl->current->scrollback, "Line 1 from parent", 18);
    ck_assert(is_ok(&res1));
    res_t res2 = ik_scrollback_append_line(repl->current->scrollback, "Line 2 from parent", 18);
    ck_assert(is_ok(&res2));
    res_t res3 = ik_scrollback_append_line(repl->current->scrollback, "Line 3 from parent", 18);
    ck_assert(is_ok(&res3));

    size_t parent_line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(parent_line_count, 3);

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Child should inherit parent's scrollback (plus fork confirmation message)
    ik_agent_ctx_t *child = repl->current;
    size_t child_line_count = ik_scrollback_get_line_count(child->scrollback);
    ck_assert_uint_ge(child_line_count, parent_line_count);

    // Verify the first 3 lines match parent's content
    for (size_t i = 0; i < 3; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(child->scrollback, i, &text, &length);
        ck_assert(is_ok(&line_res));
        ck_assert_ptr_nonnull(text);

        char expected[32];
        snprintf(expected, sizeof(expected), "Line %zu from parent", i + 1);
        ck_assert_uint_eq(length, 18);
        ck_assert_int_eq(strncmp(text, expected, length), 0);
    }
}
END_TEST


static Suite *cmd_fork_suite(void)
{
    Suite *s = suite_create("Fork Command Advanced");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_records_fork_message_id_no_messages);
    tcase_add_test(tc, test_fork_registry_has_fork_message_id);
    tcase_add_test(tc, test_fork_child_inherits_conversation);
    tcase_add_test(tc, test_fork_child_post_fork_messages_separate);
    tcase_add_test(tc, test_fork_no_running_tools_proceeds);
    tcase_add_test(tc, test_has_running_tools_false_when_idle);
    tcase_add_test(tc, test_has_running_tools_true_when_running);
    tcase_add_test(tc, test_fork_waiting_message_when_tools_running);
    tcase_add_test(tc, test_has_running_tools_respects_complete_flag);
    tcase_add_test(tc, test_fork_persists_parent_side_event);
    tcase_add_test(tc, test_fork_persists_child_side_event);
    tcase_add_test(tc, test_fork_events_linked_by_fork_message_id);
    tcase_add_test(tc, test_fork_child_inherits_scrollback);

    suite_add_tcase(s, tc);
    return s;
}
int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_fork_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
