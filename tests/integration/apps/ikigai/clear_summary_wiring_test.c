/**
 * @file clear_summary_wiring_test.c
 * @brief Integration tests for /clear → session summary generation
 *
 * Verifies that ik_cmd_clear() generates and stores a session summary when
 * conversation messages exist, and skips generation on an empty epoch.
 */

#include <check.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>
#include <time.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands_basic.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/agent.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/summary.h"
#include "apps/ikigai/summary_worker.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "tests/helpers/test_utils_helper.h"

/* ---- Mock provider ---- */

typedef struct {
    const char *response_text;
} clear_mock_ctx_t;

static res_t clear_mock_fdset(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd)
{
    (void)ctx; (void)r; (void)w; (void)e;
    *max_fd = -1;
    return OK(NULL);
}

static res_t clear_mock_timeout(void *ctx, long *timeout_ms)
{
    (void)ctx;
    *timeout_ms = 0;
    return OK(NULL);
}

static res_t clear_mock_perform(void *ctx, int *running_handles)
{
    (void)ctx;
    *running_handles = 0;
    return OK(NULL);
}

static void clear_mock_info_read(void *ctx, ik_logger_t *logger)
{
    (void)ctx; (void)logger;
}

static res_t clear_mock_start_stream(void *ctx, const ik_request_t *req,
                                     ik_stream_cb_t scb, void *sctx,
                                     ik_provider_completion_cb_t ccb, void *cctx)
{
    (void)req; (void)scb; (void)sctx;
    clear_mock_ctx_t *m = ctx;

    TALLOC_CTX *tmp = talloc_new(NULL);
    ik_response_t *resp = talloc_zero(tmp, ik_response_t);
    ik_content_block_t *b = talloc_zero(tmp, ik_content_block_t);
    b->type = IK_CONTENT_TEXT;
    b->data.text.text = talloc_strdup(tmp, m->response_text);
    resp->content_blocks = b;
    resp->content_count = 1;

    ik_provider_completion_t completion = { .success = true, .response = resp };
    ccb(&completion, cctx);

    talloc_free(tmp);
    return OK(NULL);
}

static res_t clear_mock_count_tokens(void *ctx, const ik_request_t *req, int32_t *out)
{
    (void)ctx; (void)req;
    *out = 0;
    return OK(NULL);
}

static const ik_provider_vtable_t clear_mock_vt = {
    .fdset = clear_mock_fdset,
    .timeout = clear_mock_timeout,
    .perform = clear_mock_perform,
    .info_read = clear_mock_info_read,
    .start_stream = clear_mock_start_stream,
    .count_tokens = clear_mock_count_tokens,
    .cleanup = NULL,
    .cancel = NULL,
};

/* Spin until summary_thread_complete is set (max 2 s). */
static void clear_wait_for_complete(ik_agent_ctx_t *a)
{
    for (int i = 0; i < 2000; i++) {
        pthread_mutex_lock(&a->summary_thread_mutex);
        bool done = a->summary_thread_complete;
        pthread_mutex_unlock(&a->summary_thread_mutex);
        if (done) return;
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
        nanosleep(&ts, NULL);
    }
}

/*
 * Spawn the session summary worker thread directly with a borrowed mock
 * provider, bypassing ik_session_summary_dispatch (which calls
 * ik_provider_create).  Builds malloc stubs from the given msgs so the
 * thread owns its inputs.  Sets is_session_summary=true so poll() stores
 * the result to DB.
 */
static void session_dispatch_with_mock(ik_agent_ctx_t *agent,
                                       ik_provider_t *provider,
                                       ik_db_ctx_t *db_ctx,
                                       int64_t start_msg_id,
                                       int64_t end_msg_id)
{
    /* Build two-message stubs for the session summary (user + assistant). */
    const char *kinds[]    = { "user",          "assistant" };
    const char *contents[] = { "Hello, world",  "Hi there!" };
    size_t n = 2;

    ik_msg_t *stubs = malloc(n * sizeof(ik_msg_t));
    ik_msg_t **ptrs  = malloc(n * sizeof(ik_msg_t *));
    ck_assert_ptr_nonnull(stubs);
    ck_assert_ptr_nonnull(ptrs);
    for (size_t i = 0; i < n; i++) {
        stubs[i].id          = (int64_t)(i + 1);
        stubs[i].kind        = strdup(kinds[i]);
        stubs[i].content     = strdup(contents[i]);
        stubs[i].data_json   = NULL;
        stubs[i].interrupted = false;
        ptrs[i] = &stubs[i];
    }

    ik_summary_worker_args_t *args = malloc(sizeof(ik_summary_worker_args_t));
    ck_assert_ptr_nonnull(args);

    args->msgs              = (ik_msg_t * const *)ptrs;
    args->msg_count         = n;
    args->provider          = provider;
    args->owns_provider     = false; /* borrowed mock; poll() must not free it */
    args->model             = strdup("test-model");
    args->max_tokens        = IK_SUMMARY_PREVIOUS_SESSION_MAX_TOKENS;
    args->agent             = agent;
    args->generation        = 0;
    args->is_session_summary = true;
    args->start_msg_id      = start_msg_id;
    args->end_msg_id        = end_msg_id;
    args->session_msgs_stubs = stubs;
    args->session_msgs_ptrs  = ptrs;

    /* Wire up agent->shared->db_ctx so poll() can store to DB. */
    agent->shared->db_ctx = db_ctx;

    pthread_mutex_lock(&agent->summary_thread_mutex);
    agent->summary_thread_running  = true;
    agent->summary_thread_complete = false;
    agent->summary_thread_result   = NULL;
    pthread_mutex_unlock(&agent->summary_thread_mutex);

    int ret = pthread_create(&agent->summary_thread, NULL,
                             ik_summary_worker_fn, args);
    if (ret != 0) {
        pthread_mutex_lock(&agent->summary_thread_mutex);
        agent->summary_thread_running = false;
        pthread_mutex_unlock(&agent->summary_thread_mutex);
        for (size_t i = 0; i < n; i++) { free(stubs[i].kind); free(stubs[i].content); }
        free(stubs); free(ptrs); free(args->model); free(args);
    }
}

