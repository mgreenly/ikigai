/**
 * @file summary_prune_wiring_test.c
 * @brief Integration tests for prune → summary dispatch wiring
 *
 * Verifies that ik_agent_prune_token_cache() triggers background summary
 * dispatch when context_start_index advances past zero.
 */

#include <check.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/summary_worker.h"
#include "apps/ikigai/token_cache.h"
#include "shared/error.h"

/* ---- Mock provider ---- */

typedef struct {
    const char *response_text;
    bool should_fail;
} prune_mock_ctx_t;

static res_t prune_mock_fdset(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd)
{
    (void)ctx; (void)r; (void)w; (void)e;
    *max_fd = -1;
    return OK(NULL);
}

static res_t prune_mock_timeout(void *ctx, long *timeout_ms)
{
    (void)ctx;
    *timeout_ms = 0;
    return OK(NULL);
}

static res_t prune_mock_perform(void *ctx, int *running_handles)
{
    (void)ctx;
    *running_handles = 0;
    return OK(NULL);
}

static void prune_mock_info_read(void *ctx, ik_logger_t *logger)
{
    (void)ctx; (void)logger;
}

static res_t prune_mock_start_stream(void *ctx, const ik_request_t *req,
                                     ik_stream_cb_t scb, void *sctx,
                                     ik_provider_completion_cb_t ccb, void *cctx)
{
    (void)req; (void)scb; (void)sctx;
    prune_mock_ctx_t *m = ctx;

    if (m->should_fail) {
        char err_msg[] = "mock provider error";
        ik_provider_completion_t completion = {
            .success = false,
            .error_message = err_msg,
        };
        ccb(&completion, cctx);
        return OK(NULL);
    }

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

static res_t prune_mock_count_tokens(void *ctx, const ik_request_t *req, int32_t *out)
{
    (void)ctx; (void)req;
    *out = 0;
    return OK(NULL);
}

static const ik_provider_vtable_t prune_mock_vt = {
    .fdset = prune_mock_fdset,
    .timeout = prune_mock_timeout,
    .perform = prune_mock_perform,
    .info_read = prune_mock_info_read,
    .start_stream = prune_mock_start_stream,
    .count_tokens = prune_mock_count_tokens,
    .cleanup = NULL,
    .cancel = NULL,
};

/* Spin until summary_thread_complete is set (max 2 s with 1 ms sleep). */
static void prune_wait_for_complete(ik_agent_ctx_t *a)
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

/* ---- Tests ---- */

/*
 * Dispatch is called when prune advances context_start_index.
 *
 * Setup: 2 turns, budget below total, so turn 0 is pruned.
 * Expect: recent_summary_generation incremented (dispatch fired).
 */
START_TEST(test_prune_triggers_dispatch) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    /* Inject mock provider (bypasses ik_provider_create) */
    prune_mock_ctx_t mock = { .response_text = "Summary.", .should_fail = false };
    ik_provider_t *provider = talloc_zero(agent, ik_provider_t);
    provider->name = "mock";
    provider->vt = &prune_mock_vt;
    provider->ctx = &mock;
    agent->provider_instance = provider;

    /* Set provider and model (required by prune dispatch guard).
     * If ANTHROPIC_API_KEY is set, a real provider is created and the
     * thread makes a live API call; otherwise provider creation fails
     * after the generation increment, no thread runs, and cleanup is safe. */
    agent->provider = talloc_strdup(agent, "anthropic");
    agent->model = talloc_strdup(agent, "test-model");

    /* Set budget very low so pruning occurs */
    ik_token_cache_set_budget(agent->token_cache, 10);

    /* Turn 0: user + assistant */
    ik_message_t *u0 = ik_message_create_text(agent, IK_ROLE_USER, "Hello");
    ik_message_t *a0 = ik_message_create_text(agent, IK_ROLE_ASSISTANT, "Hi there");
    res_t r0 = ik_agent_add_message(agent, u0);
    ck_assert(is_ok(&r0));
    res_t r1 = ik_agent_add_message(agent, a0);
    ck_assert(is_ok(&r1));
    ik_token_cache_add_turn(agent->token_cache);
    ik_token_cache_record_turn(agent->token_cache, 0, 30);

    /* Turn 1: user + assistant */
    ik_message_t *u1 = ik_message_create_text(agent, IK_ROLE_USER, "What is 2+2?");
    ik_message_t *a1 = ik_message_create_text(agent, IK_ROLE_ASSISTANT, "4");
    res_t r2 = ik_agent_add_message(agent, u1);
    ck_assert(is_ok(&r2));
    res_t r3 = ik_agent_add_message(agent, a1);
    ck_assert(is_ok(&r3));
    ik_token_cache_add_turn(agent->token_cache);
    ik_token_cache_record_turn(agent->token_cache, 1, 30);

    /* Total is 60 > budget 10, turn_count is 2 > 1: pruning will occur */
    uint32_t gen_before = agent->recent_summary_generation;

    ik_agent_prune_token_cache(agent);

    /* Generation must have been incremented (dispatch fired) */
    ck_assert_uint_gt(agent->recent_summary_generation, gen_before);

    /* Wait for thread and clean up */
    prune_wait_for_complete(agent);
    ik_summary_worker_poll(agent);

    talloc_free(ctx);
}
END_TEST

