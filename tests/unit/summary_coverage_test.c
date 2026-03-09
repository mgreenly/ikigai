#include "tests/test_constants.h"
#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/summary.h"
#include "shared/error.h"

/* ----------------------------------------------------------------
 * Minimal mock vtable (shared by all tests in this file)
 * ---------------------------------------------------------------- */

static res_t base_fdset(void *ctx, fd_set *r, fd_set *w, fd_set *e, int *max_fd)
{
    (void)ctx; (void)r; (void)w; (void)e;
    *max_fd = -1;
    return OK(NULL);
}

static res_t base_timeout(void *ctx, long *timeout_ms)
{
    (void)ctx; *timeout_ms = 0;
    return OK(NULL);
}

static res_t base_perform(void *ctx, int *running_handles)
{
    (void)ctx; *running_handles = 0;
    return OK(NULL);
}

static void base_info_read(void *ctx, ik_logger_t *logger)
{
    (void)ctx; (void)logger;
}

static res_t base_start_request(void *ctx, const ik_request_t *req,
                                 ik_provider_completion_cb_t cb, void *cb_ctx)
{
    (void)ctx; (void)req; (void)cb; (void)cb_ctx;
    return OK(NULL);
}

static res_t base_start_stream(void *ctx, const ik_request_t *req,
                                ik_stream_cb_t scb, void *sctx,
                                ik_provider_completion_cb_t ccb, void *cctx)
{
    (void)ctx; (void)req; (void)scb; (void)sctx; (void)ccb; (void)cctx;
    return ERR(NULL, PROVIDER, "not supported");
}

static res_t base_count_tokens(void *ctx, const ik_request_t *req, int32_t *out)
{
    (void)ctx; (void)req; *out = 0;
    return OK(NULL);
}

static const ik_provider_vtable_t base_vt = {
    .fdset = base_fdset, .timeout = base_timeout,
    .perform = base_perform, .info_read = base_info_read,
    .start_request = base_start_request, .start_stream = base_start_stream,
    .count_tokens = base_count_tokens, .cleanup = NULL, .cancel = NULL,
};

/* ----------------------------------------------------------------
 * Mock implementations
 * ---------------------------------------------------------------- */

static res_t mock_null_response(void *ctx, const ik_request_t *req,
                                ik_provider_completion_cb_t cb, void *cb_ctx)
{
    (void)ctx; (void)req;
    ik_provider_completion_t c = { .success = true, .response = NULL };
    cb(&c, cb_ctx);
    return OK(NULL);
}

static res_t mock_no_text_block(void *ctx, const ik_request_t *req,
                                ik_provider_completion_cb_t cb, void *cb_ctx)
{
    (void)ctx; (void)req;
    TALLOC_CTX *tmp = talloc_new(NULL);
    ik_response_t *resp = talloc_zero(tmp, ik_response_t);
    resp->content_count = 0;
    ik_provider_completion_t c = { .success = true, .response = resp };
    cb(&c, cb_ctx);
    talloc_free(tmp);
    return OK(NULL);
}

static res_t mock_non_text_block(void *ctx, const ik_request_t *req,
                                  ik_provider_completion_cb_t cb, void *cb_ctx)
{
    (void)ctx; (void)req;
    TALLOC_CTX *tmp = talloc_new(NULL);
    ik_response_t *resp = talloc_zero(tmp, ik_response_t);
    ik_content_block_t *block = talloc_zero(tmp, ik_content_block_t);
    block->type = IK_CONTENT_TOOL_CALL;
    resp->content_blocks = block;
    resp->content_count = 1;
    ik_provider_completion_t c = { .success = true, .response = resp };
    cb(&c, cb_ctx);
    talloc_free(tmp);
    return OK(NULL);
}

static int g_deferred_count = 0;

static res_t mock_deferred_start(void *ctx, const ik_request_t *req,
                                  ik_provider_completion_cb_t cb, void *cb_ctx)
{
    (void)ctx; (void)req; (void)cb; (void)cb_ctx;
    return OK(NULL);
}

static res_t mock_deferred_perform(void *ctx, int *running_handles)
{
    (void)ctx;
    g_deferred_count++;
    *running_handles = 0;
    return OK(NULL);
}

typedef struct { const char *text; } text_ctx_t;

