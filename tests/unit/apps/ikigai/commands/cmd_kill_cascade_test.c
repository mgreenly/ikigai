// Unit tests for /kill command (cascade kill variant)

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <talloc.h>

int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static int64_t session_id;

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
    agent->parent_uuid = NULL;
    agent->created_at = 1234567890;
    agent->fork_message_id = 0;
    repl->current = agent;

    ik_shared_ctx_t *shared = talloc_zero(test_ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->db_ctx = db;
    atomic_init(&shared->fork_pending, false);
    shared->session_id = session_id;
    repl->shared = shared;
    agent->shared = shared;

    repl->agents = talloc_zero_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 16;

    res_t res = ik_db_agent_insert(db, agent);
    if (is_err(&res)) {
        fprintf(stderr, "Insert agent failed: %s\n", error_message(res.err));
        ck_abort_msg("Setup failed");
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

    const char *session_query = "INSERT INTO sessions DEFAULT VALUES RETURNING id";
    PGresult *session_res = PQexec(db->conn, session_query);
    if (PQresultStatus(session_res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Failed to create session: %s\n", PQerrorMessage(db->conn));
        PQclear(session_res);
        ck_abort_msg("Session creation failed");
    }
    const char *session_id_str = PQgetvalue(session_res, 0, 0);
    session_id = (int64_t)atoll(session_id_str);
    PQclear(session_res);

    setup_repl();
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

START_TEST(test_kill_cascade_kills_target_and_children) {
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

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;
    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, initial_count);

    ik_agent_ctx_t *found_parent = ik_repl_find_agent(repl, parent_uuid);
    ik_agent_ctx_t *found_child1 = ik_repl_find_agent(repl, child1_uuid);
    ik_agent_ctx_t *found_child2 = ik_repl_find_agent(repl, child2_uuid);

    ck_assert_ptr_nonnull(found_parent);
    ck_assert_ptr_nonnull(found_child1);
    ck_assert_ptr_nonnull(found_child2);

    ck_assert(found_parent->dead);
    ck_assert(found_child1->dead);
    ck_assert(found_child2->dead);
}
END_TEST

START_TEST(test_kill_cascade_includes_grandchildren) {
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

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;
    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, initial_count);

    ik_agent_ctx_t *found_parent = ik_repl_find_agent(repl, parent_uuid);
    ik_agent_ctx_t *found_child = ik_repl_find_agent(repl, child_uuid);
    ik_agent_ctx_t *found_grandchild = ik_repl_find_agent(repl, grandchild_uuid);

    ck_assert_ptr_nonnull(found_parent);
    ck_assert_ptr_nonnull(found_child);
    ck_assert_ptr_nonnull(found_grandchild);

    ck_assert(found_parent->dead);
    ck_assert(found_child->dead);
    ck_assert(found_grandchild->dead);
}

END_TEST

START_TEST(test_kill_cascade_reports_count) {
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

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));

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

START_TEST(test_kill_cascade_always_includes_descendants) {
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

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    size_t initial_count = repl->agent_count;

    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->agent_count, initial_count);

    ik_agent_ctx_t *found_parent = ik_repl_find_agent(repl, parent_uuid);
    ik_agent_ctx_t *found_child1 = ik_repl_find_agent(repl, child1_uuid);
    ik_agent_ctx_t *found_child2 = ik_repl_find_agent(repl, child2_uuid);

    ck_assert_ptr_nonnull(found_parent);
    ck_assert_ptr_nonnull(found_child1);
    ck_assert_ptr_nonnull(found_child2);

    ck_assert(found_parent->dead);
    ck_assert(found_child1->dead);
    ck_assert(found_child2->dead);
}

END_TEST

START_TEST(test_kill_cascade_all_have_ended_at) {
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

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));

    time_t before_kill = time(NULL);

    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));

    time_t after_kill = time(NULL);

    const char *uuids[] = {parent_uuid, child1_uuid, child2_uuid};
    for (size_t i = 0; i < 3; i++) {
        ik_db_agent_row_t *row = NULL;
        res_t db_res = ik_db_agent_get(db, test_ctx, uuids[i], &row);
        ck_assert(is_ok(&db_res));
        ck_assert_ptr_nonnull(row);

        ck_assert_int_ne(row->ended_at, 0);
        ck_assert_int_ge(row->ended_at, before_kill);
        ck_assert_int_le(row->ended_at, after_kill + 1);
        ck_assert_str_eq(row->status, "dead");
    }
}

END_TEST

START_TEST(test_kill_cascade_event_has_cascade_metadata) {
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *parent_uuid = repl->current->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));
    const char *killer_uuid = repl->current->uuid;

    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));

    const char *query = "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {killer_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

    const char *data = PQgetvalue(pg_res, 0, 0);
    ck_assert_ptr_nonnull(data);
    ck_assert_ptr_nonnull(strstr(data, "cascade"));
    ck_assert_ptr_nonnull(strstr(data, "true"));

    PQclear(pg_res);
}

END_TEST

START_TEST(test_kill_cascade_event_count_matches) {
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));
    const char *parent_uuid = repl->current->uuid;

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    res = ik_repl_switch_agent(repl, ik_repl_find_agent(repl, parent_uuid));
    ck_assert(is_ok(&res));

    res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    res = ik_repl_switch_agent(repl, repl->agents[0]);
    ck_assert(is_ok(&res));
    const char *killer_uuid = repl->current->uuid;

    res = ik_cmd_kill(test_ctx, repl, parent_uuid);
    ck_assert(is_ok(&res));

    const char *query = "SELECT data FROM messages WHERE agent_uuid = $1 AND kind = 'agent_killed'";
    const char *params[1] = {killer_uuid};

    PGresult *pg_res = PQexecParams(db->conn, query, 1, NULL, params, NULL, NULL, 0);
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);
    ck_assert_int_ge(PQntuples(pg_res), 1);

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
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_kill_cascade_kills_target_and_children);
    tcase_add_test(tc, test_kill_cascade_includes_grandchildren);
    tcase_add_test(tc, test_kill_cascade_reports_count);
    tcase_add_test(tc, test_kill_cascade_always_includes_descendants);
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
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_kill_cascade_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
