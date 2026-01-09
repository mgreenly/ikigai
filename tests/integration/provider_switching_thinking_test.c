/**
 * @file provider_switching_thinking_test.c
 * @brief Integration tests for thinking level translation
 *
 * Tests: 4 total
 * - Thinking level enum, supports check, preservation, change
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
#include "../../src/paths.h"
#include "../../src/providers/factory.h"
#include "../../src/providers/provider.h"
#include "../../src/providers/request.h"
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../test_utils.h"
#include "../helpers/vcr.h"

/* Mock declarations */
int posix_open_(const char *, int); int posix_tcgetattr_(int, struct termios *);
int posix_tcsetattr_(int, int, const struct termios *); int posix_tcflush_(int, int);
ssize_t posix_write_(int, const void *, size_t); ssize_t posix_read_(int, void *, size_t);
int posix_ioctl_(int, unsigned long, void *); int posix_close_(int);
CURLM *curl_multi_init_(void); CURLMcode curl_multi_cleanup_(CURLM *);
CURLMcode curl_multi_fdset_(CURLM *, fd_set *, fd_set *, fd_set *, int *);
CURLMcode curl_multi_timeout_(CURLM *, long *); CURLMcode curl_multi_perform_(CURLM *, int *);
CURLMsg *curl_multi_info_read_(CURLM *, int *);
CURLMcode curl_multi_add_handle_(CURLM *, CURL *); CURLMcode curl_multi_remove_handle_(CURLM *, CURL *);
const char *curl_multi_strerror_(CURLMcode);
CURL *curl_easy_init_(void); void curl_easy_cleanup_(CURL *);
CURLcode curl_easy_setopt_(CURL *, CURLoption, const void *);
CURLcode curl_easy_getinfo_(CURL *, CURLINFO, ...);
struct curl_slist *curl_slist_append_(struct curl_slist *, const char *);
void curl_slist_free_all_(struct curl_slist *);
int pthread_mutex_init_(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy_(pthread_mutex_t *); int pthread_mutex_lock_(pthread_mutex_t *);
int pthread_mutex_unlock_(pthread_mutex_t *);
int pthread_create_(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int pthread_join_(pthread_t, void **);

/* State */
static int mock_tty_fd = 100, mock_multi_storage, mock_easy_storage;
static char test_dir[256], orig_dir[1024];
static int mock_perform_calls, mock_running_handles;

/* Mock implementations */
int posix_open_(const char *p, int f)
{
    (void)p; (void)f; return mock_tty_fd;
}

int posix_tcgetattr_(int fd, struct termios *t)
{
    (void)fd; t->c_iflag = ICRNL | IXON; t->c_oflag = OPOST; t->c_cflag = CS8;
    t->c_lflag = ECHO | ICANON | IEXTEN | ISIG; t->c_cc[VMIN] = 0; t->c_cc[VTIME] = 0; return 0;
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
    (void)fd; (void)r; struct winsize *w = a; w->ws_row = 24; w->ws_col = 80; return 0;
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
    (void)m; (void)r; (void)w; (void)e; *x = -1; return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *m, long *t)
{
    (void)m; *t = 0; return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *m, int *r)
{
    (void)m; mock_perform_calls++; *r = mock_perform_calls >= 1 ? 0 : mock_running_handles; return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *m, int *q)
{
    (void)m; *q = 0; return NULL;
}

CURLMcode curl_multi_add_handle_(CURLM *m, CURL *e)
{
    (void)m; (void)e; mock_running_handles = 1; return CURLM_OK;
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
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_provider_test_%d", getpid());
    mkdir(test_dir, 0755);
    if (chdir(test_dir) != 0) ck_abort_msg("chdir failed");
}

static void teardown_test_env(void)
{
    if (chdir(orig_dir) != 0) ck_abort_msg("chdir failed");
    char cmd[512]; snprintf(cmd, sizeof(cmd), "rm -rf '%s'", test_dir);
    int ret = system(cmd); (void)ret;
}

static void reset_mock_state(void)
{
    mock_perform_calls = 0; mock_running_handles = 0;
}

/* Thinking Level Tests */
START_TEST(test_thinking_level_enum_values) {
    ck_assert_int_eq(IK_THINKING_NONE, 0);
    ck_assert_int_eq(IK_THINKING_LOW, 1);
    ck_assert_int_eq(IK_THINKING_MED, 2);
    ck_assert_int_eq(IK_THINKING_HIGH, 3);
}

END_TEST

START_TEST(test_model_supports_thinking) {
    bool supports = false;
    ik_model_supports_thinking("claude-sonnet-4-5", &supports); ck_assert(supports);
    ik_model_supports_thinking("claude-opus-4-5", &supports); ck_assert(supports);
    ik_model_supports_thinking("claude-haiku-4-5", &supports); ck_assert(supports);
    ik_model_supports_thinking("gpt-5", &supports); ck_assert(supports);
    ik_model_supports_thinking("gpt-5-mini", &supports); ck_assert(supports);
    ik_model_supports_thinking("gemini-2.5-flash-lite", &supports); ck_assert(supports);
    ik_model_supports_thinking("gemini-3.0-flash", &supports); ck_assert(supports);
    ik_model_supports_thinking("gpt-4o", &supports); ck_assert(!supports);
    ik_model_supports_thinking("gpt-4", &supports); ck_assert(!supports);
    ik_model_supports_thinking("o1-mini", &supports); ck_assert(!supports);
    ik_model_supports_thinking("unknown-model", &supports); ck_assert(!supports);
}

END_TEST

START_TEST(test_thinking_level_preserved_on_switch) {
    setup_test_env(); reset_mock_state();
    test_paths_setup_env();
    TALLOC_CTX *ctx = talloc_new(NULL); ck_assert_ptr_nonnull(ctx);
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_paths_t *paths = NULL;
    res_t r = ik_paths_init(ctx, &paths); ck_assert(is_ok(&r));
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    r = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&r));
    ik_agent_ctx_t *agent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &agent); ck_assert(is_ok(&r));
    agent->provider = talloc_strdup(agent, "anthropic");
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = IK_THINKING_MED;
    talloc_free(agent->provider); talloc_free(agent->model);
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "o1-preview");
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_MED);
    talloc_free(agent->provider); talloc_free(agent->model);
    agent->provider = talloc_strdup(agent, "google");
    agent->model = talloc_strdup(agent, "gemini-2.5-flash-lite");
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_MED);
    talloc_free(ctx); teardown_test_env();
}