static res_t mock_text_response(void *ctx, const ik_request_t *req,
                                 ik_provider_completion_cb_t cb, void *cb_ctx)
{
    (void)req;
    text_ctx_t *tc = ctx;
    TALLOC_CTX *tmp = talloc_new(NULL);
    ik_response_t *resp = talloc_zero(tmp, ik_response_t);
    ik_content_block_t *block = talloc_zero(tmp, ik_content_block_t);
    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(tmp, tc->text);
    resp->content_blocks = block;
    resp->content_count = 1;
    ik_provider_completion_t c = { .success = true, .response = resp };
    cb(&c, cb_ctx);
    talloc_free(tmp);
    return OK(NULL);
}

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static ik_msg_t *make_user_msg(TALLOC_CTX *ctx)
{
    ik_msg_t *m = talloc_zero(ctx, ik_msg_t);
    m->kind = talloc_strdup(m, "user");
    m->content = talloc_strdup(m, "hi");
    return m;
}

START_TEST(test_null_response_gives_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msg = make_user_msg(ctx);
    ik_msg_t *msgs[] = { msg };
    ik_provider_vtable_t vt = base_vt;
    vt.start_request = mock_null_response;
    ik_provider_t prov = { .name = "mock", .vt = &vt, .ctx = NULL };
    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &prov, "m", 1000, &summary);
    ck_assert(is_err(&res));
    talloc_free(res.err);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_no_text_block_gives_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msg = make_user_msg(ctx);
    ik_msg_t *msgs[] = { msg };
    ik_provider_vtable_t vt = base_vt;
    vt.start_request = mock_no_text_block;
    ik_provider_t prov = { .name = "mock", .vt = &vt, .ctx = NULL };
    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &prov, "m", 1000, &summary);
    ck_assert(is_err(&res));
    talloc_free(res.err);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_non_text_block_gives_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msg = make_user_msg(ctx);
    ik_msg_t *msgs[] = { msg };
    ik_provider_vtable_t vt = base_vt;
    vt.start_request = mock_non_text_block;
    ik_provider_t prov = { .name = "mock", .vt = &vt, .ctx = NULL };
    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &prov, "m", 1000, &summary);
    ck_assert(is_err(&res));
    talloc_free(res.err);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_loop_body_via_deferred_callback) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msg = make_user_msg(ctx);
    ik_msg_t *msgs[] = { msg };
    ik_provider_vtable_t vt = base_vt;
    vt.start_request = mock_deferred_start;
    vt.perform = mock_deferred_perform;
    g_deferred_count = 0;
    ik_provider_t prov = { .name = "mock", .vt = &vt, .ctx = NULL };
    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &prov, "m", 1000, &summary);
    ck_assert_int_ge(g_deferred_count, 1);
    ck_assert(is_err(&res));
    talloc_free(res.err);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_truncate_no_sentence_boundary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msg = make_user_msg(ctx);
    ik_msg_t *msgs[] = { msg };
    text_ctx_t tc = { .text = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    ik_provider_vtable_t vt = base_vt;
    vt.start_request = mock_text_response;
    ik_provider_t prov = { .name = "mock", .vt = &vt, .ctx = &tc };
    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &prov, "m", 5, &summary);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(strlen(summary), 20);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_sentence_boundary_at_tab) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msg = make_user_msg(ctx);
    ik_msg_t *msgs[] = { msg };
    text_ctx_t tc = { .text = "First sentence.\tSecond sentence that is quite long indeed." };
    ik_provider_vtable_t vt = base_vt;
    vt.start_request = mock_text_response;
    ik_provider_t prov = { .name = "mock", .vt = &vt, .ctx = &tc };
    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &prov, "m", 5, &summary);
    ck_assert(is_ok(&res));
    ck_assert_str_eq(summary, "First sentence.");
    talloc_free(ctx);
}
END_TEST

/* ----------------------------------------------------------------
 * Suite
 * ---------------------------------------------------------------- */

static Suite *summary_coverage_suite(void)
{
    Suite *s = suite_create("summary_coverage");

    TCase *tc = tcase_create("coverage");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_test(tc, test_null_response_gives_error);
    tcase_add_test(tc, test_no_text_block_gives_error);
    tcase_add_test(tc, test_non_text_block_gives_error);
    tcase_add_test(tc, test_loop_body_via_deferred_callback);
    tcase_add_test(tc, test_truncate_no_sentence_boundary);
    tcase_add_test(tc, test_sentence_boundary_at_tab);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = summary_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/summary_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