/* ---- DB / test setup ---- */

static const char *DB_NAME;
static bool db_available = false;

static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;
static int64_t session_id;

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
    }
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

/*
 * Build a minimal repl context backed by the test DB.
 * The agent is inserted into the DB so FK constraints are satisfied.
 */
static ik_repl_ctx_t *build_repl(void)
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

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(repl, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 4);
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->agent_capacity = 4;
    repl->current = agent;

    /* Insert agent row so FK constraints are satisfied for message inserts */
    res = ik_db_agent_insert(db, agent);
    ck_assert(is_ok(&res));

    return repl;
}

/* ---- Tests ---- */

/*
 * Session summary dispatch+poll must insert a row into session_summaries and
 * populate agent->session_summaries[].  Tests the async path directly using
 * a mock provider so no real API call is needed.
 *
 * This mirrors what /clear triggers but isolates dispatch+poll from the
 * /clear command so the test works regardless of provider credentials.
 */
START_TEST(test_clear_generates_session_summary) {
    SKIP_IF_NO_DB();

    ik_repl_ctx_t *repl = build_repl();
    ik_agent_ctx_t *agent = repl->current;

    /* Insert two epoch messages so the session summary has something to store. */
    res_t res = ik_db_message_insert(db, session_id, agent->uuid, "user", "Hello, world", "{}");
    ck_assert(is_ok(&res));

    res = ik_db_message_insert(db, session_id, agent->uuid, "assistant", "Hi there!", "{}");
    ck_assert(is_ok(&res));

    /* Fake start/end message IDs (the stubs in session_dispatch_with_mock use
     * index-based IDs; use 1 and 2 as stable values for the DB insert). */
    int64_t start_id = 1;
    int64_t end_id   = 2;

    /* Use a mock provider that instantly returns the summary text. */
    clear_mock_ctx_t mock = { .response_text = "Summary of the conversation." };
    ik_provider_t prov = {
        .name = "mock",
        .vt   = &clear_mock_vt,
        .ctx  = &mock,
    };

    /* Dispatch the background session summary thread with the mock. */
    session_dispatch_with_mock(agent, &prov, db, start_id, end_id);

    /* Wait for the thread to complete (mock is synchronous — finishes fast). */
    clear_wait_for_complete(agent);
    ck_assert(agent->summary_thread_complete);

    /* Poll: stores result to DB and reloads agent->session_summaries. */
    ik_summary_worker_poll(agent);

    /* A session_summaries row must have been inserted. */
    ik_session_summary_t **summaries = NULL;
    size_t count = 0;
    res = ik_db_session_summary_load(db, test_ctx, agent->uuid, &summaries, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 1);
    ck_assert_str_eq(summaries[0]->summary, "Summary of the conversation.");

    /* agent->session_summaries must also be populated. */
    ck_assert_int_eq((int)agent->session_summary_count, 1);
    ck_assert_str_eq(agent->session_summaries[0]->summary, "Summary of the conversation.");
}
END_TEST

/*
 * /clear on an empty epoch (no user/assistant messages) must skip
 * summary generation — session_summaries stays empty.
 */
START_TEST(test_clear_empty_epoch_skips_summary) {
    SKIP_IF_NO_DB();

    ik_repl_ctx_t *repl = build_repl();
    ik_agent_ctx_t *agent = repl->current;

    /* Inject mock provider (must NOT be invoked) */
    clear_mock_ctx_t mock = { .response_text = "Should not be generated." };
    ik_provider_t *prov = talloc_zero(agent, ik_provider_t);
    prov->name = "mock";
    prov->vt = &clear_mock_vt;
    prov->ctx = &mock;
    agent->provider_instance = prov;
    agent->model = talloc_strdup(agent, "test-model");

    /* No conversation messages — epoch is empty */

    res_t res = ik_cmd_clear(test_ctx, repl, NULL);
    ck_assert(is_ok(&res));

    /* session_summaries must remain empty */
    ik_session_summary_t **summaries = NULL;
    size_t count = 0;
    res = ik_db_session_summary_load(db, test_ctx, agent->uuid, &summaries, &count);
    ck_assert(is_ok(&res));
    ck_assert_int_eq((int)count, 0);
}
END_TEST

/* ---- Suite ---- */

static Suite *clear_summary_wiring_suite(void)
{
    Suite *s = suite_create("clear_summary_wiring");
    TCase *tc = tcase_create("ClearFlow");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);

    tcase_add_test(tc, test_clear_generates_session_summary);
    tcase_add_test(tc, test_clear_empty_epoch_skips_summary);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = clear_summary_wiring_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
                    "reports/check/integration/apps/ikigai/clear_summary_wiring_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