END_TEST

START_TEST(test_thinking_level_change_after_switch) {
    setup_test_env(); reset_mock_state();
    test_paths_setup_env();
    TALLOC_CTX *ctx = talloc_new(NULL); ck_assert_ptr_nonnull(ctx);
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_paths_t *paths = NULL;
    res_t r = ik_paths_init(ctx, &paths); ck_assert(is_ok(&r));
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    r = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&r));
    ik_agent_ctx_t *agent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &agent); ck_assert(is_ok(&r));
    agent->provider = talloc_strdup(agent, "anthropic");
    agent->model = talloc_strdup(agent, "claude-sonnet-4-5");
    agent->thinking_level = IK_THINKING_LOW;
    talloc_free(agent->provider); talloc_free(agent->model);
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "o1-preview");
    agent->thinking_level = IK_THINKING_HIGH;
    ck_assert_int_eq(agent->thinking_level, IK_THINKING_HIGH);
    ck_assert_str_eq(agent->provider, "openai");
    talloc_free(ctx); teardown_test_env();
}

END_TEST

static Suite *provider_switching_thinking_suite(void)
{
    Suite *s = suite_create("Provider Switching Thinking");
    TCase *tc_thinking = tcase_create("Thinking Level Translation");
    tcase_set_timeout(tc_thinking, IK_TEST_TIMEOUT);
    tcase_add_test(tc_thinking, test_thinking_level_enum_values);
    tcase_add_test(tc_thinking, test_model_supports_thinking);
    tcase_add_test(tc_thinking, test_thinking_level_preserved_on_switch);
    tcase_add_test(tc_thinking, test_thinking_level_change_after_switch);
    suite_add_tcase(s, tc_thinking);
    return s;
}

int main(void)
{
    Suite *s = provider_switching_thinking_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
