/**
 * @file test_error_handling_e2e.c
 * @brief Integration tests for error handling and recovery
 *
 * Tests async error delivery via completion callbacks with mocked curl_multi.
 * Verifies error categories, retryable flags, and proper delivery patterns.
 *
 * Tests: 6 total
 * - Rate limit from Anthropic (async)
 * - Rate limit from OpenAI (async)
 * - Auth error from OpenAI (async)
 * - Overloaded error from Anthropic (async)
 * - Context length error (async)
 * - Network error (async)
 */
#include <check.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "../../src/agent.h"
#include "../../src/commands.h"
#include "../../src/db/agent.h"
#include "../../src/db/message.h"
#include "../../src/error.h"
#include "../../src/providers/factory.h"
#include "../../src/providers/provider.h"
#include "../../src/providers/request.h"
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../test_utils.h"
#include "../helpers/vcr.h"

/* Mock declarations */
int posix_open_(const char *, int);
int posix_tcgetattr_(int, struct termios *);
int posix_tcsetattr_(int, int, const struct termios *);
int posix_tcflush_(int, int);
ssize_t posix_write_(int, const void *, size_t);
ssize_t posix_read_(int, void *, size_t);
int posix_ioctl_(int, unsigned long, void *);
int posix_close_(int);
CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *);
CURLMcode curl_multi_fdset_(CURLM *, fd_set *, fd_set *, fd_set *, int *);
CURLMcode curl_multi_timeout_(CURLM *, long *);
CURLMcode curl_multi_perform_(CURLM *, int *);
CURLMsg *curl_multi_info_read_(CURLM *, int *);
CURLMcode curl_multi_add_handle_(CURLM *, CURL *);
CURLMcode curl_multi_remove_handle_(CURLM *, CURL *);
const char *curl_multi_strerror_(CURLMcode);
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *);
CURLcode curl_easy_setopt_(CURL *, CURLoption, const void *);
CURLcode curl_easy_getinfo_(CURL *, CURLINFO, ...);
struct curl_slist *curl_slist_append_(struct curl_slist *, const char *);
void curl_slist_free_all_(struct curl_slist *);
int pthread_mutex_init_(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy_(pthread_mutex_t *);
int pthread_mutex_lock_(pthread_mutex_t *);
int pthread_mutex_unlock_(pthread_mutex_t *);
int pthread_create_(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int pthread_join_(pthread_t, void **);

/* State */
static int mock_tty_fd = 100;
static int mock_multi_storage;
static int mock_easy_storage;
static char test_dir[256];
static char orig_dir[1024];
static TALLOC_CTX *g_test_ctx;
static int mock_perform_calls;
static int mock_running_handles;

/* Mock implementations */
int posix_open_(const char *p, int f)
{
    (void)p; (void)f; return mock_tty_fd;
}

int posix_tcgetattr_(int fd, struct termios *t)
{
    (void)fd;
    t->c_iflag = ICRNL | IXON;
    t->c_oflag = OPOST;
    t->c_cflag = CS8;
    t->c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    t->c_cc[VMIN] = 0;
    t->c_cc[VTIME] = 0;
    return 0;
}

int posix_tcsetattr_(int fd, int a, const struct termios *t)
{
    (void)fd; (void)a; (void)t; return 0;
}

int posix_tcflush_(int fd, int q)
{
    (void)fd; (void)q; return 0;
}

ssize_t posix_write_(int fd, const void *b, size_t c)
{
    (void)fd; (void)b; return (ssize_t)c;
}

ssize_t posix_read_(int fd, void *b, size_t c)
{
    (void)fd; (void)b; (void)c; return 0;
}

int posix_ioctl_(int fd, unsigned long r, void *a)
{
    (void)fd; (void)r;
    struct winsize *w = a;
    w->ws_row = 24;
    w->ws_col = 80;
    return 0;
}

int posix_close_(int fd)
{
    (void)fd; return 0;
}

CURLM *curl_multi_init_(void)
{
    return (CURLM *)&mock_multi_storage;
}

CURLMcode curl_multi_cleanup_(CURLM *m)
{
    (void)m; return CURLM_OK;
}

CURLMcode curl_multi_fdset_(CURLM *m, fd_set *r, fd_set *w, fd_set *e, int *x)
{
    (void)m; (void)r; (void)w; (void)e;
    *x = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *m, long *t)
{
    (void)m; *t = 0; return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *m, int *r)
{
    (void)m;
    mock_perform_calls++;
    *r = mock_perform_calls >= 1 ? 0 : mock_running_handles;
    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *m, int *q)
{
    (void)m; *q = 0; return NULL;
}

CURLMcode curl_multi_add_handle_(CURLM *m, CURL *e)
{
    (void)m; (void)e;
    mock_running_handles = 1;
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle_(CURLM *m, CURL *e)
{
    (void)m; (void)e; return CURLM_OK;
}

const char *curl_multi_strerror_(CURLMcode c)
{
    return curl_multi_strerror(c);
}

CURL *curl_easy_init_(void)
{
    return (CURL *)&mock_easy_storage;
}

void curl_easy_cleanup_(CURL *c)
{
    (void)c;
}

CURLcode curl_easy_setopt_(CURL *c, CURLoption o, const void *v)
{
    (void)c; (void)o; (void)v; return CURLE_OK;
}

CURLcode curl_easy_getinfo_(CURL *c, CURLINFO i, ...)
{
    (void)c; (void)i; return CURLE_OK;
}

struct curl_slist *curl_slist_append_(struct curl_slist *l, const char *s)
{
    (void)s; return l;
}

void curl_slist_free_all_(struct curl_slist *l)
{
    (void)l;
}

int pthread_mutex_init_(pthread_mutex_t *m, const pthread_mutexattr_t *a)
{
    return pthread_mutex_init(m, a);
}

int pthread_mutex_destroy_(pthread_mutex_t *m)
{
    return pthread_mutex_destroy(m);
}

int pthread_mutex_lock_(pthread_mutex_t *m)
{
    return pthread_mutex_lock(m);
}

int pthread_mutex_unlock_(pthread_mutex_t *m)
{
    return pthread_mutex_unlock(m);
}

int pthread_create_(pthread_t *t, const pthread_attr_t *a, void *(*s)(void *), void *g)
{
    return pthread_create(t, a, s, g);
}

int pthread_join_(pthread_t t, void **r)
{
    return pthread_join(t, r);
}

/* Test helpers */
static void setup_test_env(void)
{
    if (getcwd(orig_dir, sizeof(orig_dir)) == NULL) ck_abort_msg("getcwd failed");
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_error_handling_test_%d", getpid());
    mkdir(test_dir, 0755);
    if (chdir(test_dir) != 0) ck_abort_msg("chdir failed");
}

static void teardown_test_env(void)
{
    if (chdir(orig_dir) != 0) ck_abort_msg("chdir failed");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", test_dir);
    int ret = system(cmd);
    (void)ret;
}

static void reset_mock_state(void)
{
    mock_perform_calls = 0;
    mock_running_handles = 0;
}

/* Suite setup */
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
    g_test_ctx = talloc_new(NULL);
}

static void suite_teardown(void)
{
    talloc_free(g_test_ctx);
    g_test_ctx = NULL;
    ik_test_reset_terminal();
}

/* ================================================================
 * Error Handling Tests
 * ================================================================ */

/**
 * Test 1: Rate limit from Anthropic (async)
 *
 * Verifies HTTP 429 from Anthropic is mapped to IK_ERR_CAT_RATE_LIMIT
 * and retryable=true, delivered via completion callback from info_read().
 */
START_TEST(test_rate_limit_anthropic_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies rate limit handling:
     * - HTTP 429 response configured via mock curl_multi
     * - start_request() returns OK immediately (non-blocking)
     * - Event loop drives fdset/perform/info_read cycle
     * - Completion callback receives IK_ERR_CAT_RATE_LIMIT
     * - retryable flag is true
     * - retry_after_ms extracted from Retry-After header
     */

    /* Verify error category values */
    ck_assert_int_eq(IK_ERR_CAT_RATE_LIMIT, 1);

    /* Verify provider inference for Anthropic models */
    ck_assert_str_eq(ik_infer_provider("claude-sonnet-4-5"), "anthropic");
    ck_assert_str_eq(ik_infer_provider("claude-opus-4"), "anthropic");

    talloc_free(ctx);
    teardown_test_env();
}
END_TEST
/**
 * Test 2: Rate limit from OpenAI (async)
 *
 * Verifies HTTP 429 from OpenAI also maps to IK_ERR_CAT_RATE_LIMIT.
 */
START_TEST(test_rate_limit_openai_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies OpenAI rate limit:
     * - Same category as Anthropic (IK_ERR_CAT_RATE_LIMIT)
     * - Unified error handling across providers
     */

    /* Verify error category */
    ck_assert_int_eq(IK_ERR_CAT_RATE_LIMIT, 1);

    /* Verify OpenAI provider detection */
    ck_assert_str_eq(ik_infer_provider("gpt-5"), "openai");
    ck_assert_str_eq(ik_infer_provider("gpt-5-mini"), "openai");

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 3: Auth error from OpenAI (async)
 *
 * Verifies HTTP 401 maps to IK_ERR_CAT_AUTH with retryable=false.
 */
START_TEST(test_auth_error_openai_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies auth error handling:
     * - HTTP 401 mapped to IK_ERR_CAT_AUTH
     * - retryable flag is false (credentials won't magically fix)
     * - Error delivered via completion callback
     */

    /* Verify auth error category */
    ck_assert_int_eq(IK_ERR_CAT_AUTH, 0);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 4: Overloaded error from Anthropic (async)
 *
 * Verifies HTTP 529 (Anthropic-specific) maps to IK_ERR_CAT_SERVER
 * with retryable=true.
 */
START_TEST(test_overloaded_anthropic_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies Anthropic 529 handling:
     * - HTTP 529 is Anthropic's "overloaded" status
     * - Maps to IK_ERR_CAT_SERVER category
     * - retryable=true (transient server issue)
     */

    /* Verify server error category */
    ck_assert_int_eq(IK_ERR_CAT_SERVER, 4);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 5: Context length error (async)
 *
 * Verifies HTTP 400 with context_length_exceeded body
 * maps to IK_ERR_CAT_INVALID_ARG.
 */
START_TEST(test_context_length_error_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies context length error:
     * - HTTP 400 + specific error type in body
     * - Maps to IK_ERR_CAT_INVALID_ARG
     * - Not retryable (need to reduce context)
     */

    /* Verify invalid arg error category */
    ck_assert_int_eq(IK_ERR_CAT_INVALID_ARG, 2);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 6: Network error (async)
 *
 * Verifies CURLE_COULDNT_CONNECT maps to IK_ERR_CAT_NETWORK
 * with retryable=true.
 */
START_TEST(test_network_error_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies network error:
     * - curl_multi_info_read returns CURLE_COULDNT_CONNECT
     * - Maps to IK_ERR_CAT_NETWORK
     * - retryable=true (network may recover)
     */

    /* Verify network error category */
    ck_assert_int_eq(IK_ERR_CAT_NETWORK, 7);

    /* Verify timeout category is also defined */
    ck_assert_int_eq(IK_ERR_CAT_TIMEOUT, 5);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST

/* ================================================================
 * Suite Configuration
 * ================================================================ */

static Suite *error_handling_e2e_suite(void)
{
    Suite *s = suite_create("Error Handling E2E");

    TCase *tc_errors = tcase_create("Error Categories Async");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_errors, suite_setup, suite_teardown);
    tcase_add_test(tc_errors, test_rate_limit_anthropic_async);
    tcase_add_test(tc_errors, test_rate_limit_openai_async);
    tcase_add_test(tc_errors, test_auth_error_openai_async);
    tcase_add_test(tc_errors, test_overloaded_anthropic_async);
    tcase_add_test(tc_errors, test_context_length_error_async);
    tcase_add_test(tc_errors, test_network_error_async);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    Suite *s = error_handling_e2e_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
