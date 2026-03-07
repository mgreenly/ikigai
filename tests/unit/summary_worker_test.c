#include <check.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/summary_worker.h"
#include "shared/error.h"

/*
 * The agent must be talloc-allocated because ik_summary_worker_poll() uses
 * the agent pointer as a talloc parent for talloc_steal() when accepting a
 * summary result.
 */
static ik_agent_ctx_t *agent_create(void)
{
    ik_agent_ctx_t *a = talloc_zero(NULL, ik_agent_ctx_t);
    pthread_mutex_init(&a->summary_thread_mutex, NULL);
    return a;
}

static void agent_destroy(ik_agent_ctx_t *a)
{
    pthread_mutex_destroy(&a->summary_thread_mutex);
    if (a->summary_thread_result != NULL) {
        talloc_free(a->summary_thread_result);
        a->summary_thread_result = NULL;
    }
    talloc_free(a); /* frees agent and all talloc children (recent_summary, etc.) */
}

/* ---- Mock provider ---- */

typedef struct {
    const char *response_text;
    bool        should_fail;
} mock_ctx_t;

static res_t mock_fdset(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd)
{
    (void)ctx; (void)r; (void)w; (void)e;
    *max_fd = -1;
    return OK(NULL);
}

static res_t mock_timeout(void *ctx, long *timeout_ms)
{
    (void)ctx;
    *timeout_ms = 0;
    return OK(NULL);
}

static res_t mock_perform(void *ctx, int *running_handles)
{
    (void)ctx;
    *running_handles = 0;
    return OK(NULL);
}

static void mock_info_read(void *ctx, ik_logger_t *logger)
{
    (void)ctx; (void)logger;
}

static res_t mock_start_request(void *ctx, const ik_request_t *req,
                                ik_provider_completion_cb_t cb, void *cb_ctx)
{
    mock_ctx_t *m = ctx;
    (void)req;

    if (m->should_fail) {
        char err_msg[] = "mock provider error";
        ik_provider_completion_t completion = {
            .success       = false,
            .error_message = err_msg,
        };
        cb(&completion, cb_ctx);
        return OK(NULL);
    }

    TALLOC_CTX *tmp       = talloc_new(NULL);
    ik_response_t *resp   = talloc_zero(tmp, ik_response_t);
    ik_content_block_t *b = talloc_zero(tmp, ik_content_block_t);
    b->type               = IK_CONTENT_TEXT;
    b->data.text.text     = talloc_strdup(tmp, m->response_text);
    resp->content_blocks  = b;
    resp->content_count   = 1;

    ik_provider_completion_t completion = { .success = true, .response = resp };
    cb(&completion, cb_ctx);

    talloc_free(tmp);
    return OK(NULL);
}

static res_t mock_start_stream(void *ctx, const ik_request_t *req,
                               ik_stream_cb_t scb, void *sctx,
                               ik_provider_completion_cb_t ccb, void *cctx)
{
    (void)ctx; (void)req; (void)scb; (void)sctx; (void)ccb; (void)cctx;
    return ERR(NULL, PROVIDER, "mock does not support streaming");
}

static res_t mock_count_tokens(void *ctx, const ik_request_t *req, int32_t *out)
{
    (void)ctx; (void)req;
    *out = 0;
    return OK(NULL);
}

static const ik_provider_vtable_t mock_vt = {
    .fdset         = mock_fdset,
    .timeout       = mock_timeout,
    .perform       = mock_perform,
    .info_read     = mock_info_read,
    .start_request = mock_start_request,
    .start_stream  = mock_start_stream,
    .count_tokens  = mock_count_tokens,
    .cleanup       = NULL,
    .cancel        = NULL,
};

/* Spin until summary_thread_complete is set (max 2 s with 1 ms sleep). */
static void wait_for_complete(ik_agent_ctx_t *a)
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

/* ---- Worker: result written and complete flag set on success ---- */

START_TEST(test_worker_fn_success)
{
    ik_agent_ctx_t *agent = agent_create();

    char ku[] = "user";
    char cu[] = "hello";
    ik_msg_t msg = { .id = 1, .kind = ku, .content = cu,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };

    mock_ctx_t mock = { .response_text = "Summary text.", .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };
    char model[] = "test-model";

    ik_summary_worker_args_t args = {
        .msgs       = (ik_msg_t * const *)msgs,
        .msg_count  = 1,
        .provider   = &provider,
        .model      = model,
        .max_tokens = 1000,
        .agent      = agent,
        .generation = 7,
    };

    void *ret = ik_summary_worker_fn(&args);
    ck_assert_ptr_eq(ret, &args); /* returns args pointer for poll to free */

    ck_assert(agent->summary_thread_complete);
    ck_assert_uint_eq(agent->summary_thread_generation, 7);
    ck_assert_ptr_nonnull(agent->summary_thread_result);
    ck_assert_ptr_nonnull(strstr(agent->summary_thread_result, "Summary text."));

    agent_destroy(agent);
}
END_TEST

/* ---- Worker: complete flag set on provider failure; result is NULL ---- */

START_TEST(test_worker_fn_failure)
{
    ik_agent_ctx_t *agent = agent_create();

    char ku[] = "user";
    char cu[] = "hello";
    ik_msg_t msg = { .id = 1, .kind = ku, .content = cu,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };

    mock_ctx_t mock = { .response_text = NULL, .should_fail = true };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };
    char model[] = "test-model";

    ik_summary_worker_args_t args = {
        .msgs       = (ik_msg_t * const *)msgs,
        .msg_count  = 1,
        .provider   = &provider,
        .model      = model,
        .max_tokens = 1000,
        .agent      = agent,
        .generation = 2,
    };

    ik_summary_worker_fn(&args);

    ck_assert(agent->summary_thread_complete);
    ck_assert_uint_eq(agent->summary_thread_generation, 2);
    ck_assert_ptr_null(agent->summary_thread_result);

    agent_destroy(agent);
}
END_TEST

