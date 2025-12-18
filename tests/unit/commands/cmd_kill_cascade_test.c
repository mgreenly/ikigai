/**
 * @file cmd_kill_cascade_test.c
 * @brief Unit tests for /kill command (cascade kill variant)
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/openai/client.h"
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

    res_t res = ik_openai_conversation_create(test_ctx);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;

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
    res = ik_db_agent_insert(db, agent);
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

// Test: --cascade kills target and children
START_TEST(test_kill_cascade_kills_target_and_children)
{
    // Create parent with 2 children
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *child1_uuid = repl->current->uuid;

    // Switch back to parent
    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *child2_uuid = repl->current->uuid;

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    // Kill parent with --cascade
    char args[128];
    snprintf(args, sizeof(args), "%s --cascade", parent_uuid);
    res = ik_cmd_kill(test_ctx, repl, args);
    ck_assert(is_ok(&res));

    // Should have removed 3 agents (parent + 2 children)
    ck_assert_uint_eq(repl->agent_count, initial_count - 3);

    // Verify none of them are in the array
    for (size_t i = 0; i < repl->agent_count; i++) {
        ck_assert_str_ne(repl->agents[i]->uuid, parent_uuid);
        ck_assert_str_ne(repl->agents[i]->uuid, child1_uuid);
        ck_assert_str_ne(repl->agents[i]->uuid, child2_uuid);
    }
}
END_TEST

// Test: --cascade with grandchildren (depth-first order)
START_TEST(test_kill_cascade_includes_grandchildren)
{
    // Create 3-level hierarchy: root -> parent -> child -> grandchild
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *grandchild_uuid = repl->current->uuid;

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    // Kill parent with --cascade
    char args[128];
    snprintf(args, sizeof(args), "%s --cascade", parent_uuid);
    res = ik_cmd_kill(test_ctx, repl, args);
    ck_assert(is_ok(&res));

    // Should have removed 3 agents
    ck_assert_uint_eq(repl->agent_count, initial_count - 3);

    // Verify all are gone
    for (size_t i = 0; i < repl->agent_count; i++) {
        ck_assert_str_ne(repl->agents[i]->uuid, parent_uuid);
        ck_assert_str_ne(repl->agents[i]->uuid, child_uuid);
        ck_assert_str_ne(repl->agents[i]->uuid, grandchild_uuid);
    }
}
END_TEST

// Test: --cascade report shows correct count
START_TEST(test_kill_cascade_reports_count)
{
    // Create parent with 2 children
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    // Kill parent with --cascade
    char args[128];
    snprintf(args, sizeof(args), "%s --cascade", parent_uuid);
    res = ik_cmd_kill(test_ctx, repl, args);
    ck_assert(is_ok(&res));

    // Check scrollback for "Killed 3 agents" message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    bool found_message = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Killed 3 agents")) {
            found_message = true;
            break;
        }
    }
    ck_assert(found_message);
}
END_TEST

// Test: without --cascade only kills target
START_TEST(test_kill_without_cascade_only_kills_target)
{
    // Create parent with 2 children
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *child1_uuid = repl->current->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *child2_uuid = repl->current->uuid;

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    // Kill parent WITHOUT --cascade (just UUID)
    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));

    // Should have removed only 1 agent (parent)
    ck_assert_uint_eq(repl->agent_count, initial_count - 1);

    // Parent should be gone
    bool found_parent = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, parent_uuid) == 0) {
            found_parent = true;
            break;
        }
    }
    ck_assert(!found_parent);

    // Children should still exist
    bool found_child1 = false;
    bool found_child2 = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, child1_uuid) == 0) {
            found_child1 = true;
        }
        if (strcmp(repl->agents[i]->uuid, child2_uuid) == 0) {
            found_child2 = true;
        }
    }
    ck_assert(found_child1);
    ck_assert(found_child2);
}
END_TEST

// Test: --cascade all killed agents have ended_at set
START_TEST(test_kill_cascade_all_have_ended_at)
{
    // Create parent with 2 children
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *child1_uuid = repl->current->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *child2_uuid = repl->current->uuid;

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    time_t before_kill = time(NULL);

    // Kill parent with --cascade
    char args[128];
    snprintf(args, sizeof(args), "%s --cascade", parent_uuid);
    res = ik_cmd_kill(test_ctx, repl, args);
    ck_assert(is_ok(&res));

    time_t after_kill = time(NULL);

    // Check registry for all 3 agents
    const char *uuids[] = {parent_uuid, child1_uuid, child2_uuid};
    for (size_t i = 0; i < 3; i++) {
        ik_db_agent_row_t *row = NULL;
        res_t db_res = ik_db_agent_get(db, test_ctx, uuids[i], &row);
        ck_assert(is_ok(&db_res));
        ck_assert_ptr_nonnull(row);

        // All should have ended_at set
        ck_assert_int_ne(row->ended_at, 0);
        ck_assert_int_ge(row->ended_at, before_kill);
        ck_assert_int_le(row->ended_at, after_kill + 1);

        // All should be dead
        ck_assert_str_eq(row->status, "dead");
    }
}
END_TEST

// Test: --cascade agent_killed event has cascade=true metadata
START_TEST(test_kill_cascade_event_has_cascade_metadata)
{
    // Create parent with 1 child
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *parent_uuid = repl->current->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));
    const char *killer_uuid = repl->current->uuid;

    // Kill parent with --cascade
    char args[128];
    snprintf(args, sizeof(args), "%s --cascade", parent_uuid);
    res = ik_cmd_kill(test_ctx, repl, args);
    ck_assert(is_ok(&res));

    // Query database for agent_killed event in current agent's history
    const char *query = "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {killer_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

    // Check data field contains cascade: true
    const char *data = PQgetvalue(pg_res, 0, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(strstr(data, "cascade"));
    ck_assert_ptr_nonnull(strstr(data, "true"));

    PQclear(pg_res);
}
END_TEST

// Test: --cascade agent_killed event count matches killed agents
START_TEST(test_kill_cascade_event_count_matches)
{
    // Create parent with 2 children
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *parent_uuid = repl->current->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    res = ik_repl_switch_agent(repl, ik_repl_find_agent(repl, parent_uuid));
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    // Switch to root
    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));
    const char *killer_uuid = repl->current->uuid;

    // Kill parent with --cascade (should kill 3 agents)
    char args[128];
    snprintf(args, sizeof(args), "%s --cascade", parent_uuid);
    res = ik_cmd_kill(test_ctx, repl, args);
    ck_assert(is_ok(&res));

    // Query database for agent_killed event
    const char *query = "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {killer_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

    // Check data field contains count: 3
    const char *data = PQgetvalue(pg_res, 0, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(strstr(data, "count"));
    ck_assert_ptr_nonnull(strstr(data, "3"));

    PQclear(pg_res);
}
END_TEST

static Suite *cmd_kill_suite(void)
{
    Suite *s = suite_create("Kill Command (Cascade)");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_kill_cascade_kills_target_and_children);
    tcase_add_test(tc, test_kill_cascade_includes_grandchildren);
    tcase_add_test(tc, test_kill_cascade_reports_count);
    tcase_add_test(tc, test_kill_without_cascade_only_kills_target);
    tcase_add_test(tc, test_kill_cascade_all_have_ended_at);
    tcase_add_test(tc, test_kill_cascade_event_has_cascade_metadata);
    tcase_add_test(tc, test_kill_cascade_event_count_matches);

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
