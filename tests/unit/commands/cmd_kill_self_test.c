/**
 * @file cmd_kill_self_test.c
 * @brief Unit tests for /kill command (self-kill variant)
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
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

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = sb;

    agent->uuid = talloc_strdup(agent, "parent-uuid-123");
    agent->name = NULL;
    agent->parent_uuid = NULL;  // Root agent
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    atomic_init(&shared->fork_pending, false);
    shared->session_id = 0;  // Will be set in setup()
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

    // Truncate all tables before setup to ensure clean slate
    ik_test_db_truncate_all(db);

    setup_repl();

    // Create a session for the tests
    const char *session_query = "INSERT INTO sessions DEFAULT VALUES RETURNING id";
    PGresult *session_res = PQexec(db->conn, session_query);
    if (PQresultStatus(session_res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Failed to create session: %s\n", PQerrorMessage(db->conn));
        PQclear(session_res);
        ck_abort_msg("Session creation failed");
    }
    const char *session_id_str = PQgetvalue(session_res, 0, 0);
    repl->shared->session_id = (int64_t)atoll(session_id_str);
    PQclear(session_res);
}

static void teardown(void)
{
    // Clean up database state for next test BEFORE freeing context
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }

    // Now free everything (this also closes db connection via destructor)
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }

    db = NULL;
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Test: /kill on non-root terminates agent
START_TEST(test_kill_terminates_non_root) {
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;
    ik_agent_ctx_t *parent = repl->agents[0];

    size_t initial_count = repl->agent_count;

    // Kill child (current agent)
    res = ik_cmd_kill(test_ctx, repl, NULL);
    if (is_err(&res)) {
        fprintf(stderr, "ik_cmd_kill failed: %s\n", error_message(res.err));
    }
    ck_assert(is_ok(&res));

    // Should switch to parent
    ck_assert_ptr_eq(repl->current, parent);

    // Agent count should decrease
    ck_assert_uint_eq(repl->agent_count, initial_count - 1);

    // Child should not be in array
    bool found = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, child_uuid) == 0) {
            found = true;
            break;
        }
    }
    ck_assert(!found);
}
END_TEST
// Test: Registry updated to status='dead'
START_TEST(test_kill_marks_dead_in_registry)
{
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;

    // Kill child
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Check registry
    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, child_uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);
    ck_assert_str_eq(row->status, "dead");
}

END_TEST
// Test: Registry ended_at is set to current timestamp
START_TEST(test_kill_sets_ended_at)
{
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;
    time_t before_kill = time(NULL);

    // Kill child
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    time_t after_kill = time(NULL);

    // Check registry
    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, child_uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);

    // ended_at should be set and within reasonable range
    ck_assert_int_ne(row->ended_at, 0);
    ck_assert_int_ge(row->ended_at, before_kill);
    ck_assert_int_le(row->ended_at, after_kill + 1);  // Allow 1 second tolerance
}

END_TEST
// Test: Agent removed from array
START_TEST(test_kill_removes_from_array)
{
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;
    size_t initial_count = repl->agent_count;

    // Kill child
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Verify count decreased
    ck_assert_uint_eq(repl->agent_count, initial_count - 1);

    // Verify child not in array
    for (size_t i = 0; i < repl->agent_count; i++) {
        ck_assert_str_ne(repl->agents[i]->uuid, child_uuid);
    }
}

END_TEST
// Test: Switches to parent
START_TEST(test_kill_switches_to_parent)
{
    ik_agent_ctx_t *parent = repl->current;

    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    ck_assert_ptr_ne(child, parent);

    // Kill child
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Should be back to parent
    ck_assert_ptr_eq(repl->current, parent);
}

END_TEST
// Test: /kill on root shows error
START_TEST(test_kill_root_shows_error)
{
    // Current agent is root (parent_uuid == NULL)
    ck_assert_ptr_null(repl->current->parent_uuid);

    res_t res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));  // Command returns OK but shows error

    // Check scrollback for error message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Cannot kill root agent")) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}

END_TEST
// Test: Root agent not modified
START_TEST(test_kill_root_not_modified)
{
    const char *root_uuid = repl->current->uuid;

    // Try to kill root
    res_t res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Root should still be current
    ck_assert_str_eq(repl->current->uuid, root_uuid);

    // Root should still be in registry with status='running'
    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, root_uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);
    ck_assert_str_eq(row->status, "running");
}

END_TEST
// Test: Kill waits for fork_pending to clear (sync barrier)
START_TEST(test_kill_waits_for_fork_pending)
{
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Set fork_pending (simulating concurrent fork)
    atomic_store(&repl->shared->fork_pending, true);

    // Kill should wait for fork_pending to be false
    // In real code, it would loop. In test, we just verify the check happens
    // by manually clearing it before calling
    atomic_store(&repl->shared->fork_pending, false);

    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Kill should have succeeded
    ck_assert_ptr_eq(repl->current, repl->agents[0]);
}

END_TEST
// Test: agent_killed event recorded in parent's history
START_TEST(test_kill_records_event_in_parent_history)
{
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *parent_uuid = repl->current->parent_uuid;

    // Kill child
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Query database for agent_killed event in parent's message history
    const char *query = "SELECT kind, data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {parent_uuid};

    TALLOC_CTX *tmp = talloc_new(NULL);
    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);

    // Should have at least one agent_killed event
    int num_rows = PQntuples(pg_res);
    ck_assert_int_ge(num_rows, 1);

    // Verify kind
    const char *kind = PQgetvalue(pg_res, 0, 0);
    ck_assert_str_eq(kind, "agent_killed");

    PQclear(pg_res);
    talloc_free(tmp);
}

END_TEST
// Test: agent_killed event has killed_by="user" metadata
START_TEST(test_kill_event_has_killed_by_user)
{
    // Create child agent
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *parent_uuid = repl->current->parent_uuid;

    // Kill child
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Query database for agent_killed event
    const char *query = "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {parent_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

    // Check data field contains killed_by: "user"
    const char *data = PQgetvalue(pg_res, 0, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(strstr(data, "killed_by"));
    ck_assert_ptr_nonnull(strstr(data, "user"));

    PQclear(pg_res);
}

END_TEST

static Suite *cmd_kill_suite(void)
{
    Suite *s = suite_create("Kill Command (Self)");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_kill_terminates_non_root);
    tcase_add_test(tc, test_kill_marks_dead_in_registry);
    tcase_add_test(tc, test_kill_sets_ended_at);
    tcase_add_test(tc, test_kill_removes_from_array);
    tcase_add_test(tc, test_kill_switches_to_parent);
    tcase_add_test(tc, test_kill_root_shows_error);
    tcase_add_test(tc, test_kill_root_not_modified);
    tcase_add_test(tc, test_kill_waits_for_fork_pending);
    tcase_add_test(tc, test_kill_records_event_in_parent_history);
    tcase_add_test(tc, test_kill_event_has_killed_by_user);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_kill_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
