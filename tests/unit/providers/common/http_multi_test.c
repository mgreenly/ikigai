#include "../../../../src/providers/common/http_multi.h"
#include "../../../test_utils.h"

#include <check.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

/**
 * Unit tests for shared HTTP multi-handle client
 *
 * Tests lifecycle, configuration, and basic operations.
 * Integration tests with actual HTTP servers go in tests/integration/.
 */

/* Test context */
static TALLOC_CTX *test_ctx = NULL;

/* Setup/teardown */
static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Lifecycle Tests
 */

START_TEST(test_multi_create_success) {
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(res.ok);

    ik_http_multi_t *multi = res.ok;
    talloc_free(multi);
}
END_TEST START_TEST(test_multi_cleanup_no_crash)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;
    /* Destructor should clean up without crash */
    talloc_free(multi);
}

END_TEST START_TEST(test_fdset_empty_multi)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&exc_fds);

    res_t fdset_res = ik_http_multi_fdset(multi, &read_fds, &write_fds, &exc_fds, &max_fd);
    ck_assert(!fdset_res.is_err);
    /* max_fd should be -1 when no handles are active */
    ck_assert_int_eq(max_fd, -1);

    talloc_free(multi);
}

END_TEST START_TEST(test_perform_empty_multi)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;
    int still_running = 999;

    res_t perform_res = ik_http_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);
    ck_assert_int_eq(still_running, 0);

    talloc_free(multi);
}

END_TEST START_TEST(test_timeout_empty_multi)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;
    long timeout_ms = 0;

    res_t timeout_res = ik_http_multi_timeout(multi, &timeout_ms);
    ck_assert(!timeout_res.is_err);
    /* Timeout should be -1 when no handles are active */
    ck_assert_int_eq(timeout_ms, -1);

    talloc_free(multi);
}

END_TEST START_TEST(test_info_read_empty_multi)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Should not crash with empty multi */
    ik_http_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST
/**
 * Request Configuration Tests
 */

START_TEST(test_add_request_minimal)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Minimal request: just URL */
    ik_http_request_t req = {
        .url = "https://example.com",
        .method = NULL,
        .headers = NULL,
        .body = NULL,
        .body_len = 0
    };

    res_t add_res = ik_http_multi_add_request(multi, &req, NULL, NULL, NULL, NULL);
    ck_assert(!add_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_add_request_with_headers)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Request with custom headers */
    const char *headers[] = {
        "Content-Type: application/json",
        "Authorization: Bearer test-token",
        NULL
    };

    ik_http_request_t req = {
        .url = "https://example.com/api",
        .method = "GET",
        .headers = headers,
        .body = NULL,
        .body_len = 0
    };

    res_t add_res = ik_http_multi_add_request(multi, &req, NULL, NULL, NULL, NULL);
    ck_assert(!add_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_add_request_with_body)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Request with POST body */
    const char *body = "{\"test\": \"data\"}";
    ik_http_request_t req = {
        .url = "https://example.com/api",
        .method = "POST",
        .headers = NULL,
        .body = body,
        .body_len = strlen(body)
    };

    res_t add_res = ik_http_multi_add_request(multi, &req, NULL, NULL, NULL, NULL);
    ck_assert(!add_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_add_request_custom_method)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Request with custom method */
    ik_http_request_t req = {
        .url = "https://example.com/api",
        .method = "DELETE",
        .headers = NULL,
        .body = NULL,
        .body_len = 0
    };

    res_t add_res = ik_http_multi_add_request(multi, &req, NULL, NULL, NULL, NULL);
    ck_assert(!add_res.is_err);

    talloc_free(multi);
}

END_TEST
/**
 * Memory Lifecycle Tests
 */

START_TEST(test_parent_context_frees_all)
{
    TALLOC_CTX *parent = talloc_new(NULL);

    res_t res = ik_http_multi_create(parent);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Add a request */
    ik_http_request_t req = {
        .url = "https://example.com",
        .method = "GET",
        .headers = NULL,
        .body = NULL,
        .body_len = 0
    };

    res_t add_res = ik_http_multi_add_request(multi, &req, NULL, NULL, NULL, NULL);
    ck_assert(!add_res.is_err);

    /* Free parent should clean up everything */
    talloc_free(parent);
    /* If we get here without crash, test passes */
}

END_TEST START_TEST(test_destructor_handles_active_requests)
{
    res_t res = ik_http_multi_create(test_ctx);
    ck_assert(!res.is_err);

    ik_http_multi_t *multi = res.ok;

    /* Add multiple requests */
    for (int i = 0; i < 3; i++) {
        ik_http_request_t req = {
            .url = "https://example.com",
            .method = "GET",
            .headers = NULL,
            .body = NULL,
            .body_len = 0
        };

        res_t add_res = ik_http_multi_add_request(multi, &req, NULL, NULL, NULL, NULL);
        ck_assert(!add_res.is_err);
    }

    /* Destructor should clean up all active requests */
    talloc_free(multi);
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *http_multi_suite(void)
{
    Suite *s = suite_create("HTTP Multi-Handle");

    /* Lifecycle tests */
    TCase *tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_multi_create_success);
    tcase_add_test(tc_lifecycle, test_multi_cleanup_no_crash);
    tcase_add_test(tc_lifecycle, test_fdset_empty_multi);
    tcase_add_test(tc_lifecycle, test_perform_empty_multi);
    tcase_add_test(tc_lifecycle, test_timeout_empty_multi);
    tcase_add_test(tc_lifecycle, test_info_read_empty_multi);
    suite_add_tcase(s, tc_lifecycle);

    /* Request configuration tests */
    TCase *tc_request = tcase_create("Request Configuration");
    tcase_add_checked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_add_request_minimal);
    tcase_add_test(tc_request, test_add_request_with_headers);
    tcase_add_test(tc_request, test_add_request_with_body);
    tcase_add_test(tc_request, test_add_request_custom_method);
    suite_add_tcase(s, tc_request);

    /* Memory lifecycle tests */
    TCase *tc_memory = tcase_create("Memory Lifecycle");
    tcase_add_test(tc_memory, test_parent_context_frees_all);
    tcase_add_test(tc_memory, test_destructor_handles_active_requests);
    suite_add_tcase(s, tc_memory);

    return s;
}

int main(void)
{
    Suite *s = http_multi_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
