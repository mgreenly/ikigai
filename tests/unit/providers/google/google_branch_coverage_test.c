/**
 * @file google_branch_coverage_test.c
 * @brief Additional branch coverage tests for Google provider google.c
 */

#include <check.h>
#include <talloc.h>
#include <sys/select.h>
#include <string.h>

#include "error.h"
#include "logger.h"
#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "providers/google/google.h"
#include "providers/google/google_internal.h"
#include "providers/google/streaming.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "wrapper_internal.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Branch Coverage Tests
 * ================================================================ */

// Helper for stream tests
static res_t noop_stream_cb(const ik_stream_event_t *e, void *c) { (void)e; (void)c; return OK(NULL); }

// Test line 125: NULL data field in event (branch 1)
START_TEST(test_google_stream_write_cb_null_event_data)
{
    ik_google_active_stream_t *stream = talloc_zero(test_ctx, ik_google_active_stream_t);
    res_t r = ik_google_stream_ctx_create(stream, noop_stream_cb, NULL, &stream->stream_ctx);
    ck_assert(!is_err(&r));
    stream->sse_parser = ik_sse_parser_create(stream);
    // Feed data that will produce an event with NULL data field
    // An event with no data lines at all - just an event type
    const char *data = "event: test\n\n";
    ck_assert_uint_eq(ik_google_stream_write_cb(data, strlen(data), stream), strlen(data));
    talloc_free(stream);
}
END_TEST

// Test line 198 branch 3: active_stream exists but not completed
START_TEST(test_google_info_read_active_stream_not_completed)
{
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);
    ck_assert(!is_err(&result));

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)provider->ctx;

    // Create an active stream that is NOT completed
    ik_google_active_stream_t *stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    stream->completed = false;  // Not completed yet
    stream->http_status = 0;
    impl_ctx->active_stream = stream;

    ik_logger_t *logger = ik_logger_create(test_ctx, "/tmp");
    provider->vt->info_read(provider->ctx, logger);

    // Stream should still exist (not cleaned up)
    ck_assert(impl_ctx->active_stream != NULL);
    ck_assert(!impl_ctx->active_stream->completed);

    // Clean up manually
    talloc_free(stream);
    impl_ctx->active_stream = NULL;
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_branch_coverage_suite(void)
{
    Suite *s = suite_create("Google Branch Coverage");

    TCase *tc_branch = tcase_create("Branch Coverage Tests");
    tcase_set_timeout(tc_branch, 30);
    tcase_add_unchecked_fixture(tc_branch, setup, teardown);

    // Line 125: NULL data in event
    tcase_add_test(tc_branch, test_google_stream_write_cb_null_event_data);

    // Line 198 branch 3: active_stream not completed
    tcase_add_test(tc_branch, test_google_info_read_active_stream_not_completed);

    suite_add_tcase(s, tc_branch);

    return s;
}

int main(void)
{
    Suite *s = google_branch_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
