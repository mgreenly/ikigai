/**
 * @file cmd_kill_target_test.c
 * @brief Unit tests for /kill <uuid> targeted kill command
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
    shared->session_id = 0;
    repl->shared = shared;
    agent->shared = shared;

    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

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

    ik_test_db_truncate_all(db);
    setup_repl();

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
    if (db != NULL && test_ctx != NULL) {
        ik_test_db_truncate_all(db);
    }
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

// Test: /kill <uuid> terminates specific agent
START_TEST(test_kill_target_terminates_specific_agent) {
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    res = ik_cmd_kill(test_ctx, repl, child_uuid);
    ck_assert(is_ok(&res));

    ck_assert_ptr_eq(repl->current, parent);
    ck_assert_uint_eq(repl->agent_count, initial_count - 1);

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
// Test: partial UUID matching works
START_TEST(test_kill_target_partial_uuid_match)
{
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    char partial[9];
    strncpy(partial, child_uuid, 8);
    partial[8] = '\0';

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_kill(test_ctx, repl, partial);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *found = ik_repl_find_agent(repl, child_uuid);
    ck_assert_ptr_null(found);
}

END_TEST
// Test: ambiguous UUID shows error
START_TEST(test_kill_target_ambiguous_uuid_error)
{
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    ik_agent_ctx_t *child1 = repl->current;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    ik_scrollback_clear(parent->scrollback);

    res = ik_cmd_kill(test_ctx, repl, "");
    ck_assert(is_ok(&res));

    ck_assert_ptr_eq(repl->current, parent);

    (void)child1;
}

END_TEST
// Test: non-existent UUID shows error
START_TEST(test_kill_target_nonexistent_uuid_error)
{
    ik_agent_ctx_t *parent = repl->current;

    ik_scrollback_clear(parent->scrollback);

    res_t res = ik_cmd_kill(test_ctx, repl, "nonexistent-uuid-123");
    ck_assert(is_ok(&res));

    size_t line_count = ik_scrollback_get_line_count(parent->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(parent->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Agent not found")) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}

END_TEST
// Test: killing current agent via UUID switches to parent
START_TEST(test_kill_target_current_switches_to_parent)
{
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;
    const char *child_uuid = child->uuid;

    res = ik_cmd_kill(test_ctx, repl, child_uuid);
    ck_assert(is_ok(&res));

    ck_assert_ptr_eq(repl->current, parent);
}

END_TEST
// Test: killing root via UUID shows error
START_TEST(test_kill_target_root_shows_error)
{
    const char *root_uuid = repl->current->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;

    ik_scrollback_clear(child->scrollback);

    res = ik_cmd_kill(test_ctx, repl, root_uuid);
    ck_assert(is_ok(&res));

    size_t line_count = ik_scrollback_get_line_count(child->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(child->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Cannot kill root agent")) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}

END_TEST
// Test: user stays on current agent after targeted kill
START_TEST(test_kill_target_user_stays_on_current)
{
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_kill(test_ctx, repl, child_uuid);
    ck_assert(is_ok(&res));

    ck_assert_ptr_eq(repl->current, parent);
}

END_TEST
// Test: registry ended_at is set for targeted kill
START_TEST(test_kill_target_sets_ended_at)
{
    ik_agent_ctx_t *parent = repl->current;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    time_t before_kill = time(NULL);

    res = ik_cmd_kill(test_ctx, repl, child_uuid);
    ck_assert(is_ok(&res));

    time_t after_kill = time(NULL);

    ik_db_agent_row_t *row = NULL;
    res_t db_res = ik_db_agent_get(db, test_ctx, child_uuid, &row);
    ck_assert(is_ok(&db_res));
    ck_assert_ptr_nonnull(row);
    ck_assert_str_eq(row->status, "dead");
    ck_assert_int_ne(row->ended_at, 0);
    ck_assert_int_ge(row->ended_at, before_kill);
    ck_assert_int_le(row->ended_at, after_kill + 1);
}

END_TEST
// Test: agent_killed event recorded in current agent's history for targeted kill
START_TEST(test_kill_target_records_event_in_current_history)
{
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_kill(test_ctx, repl, child_uuid);
    ck_assert(is_ok(&res));

    const char *query = "SELECT kind, data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {parent_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

    const char *kind = PQgetvalue(pg_res, 0, 0);
    ck_assert_str_eq(kind, "agent_killed");

    PQclear(pg_res);
}

END_TEST
// Test: agent_killed event has correct target UUID in metadata
START_TEST(test_kill_target_event_has_target_uuid)
{
    ik_agent_ctx_t *parent = repl->current;
    const char *parent_uuid = parent->uuid;

    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    const char *child_uuid = repl->current->uuid;

    res = ik_repl_switch_agent(repl, parent);
    ck_assert(is_ok(&res));

    res = ik_cmd_kill(test_ctx, repl, child_uuid);
    ck_assert(is_ok(&res));

    const char *query = "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {parent_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

    const char *data = PQgetvalue(pg_res, 0, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(strstr(data, "target"));
    ck_assert_ptr_nonnull(strstr(data, child_uuid));

    PQclear(pg_res);
}

END_TEST

static Suite *cmd_kill_target_suite(void)
{
    Suite *s = suite_create("Kill Command (Targeted)");
    TCase *tc = tcase_create("Targeted Kill");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_kill_target_terminates_specific_agent);
    tcase_add_test(tc, test_kill_target_partial_uuid_match);
    tcase_add_test(tc, test_kill_target_ambiguous_uuid_error);
    tcase_add_test(tc, test_kill_target_nonexistent_uuid_error);
    tcase_add_test(tc, test_kill_target_current_switches_to_parent);
    tcase_add_test(tc, test_kill_target_root_shows_error);
    tcase_add_test(tc, test_kill_target_user_stays_on_current);
    tcase_add_test(tc, test_kill_target_sets_ended_at);
    tcase_add_test(tc, test_kill_target_records_event_in_current_history);
    tcase_add_test(tc, test_kill_target_event_has_target_uuid);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_kill_target_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
