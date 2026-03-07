#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/summary.h"
#include "shared/error.h"

/* ---- Boundaries tests ---- */

START_TEST(test_boundaries_zero_context_start) {
    ik_summary_range_t r = ik_summary_boundaries(10, 0);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 0);
}
END_TEST

START_TEST(test_boundaries_context_start_one) {
    ik_summary_range_t r = ik_summary_boundaries(5, 1);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 1);
}
END_TEST

START_TEST(test_boundaries_context_start_equals_count) {
    ik_summary_range_t r = ik_summary_boundaries(5, 5);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 5);
}
END_TEST

START_TEST(test_boundaries_zero_count_zero_index) {
    ik_summary_range_t r = ik_summary_boundaries(0, 0);
    ck_assert_uint_eq(r.start, 0);
    ck_assert_uint_eq(r.end, 0);
}
END_TEST

/* ---- Transcript tests ---- */

START_TEST(test_transcript_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_msg_t *msgs[] = { NULL };
    char *t = ik_summary_transcript(ctx, msgs, 0);
    ck_assert_str_eq(t, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_transcript_single_user) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char kind[] = "user";
    char content[] = "Hello";
    ik_msg_t msg = { .id = 1, .kind = kind, .content = content,
                     .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &msg };
    char *t = ik_summary_transcript(ctx, msgs, 1);
    ck_assert_ptr_nonnull(strstr(t, "user: Hello\n"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_transcript_excludes_metadata) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char ku[] = "user";      char cu[] = "hello";
    char kc[] = "clear";     char cc[] = "cleared";
    char km[] = "mark";      char cm[] = "marked";
    char kr[] = "rewind";    char cr[] = "rewound";
    char ka[] = "assistant"; char ca[] = "world";
    ik_msg_t user_msg = { .id = 1, .kind = ku, .content = cu, .data_json = NULL, .interrupted = false };
    ik_msg_t clear_msg = { .id = 2, .kind = kc, .content = cc, .data_json = NULL, .interrupted = false };
    ik_msg_t mark_msg = { .id = 3, .kind = km, .content = cm, .data_json = NULL, .interrupted = false };
    ik_msg_t rewind_msg = { .id = 4, .kind = kr, .content = cr, .data_json = NULL, .interrupted = false };
    ik_msg_t asst_msg = { .id = 5, .kind = ka, .content = ca, .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &user_msg, &clear_msg, &mark_msg, &rewind_msg, &asst_msg };

    char *t = ik_summary_transcript(ctx, msgs, 5);
    ck_assert_ptr_nonnull(strstr(t, "user: hello\n"));
    ck_assert_ptr_nonnull(strstr(t, "assistant: world\n"));
    ck_assert_ptr_null(strstr(t, "clear:"));
    ck_assert_ptr_null(strstr(t, "mark:"));
    ck_assert_ptr_null(strstr(t, "rewind:"));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_transcript_only_metadata_produces_empty) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char kind[] = "clear";
    char content[] = "cleared";
    ik_msg_t clear_msg = { .id = 1, .kind = kind, .content = content,
                           .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &clear_msg };
    char *t = ik_summary_transcript(ctx, msgs, 1);
    ck_assert_str_eq(t, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_prompt_non_empty) {
    ck_assert(strlen(IK_SUMMARY_PROMPT) > 0);
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "key decisions"));
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "unresolved"));
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "technical details"));
    ck_assert_ptr_nonnull(strstr(IK_SUMMARY_PROMPT, "concise"));
}
END_TEST

/* ---- Mock provider for ik_summary_generate tests ---- */

typedef struct {
    const ik_request_t *captured_req;
    const char *response_text;
    bool should_fail;
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
    m->captured_req = req;

    if (m->should_fail) {
        char err_msg[] = "mock provider error";
        ik_provider_completion_t completion = {
            .success = false,
            .error_message = err_msg,
        };
        cb(&completion, cb_ctx);
        return OK(NULL);
    }

    TALLOC_CTX *tmp = talloc_new(NULL);
    ik_response_t *resp = talloc_zero(tmp, ik_response_t);
    ik_content_block_t *block = talloc_zero(tmp, ik_content_block_t);
    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(tmp, m->response_text);
    resp->content_blocks = block;
    resp->content_count = 1;

    ik_provider_completion_t completion = {
        .success = true,
        .response = resp,
    };
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

static res_t mock_count_tokens(void *ctx, const ik_request_t *req,
                               int32_t *out)
{
    (void)ctx; (void)req;
    *out = 0;
    return OK(NULL);
}

static const ik_provider_vtable_t mock_vt = {
    .fdset = mock_fdset,
    .timeout = mock_timeout,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .start_request = mock_start_request,
    .start_stream = mock_start_stream,
    .count_tokens = mock_count_tokens,
    .cleanup = NULL,
    .cancel = NULL,
};

/* ---- Generate: request structure ---- */

START_TEST(test_generate_request_structure) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char ku[] = "user";      char cu[] = "hello world";
    char ka[] = "assistant"; char ca[] = "goodbye world";
    ik_msg_t user_msg = { .id = 1, .kind = ku, .content = cu,
                          .data_json = NULL, .interrupted = false };
    ik_msg_t asst_msg = { .id = 2, .kind = ka, .content = ca,
                          .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &user_msg, &asst_msg };

    mock_ctx_t mock = { .response_text = "A short summary.", .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 2, &provider, "test-model",
                                    IK_SUMMARY_RECENT_MAX_TOKENS, &summary);

    ck_assert_msg(is_ok(&res), "expected OK");
    ck_assert_ptr_nonnull(summary);

    /* System block must contain the summarization prompt */
    ck_assert_ptr_nonnull(mock.captured_req);
    ck_assert_uint_ge(mock.captured_req->system_block_count, 1);
    ck_assert_ptr_nonnull(strstr(mock.captured_req->system_blocks[0].text,
                                 "key decisions"));

    /* User message must contain the transcript */
    ck_assert_uint_ge(mock.captured_req->message_count, 1);
    ck_assert_int_eq(mock.captured_req->messages[0].role, IK_ROLE_USER);
    ck_assert_uint_ge(mock.captured_req->messages[0].content_count, 1);
    const char *body = mock.captured_req->messages[0].content_blocks[0].data.text.text;
    ck_assert_ptr_nonnull(strstr(body, "user: hello world"));
    ck_assert_ptr_nonnull(strstr(body, "assistant: goodbye world"));

    /* max_output_tokens must equal the supplied limit */
    ck_assert_int_eq(mock.captured_req->max_output_tokens,
                     IK_SUMMARY_RECENT_MAX_TOKENS);

    talloc_free(ctx);
}
END_TEST

