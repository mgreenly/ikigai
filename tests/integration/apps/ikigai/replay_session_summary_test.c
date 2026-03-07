/**
 * @file replay_session_summary_test.c
 * @brief Integration tests for session summary loading during agent restore
 *
 * Verifies that ik_repl_restore_agents() populates agent->session_summaries[]
 * from the database when session summaries exist, and leaves it empty when none exist.
 */

#include "apps/ikigai/repl/agent_restore.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

/* ---- DB / test setup ---- */

static const char *DB_NAME;
static bool db_available = false;

static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;
static ik_shared_ctx_t shared_ctx;

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

static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

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

    shared_ctx.session_id = session_id;
}

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

/* ---- Helpers ---- */

static void insert_agent(const char *uuid, const char *parent_uuid,
                         int64_t created_at, int64_t fork_message_id)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = parent_uuid ? talloc_strdup(test_ctx, parent_uuid) : NULL;
    agent.created_at = created_at;
    agent.fork_message_id = fork_message_id;
    agent.shared = &shared_ctx;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

static int64_t insert_msg_id(const char *uuid, const char *kind, const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
    int64_t msg_id = 0;
    res = ik_db_agent_get_last_message_id(db, uuid, &msg_id);
    ck_assert(is_ok(&res));
    return msg_id;
}

static ik_repl_ctx_t *create_test_repl(const char *agent0_uuid)
{
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = db;
    shared->session_id = session_id;
    shared->logger = ik_logger_create(shared, "/tmp");
    ck_assert_ptr_nonnull(shared->logger);
    shared->cfg = ik_test_create_config(shared);

    repl->shared = shared;

    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
    repl->agent_capacity = 16;

    ik_agent_ctx_t *agent0 = NULL;
    res_t res = ik_agent_create(repl, shared, NULL, &agent0);
    ck_assert(is_ok(&res));

    talloc_free(agent0->uuid);
    agent0->uuid = talloc_strdup(agent0, agent0_uuid);

    repl->agents[0] = agent0;
    repl->agent_count = 1;
    repl->current = agent0;

    return repl;
}

/* ---- Tests ---- */

/*
 * After restore, agent->session_summaries[] must be populated from DB.
 */
START_TEST(test_restore_loads_session_summaries)
{
    SKIP_IF_NO_DB();

    const char *uuid = "restore-summary-agent";
    insert_agent(uuid, NULL, 1000, 0);

    /* Insert two messages to establish valid start/end IDs */
    int64_t start_id = insert_msg_id(uuid, "clear", NULL);
    int64_t end_id   = insert_msg_id(uuid, "user", "Hello");

    /* Insert a session summary for the agent */
    res_t res = ik_db_session_summary_insert(db, uuid,
                                             "Previous session summary.",
                                             start_id, end_id, 42);
    ck_assert(is_ok(&res));

    /* Restore agents — triggers restore_agent_zero → ik_db_session_summary_load */
    ik_repl_ctx_t *repl = create_test_repl(uuid);
    res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *agent = repl->current;
    ck_assert_int_eq((int)agent->session_summary_count, 1);
    ck_assert_ptr_nonnull(agent->session_summaries);
    ck_assert_str_eq(agent->session_summaries[0]->summary, "Previous session summary.");
    ck_assert_int_eq(agent->session_summaries[0]->token_count, 42);
}
END_TEST

/*
 * After restore with no summaries in DB, agent->session_summaries must be NULL
 * and session_summary_count must be 0.
 */
START_TEST(test_restore_no_summaries_leaves_empty)
{
    SKIP_IF_NO_DB();

    const char *uuid = "restore-no-summary-agent";
    insert_agent(uuid, NULL, 1000, 0);
    insert_msg_id(uuid, "clear", NULL);

    ik_repl_ctx_t *repl = create_test_repl(uuid);
    res_t res = ik_repl_restore_agents(repl, db);
    ck_assert(is_ok(&res));

    ik_agent_ctx_t *agent = repl->current;
    ck_assert_int_eq((int)agent->session_summary_count, 0);
    ck_assert_ptr_null(agent->session_summaries);
}
END_TEST

/* ---- Suite ---- */

static Suite *replay_session_summary_suite(void)
{
    Suite *s  = suite_create("replay_session_summary");
    TCase *tc = tcase_create("ReplayLoad");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);

    tcase_add_test(tc, test_restore_loads_session_summaries);
    tcase_add_test(tc, test_restore_no_summaries_leaves_empty);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite   *s  = replay_session_summary_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/integration/apps/ikigai/replay_session_summary_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
