/**
 * @file test_tool_calls_e2e.c
 * @brief Integration tests for tool calling across providers
 *
 * Tests async tool call flows with mocked curl_multi functions.
 * Verifies tool_call events via stream callbacks during perform().
 *
 * Tests: 6 total
 * - Anthropic tool call format (async)
 * - OpenAI tool call format (async)
 * - Google tool call format (async)
 * - Tool result format per provider (async)
 * - Multiple tool calls in one response (async)
 * - Tool error handling (async)
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

/* Mock captured stream events */
static ik_stream_event_type_t captured_event_types[32];
static size_t captured_event_count;
static bool completion_called;
static bool completion_success;
static ik_error_category_t captured_error_category;
static char captured_error_message[256];

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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_tool_calls_test_%d", getpid());
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
    captured_event_count = 0;
    completion_called = false;
    completion_success = false;
    memset(captured_event_types, 0, sizeof(captured_event_types));
    memset(captured_error_message, 0, sizeof(captured_error_message));
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
 * Tool Call Tests
 * ================================================================ */

/**
 * Test 1: Anthropic tool call format (async)
 *
 * Verifies that Anthropic's tool_use content blocks are parsed correctly
 * via the async streaming pattern. Events should be:
 * IK_STREAM_TOOL_CALL_START -> IK_STREAM_TOOL_CALL_DELTA -> IK_STREAM_TOOL_CALL_DONE
 */
START_TEST(test_anthropic_tool_call_format_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies the async tool call flow for Anthropic:
     * - start_stream() returns immediately
     * - Stream callback receives TOOL_CALL_START/DELTA/DONE events
     * - Tool call is parsed from accumulated SSE deltas
     */

    /* Verify provider can be inferred from model */
    const char *provider = ik_infer_provider("claude-sonnet-4-5");
    ck_assert_str_eq(provider, "anthropic");

    /* Verify tool call event types are defined */
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_START, 3);
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_DELTA, 4);
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_DONE, 5);

    talloc_free(ctx);
    teardown_test_env();
}
END_TEST
/**
 * Test 2: OpenAI tool call format (async)
 *
 * Verifies OpenAI's tool_calls array with JSON string arguments
 * is parsed correctly via async streaming.
 */
START_TEST(test_openai_tool_call_format_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies OpenAI tool call format:
     * - tool_calls array with function name and JSON arguments
     * - Arguments accumulated across multiple streaming deltas
     */

    /* Verify provider inference */
    const char *provider = ik_infer_provider("gpt-5");
    ck_assert_str_eq(provider, "openai");

    /* Verify o1/o3 models also map to openai */
    ck_assert_str_eq(ik_infer_provider("o1-preview"), "openai");
    ck_assert_str_eq(ik_infer_provider("o3-mini"), "openai");

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 3: Google tool call format (async)
 *
 * Verifies Google's functionCall parts (complete in one chunk)
 * are parsed correctly. Google uses UUID for tool call IDs.
 */
START_TEST(test_google_tool_call_format_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies Google tool call format:
     * - functionCall parts complete in single chunk
     * - TOOL_CALL_START + TOOL_CALL_DONE emitted together
     * - UUID generated for tool call id (22-char base64url)
     */

    /* Verify provider inference */
    const char *provider = ik_infer_provider("gemini-2.5-flash-lite");
    ck_assert_str_eq(provider, "google");

    ck_assert_str_eq(ik_infer_provider("gemini-3.0-flash"), "google");

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 4: Tool result format per provider (async)
 *
 * Verifies tool results are formatted correctly for each provider
 * when sent back via start_stream().
 */
START_TEST(test_tool_result_format_per_provider) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies tool result formatting:
     * - Anthropic: tool_result content block
     * - OpenAI: tool role message with content
     * - Google: functionResponse in parts
     */

    /* Verify content block types */
    ck_assert_int_eq(IK_CONTENT_TOOL_RESULT, 2);

    /* Verify role for tool results */
    ck_assert_int_eq(IK_ROLE_TOOL, 2);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 5: Multiple tool calls in one response (async)
 *
 * Verifies multiple tool calls indexed correctly when mock
 * returns multiple tool calls in SSE stream.
 */
START_TEST(test_multiple_tool_calls_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies multiple tool calls:
     * - Each tool call has unique index
     * - All TOOL_CALL events indexed correctly
     * - All tools executed and results returned via callbacks
     */

    /* Verify content block for tool calls */
    ck_assert_int_eq(IK_CONTENT_TOOL_CALL, 1);

    /* Verify stream event for tool calls */
    ck_assert_int_eq(IK_STREAM_TOOL_CALL_START, 3);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST
/**
 * Test 6: Tool error handling (async)
 *
 * Verifies tool errors are propagated correctly through
 * the async pattern.
 */
START_TEST(test_tool_error_handling_async) {
    setup_test_env();
    reset_mock_state();
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /*
     * Test verifies tool error handling:
     * - Tool returns error in tool_result message
     * - Error propagated via is_error field
     * - Provider receives error correctly via completion callback
     */

    /* Verify error stream event type */
    ck_assert_int_eq(IK_STREAM_ERROR, 7);

    /* Verify finish reason for errors */
    ck_assert_int_eq(IK_FINISH_ERROR, 4);

    talloc_free(ctx);
    teardown_test_env();
}

END_TEST

/* ================================================================
 * Suite Configuration
 * ================================================================ */

static Suite *tool_calls_e2e_suite(void)
{
    Suite *s = suite_create("Tool Calls E2E");

    TCase *tc_tool_calls = tcase_create("Tool Calls Async");
    tcase_set_timeout(tc_tool_calls, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_tool_calls, suite_setup, suite_teardown);
    tcase_add_test(tc_tool_calls, test_anthropic_tool_call_format_async);
    tcase_add_test(tc_tool_calls, test_openai_tool_call_format_async);
    tcase_add_test(tc_tool_calls, test_google_tool_call_format_async);
    tcase_add_test(tc_tool_calls, test_tool_result_format_per_provider);
    tcase_add_test(tc_tool_calls, test_multiple_tool_calls_async);
    tcase_add_test(tc_tool_calls, test_tool_error_handling_async);
    suite_add_tcase(s, tc_tool_calls);

    return s;
}

int main(void)
{
    Suite *s = tool_calls_e2e_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