/* ---- Dispatch: second dispatch while running is skipped ---- */

START_TEST(test_dispatch_skips_when_running)
{
    ik_agent_ctx_t *agent = agent_create();

    /* Mark thread as running to simulate an in-flight dispatch */
    pthread_mutex_lock(&agent->summary_thread_mutex);
    agent->summary_thread_running = true;
    pthread_mutex_unlock(&agent->summary_thread_mutex);

    uint32_t gen_before = agent->recent_summary_generation;

    mock_ctx_t mock = { .response_text = "x", .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };
    char ku[] = "user";
    char cu[] = "hi";
    ik_msg_t msg = { .id = 1, .kind = ku, .content = cu,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };

    ik_summary_worker_dispatch(agent, &provider, "model",
                               (ik_msg_t * const *)msgs, 1, 100);

    /* Generation must NOT have been incremented (dispatch skipped) */
    ck_assert_uint_eq(agent->recent_summary_generation, gen_before);
    ck_assert(agent->summary_thread_running);

    /* Reset running so agent_destroy doesn't encounter a dangling thread */
    pthread_mutex_lock(&agent->summary_thread_mutex);
    agent->summary_thread_running = false;
    pthread_mutex_unlock(&agent->summary_thread_mutex);

    agent_destroy(agent);
}
END_TEST

/* ---- Poll: matching generation accepts result ---- */

START_TEST(test_poll_accepts_matching_generation)
{
    ik_agent_ctx_t *agent = agent_create();

    char ku[] = "user";
    char cu[] = "question";
    ik_msg_t msg = { .id = 1, .kind = ku, .content = cu,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };

    mock_ctx_t mock = { .response_text = "Real summary.", .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    ik_summary_worker_dispatch(agent, &provider, "model",
                               (ik_msg_t * const *)msgs, 1, 1000);

    wait_for_complete(agent);
    ck_assert(agent->summary_thread_complete);

    ik_summary_worker_poll(agent);

    ck_assert_ptr_nonnull(agent->recent_summary);
    ck_assert_ptr_nonnull(strstr(agent->recent_summary, "Real summary."));
    ck_assert_int_gt(agent->recent_summary_tokens, 0);
    ck_assert(!agent->summary_thread_running);
    ck_assert(!agent->summary_thread_complete);
    ck_assert_ptr_null(agent->summary_thread_result);

    agent_destroy(agent);
}
END_TEST

/* ---- Poll: stale generation discards result ---- */

START_TEST(test_poll_stale_generation_discards)
{
    ik_agent_ctx_t *agent = agent_create();

    char ku[] = "user";
    char cu[] = "question";
    ik_msg_t msg = { .id = 1, .kind = ku, .content = cu,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };

    mock_ctx_t mock = { .response_text = "Stale summary.", .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    /* Dispatch increments generation to 1 */
    ik_summary_worker_dispatch(agent, &provider, "model",
                               (ik_msg_t * const *)msgs, 1, 1000);

    wait_for_complete(agent);

    /* Simulate a newer prune that bumped recent_summary_generation */
    agent->recent_summary_generation = 99;

    ik_summary_worker_poll(agent);

    /* Result must have been discarded; recent_summary stays NULL */
    ck_assert_ptr_null(agent->recent_summary);
    ck_assert(!agent->summary_thread_running);
    ck_assert(!agent->summary_thread_complete);

    agent_destroy(agent);
}
END_TEST

/* ---- Poll: LLM failure keeps previous recent_summary ---- */

START_TEST(test_poll_failure_keeps_previous_summary)
{
    ik_agent_ctx_t *agent = agent_create();

    /* Pre-populate an existing summary owned by the agent */
    agent->recent_summary = talloc_strdup(agent, "Previous summary.");
    agent->recent_summary_tokens = 42;

    char ku[] = "user";
    char cu[] = "question";
    ik_msg_t msg = { .id = 1, .kind = ku, .content = cu,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };

    mock_ctx_t mock = { .response_text = NULL, .should_fail = true };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    ik_summary_worker_dispatch(agent, &provider, "model",
                               (ik_msg_t * const *)msgs, 1, 1000);

    wait_for_complete(agent);

    ik_summary_worker_poll(agent);

    /* Previous summary must be intact */
    ck_assert_ptr_nonnull(agent->recent_summary);
    ck_assert_str_eq(agent->recent_summary, "Previous summary.");
    ck_assert_int_eq(agent->recent_summary_tokens, 42);

    agent_destroy(agent);
}
END_TEST

static Suite *summary_worker_suite(void)
{
    Suite *s = suite_create("summary_worker");

    TCase *tc_worker = tcase_create("Worker");
    tcase_add_test(tc_worker, test_worker_fn_success);
    tcase_add_test(tc_worker, test_worker_fn_failure);
    suite_add_tcase(s, tc_worker);

    TCase *tc_dispatch = tcase_create("Dispatch");
    tcase_add_test(tc_dispatch, test_dispatch_skips_when_running);
    suite_add_tcase(s, tc_dispatch);

    TCase *tc_poll = tcase_create("Poll");
    tcase_add_test(tc_poll, test_poll_accepts_matching_generation);
    tcase_add_test(tc_poll, test_poll_stale_generation_discards);
    tcase_add_test(tc_poll, test_poll_failure_keeps_previous_summary);
    suite_add_tcase(s, tc_poll);

    return s;
}

int32_t main(void)
{
    Suite *s = summary_worker_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/summary_worker_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
