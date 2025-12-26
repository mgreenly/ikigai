/**
 * @file cmd_fork_events_test.c
 * @brief Unit tests for /fork command - event persistence
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/error.h"
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

// Test: Fork persists parent-side fork event
START_TEST(test_fork_persists_parent_side_event) {
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
    const char *query =
        "SELECT kind, content, data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id";
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
    const char *query =
        "SELECT kind, content, data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id";
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
    const char *query1 =
        "SELECT data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id LIMIT 1";
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
    const char *query2 =
        "SELECT data FROM messages WHERE session_id=$1 AND agent_uuid=$2 AND kind='fork' ORDER BY id LIMIT 1";
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

static Suite *cmd_fork_suite(void)
{
    Suite *s = suite_create("Fork Command Events");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_persists_parent_side_event);
    tcase_add_test(tc, test_fork_persists_child_side_event);
    tcase_add_test(tc, test_fork_events_linked_by_fork_message_id);

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