/* ---- Generate: summary within token limit passes through unchanged ---- */

START_TEST(test_generate_within_limit) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char ku[] = "user"; char cu[] = "hi";
    ik_msg_t user_msg = { .id = 1, .kind = ku, .content = cu,
                          .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &user_msg };

    const char *expected = "This is a short summary that fits.";
    mock_ctx_t mock = { .response_text = expected, .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    char *summary = NULL;
    /* max_tokens=1000 -> max_bytes=4000; response is <40 bytes */
    res_t res = ik_summary_generate(ctx, msgs, 1, &provider, "test-model",
                                    1000, &summary);

    ck_assert_msg(is_ok(&res), "expected OK");
    ck_assert_ptr_nonnull(summary);
    ck_assert_str_eq(summary, expected);

    talloc_free(ctx);
}
END_TEST

/* ---- Generate: oversized summary truncated at last sentence boundary ---- */

START_TEST(test_generate_oversized_truncated_at_sentence) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char ku[] = "user"; char cu[] = "question";
    ik_msg_t user_msg = { .id = 1, .kind = ku, .content = cu,
                          .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &user_msg };

    /*
     * max_tokens=5 -> max_bytes=20.
     * "First sentence. " is 16 chars (period at index 14, space at 15).
     * The response is 47 bytes total, which exceeds max_bytes=20.
     * truncate_at_sentence should find the '.' at index 14 and cut at 15,
     * yielding "First sentence."
     */
    const char *response = "First sentence. Second sentence that is longer.";
    mock_ctx_t mock = { .response_text = response, .should_fail = false };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &provider, "test-model",
                                    5, &summary);

    ck_assert_msg(is_ok(&res), "expected OK");
    ck_assert_ptr_nonnull(summary);
    ck_assert_str_eq(summary, "First sentence.");

    talloc_free(ctx);
}
END_TEST

/* ---- Generate: provider failure returns ERR ---- */

START_TEST(test_generate_provider_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char ku[] = "user"; char cu[] = "hi";
    ik_msg_t user_msg = { .id = 1, .kind = ku, .content = cu,
                          .data_json = NULL, .interrupted = false };
    ik_msg_t *msgs[] = { &user_msg };

    mock_ctx_t mock = { .response_text = NULL, .should_fail = true };
    ik_provider_t provider = { .name = "mock", .vt = &mock_vt, .ctx = &mock };

    char *summary = NULL;
    res_t res = ik_summary_generate(ctx, msgs, 1, &provider, "test-model",
                                    1000, &summary);

    ck_assert_msg(is_err(&res), "expected ERR on provider failure");

    talloc_free(ctx);
}
END_TEST

static Suite *summary_suite(void)
{
    Suite *s = suite_create("summary");

    TCase *tc_bounds = tcase_create("Boundaries");
    tcase_add_test(tc_bounds, test_boundaries_zero_context_start);
    tcase_add_test(tc_bounds, test_boundaries_context_start_one);
    tcase_add_test(tc_bounds, test_boundaries_context_start_equals_count);
    tcase_add_test(tc_bounds, test_boundaries_zero_count_zero_index);
    suite_add_tcase(s, tc_bounds);

    TCase *tc_transcript = tcase_create("Transcript");
    tcase_add_test(tc_transcript, test_transcript_empty);
    tcase_add_test(tc_transcript, test_transcript_single_user);
    tcase_add_test(tc_transcript, test_transcript_excludes_metadata);
    tcase_add_test(tc_transcript, test_transcript_only_metadata_produces_empty);
    tcase_add_test(tc_transcript, test_prompt_non_empty);
    suite_add_tcase(s, tc_transcript);

    TCase *tc_gen = tcase_create("Generate");
    tcase_add_test(tc_gen, test_generate_request_structure);
    tcase_add_test(tc_gen, test_generate_within_limit);
    tcase_add_test(tc_gen, test_generate_oversized_truncated_at_sentence);
    tcase_add_test(tc_gen, test_generate_provider_failure);
    suite_add_tcase(s, tc_gen);

    return s;
}

int32_t main(void)
{
    Suite *s = summary_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/summary_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
