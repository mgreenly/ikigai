/**
 * @file cmd_agent_coverage_test.c
 * @brief Coverage tests for commands_agent.c error paths
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"
#include "tests/helpers/test_utils_helper.h"

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
static int64_t session_id;

// Helper: Create minimal REPL for testing
static void setup_repl(void)
{
    ik_scrollback_t *sb = ik_scrollback_create(test_ctx, 80);
    ck_assert_ptr_nonnull(sb);

    ik_config_t *cfg = talloc_zero(test_ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

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
    agent->tool_thread_running = false;
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

// Test: Fork with unterminated quoted string shows error
START_TEST(test_fork_unterminated_quote_error) {
    res_t res = ik_cmd_fork(test_ctx, repl, "\"unterminated string");
    ck_assert(is_ok(&res));  // Returns OK but shows error

    // Check scrollback for error message
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    bool found_error = false;
    for (size_t i = 0; i < line_count; i++) {
        const char *text = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &text, &length);
        if (is_ok(&line_res) && text && strstr(text, "Unterminated quoted string")) {
            found_error = true;
            break;
        }
    }
    ck_assert(found_error);
}
END_TEST
// Test: Kill with parent not found (corrupt state)
START_TEST(test_kill_parent_not_found_error) {
    // Create child
    res_t res = ik_cmd_fork(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *child = repl->current;

    // Corrupt state: remove parent from array (simulates parent not found)
    // Save parent pointer first
    ik_agent_ctx_t *parent = repl->agents[0];
    const char *parent_uuid = parent->uuid;

    // Remove parent from array (corrupt state)
    for (size_t i = 0; i < repl->agent_count - 1; i++) {
        repl->agents[i] = repl->agents[i + 1];
    }
    repl->agent_count--;

    // Now child's parent_uuid points to non-existent agent
    // Try to kill child - should error
    res = ik_cmd_kill(test_ctx, repl, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(res.err->code, ERR_INVALID_ARG);

    // Cleanup
    talloc_free(res.err);

    // Restore parent for cleanup
    repl->agents[repl->agent_count] = parent;
    repl->agent_count++;

    (void)child;
    (void)parent_uuid;
}

END_TEST
// Test: Kill with UUID shows error when not found
START_TEST(test_kill_uuid_not_found_shows_error) {
    ik_agent_ctx_t *parent = repl->current;

    ik_scrollback_clear(parent->scrollback);

    // Try to kill with non-existent UUID
    res_t res = ik_cmd_kill(test_ctx, repl, "zzz");
    ck_assert(is_ok(&res));

    // Should show "Agent not found" error
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

static Suite *cmd_agent_coverage_suite(void)
{
    Suite *s = suite_create("Commands Agent Coverage");
    TCase *tc = tcase_create("Error Paths");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_fork_unterminated_quote_error);
    tcase_add_test(tc, test_kill_parent_not_found_error);
    tcase_add_test(tc, test_kill_uuid_not_found_shows_error);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    if (!suite_setup()) {
        fprintf(stderr, "Suite setup failed\n");
        return 1;
    }

    Suite *s = cmd_agent_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/cmd_agent_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