/*
 * No dispatch when nothing is pruned (total within budget).
 */
START_TEST(test_prune_no_dispatch_within_budget) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    prune_mock_ctx_t mock = { .response_text = "Summary.", .should_fail = false };
    ik_provider_t *provider = talloc_zero(agent, ik_provider_t);
    provider->name = "mock";
    provider->vt = &prune_mock_vt;
    provider->ctx = &mock;
    agent->provider_instance = provider;
    agent->model = talloc_strdup(agent, "test-model");

    /* Budget is 100000 (default) — well above any recorded tokens */
    ik_message_t *u0 = ik_message_create_text(agent, IK_ROLE_USER, "Hello");
    ik_message_t *a0 = ik_message_create_text(agent, IK_ROLE_ASSISTANT, "Hi");
    res_t s0 = ik_agent_add_message(agent, u0);
    ck_assert(is_ok(&s0));
    res_t s1 = ik_agent_add_message(agent, a0);
    ck_assert(is_ok(&s1));
    ik_token_cache_add_turn(agent->token_cache);
    ik_token_cache_record_turn(agent->token_cache, 0, 5);

    ik_message_t *u1 = ik_message_create_text(agent, IK_ROLE_USER, "Bye");
    ik_message_t *a1 = ik_message_create_text(agent, IK_ROLE_ASSISTANT, "Bye");
    res_t s2 = ik_agent_add_message(agent, u1);
    ck_assert(is_ok(&s2));
    res_t s3 = ik_agent_add_message(agent, a1);
    ck_assert(is_ok(&s3));
    ik_token_cache_add_turn(agent->token_cache);
    ik_token_cache_record_turn(agent->token_cache, 1, 5);

    uint32_t gen_before = agent->recent_summary_generation;

    ik_agent_prune_token_cache(agent);

    /* Generation must NOT have changed (no pruning occurred) */
    ck_assert_uint_eq(agent->recent_summary_generation, gen_before);

    talloc_free(ctx);
}
END_TEST

static Suite *summary_prune_wiring_suite(void)
{
    Suite *s = suite_create("summary_prune_wiring");

    TCase *tc = tcase_create("PruneDispatch");
    tcase_set_timeout(tc, 10);
    tcase_add_test(tc, test_prune_triggers_dispatch);
    tcase_add_test(tc, test_prune_no_dispatch_within_budget);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = summary_prune_wiring_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/apps/ikigai/summary_prune_wiring_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
