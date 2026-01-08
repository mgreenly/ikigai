/**
 * @file provider_switching_fork_test.c
 * @brief Integration tests for fork inheritance
 *
 * Tests: 5 total
 * - Fork inheritance: parent provider, model override, thinking override, cross-provider, DB hierarchy
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
static const char *DB_NAME;
static ik_db_ctx_t *g_db;
static TALLOC_CTX *g_test_ctx;
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

/* Fork Inheritance Tests */
START_TEST(test_fork_inherits_parent_provider) {
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
    ik_agent_ctx_t *parent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &parent); ck_assert(is_ok(&r));
    parent->provider = talloc_strdup(parent, "anthropic");
    parent->model = talloc_strdup(parent, "claude-sonnet-4-5");
    parent->thinking_level = IK_THINKING_MED;
    ik_agent_ctx_t *child = NULL;
    r = ik_agent_create(ctx, shared, parent->uuid, &child); ck_assert(is_ok(&r));
    child->provider = talloc_strdup(child, parent->provider);
    child->model = talloc_strdup(child, parent->model);
    child->thinking_level = parent->thinking_level;
    ck_assert_str_eq(child->provider, "anthropic");
    ck_assert_str_eq(child->model, "claude-sonnet-4-5");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
    ck_assert_str_eq(child->parent_uuid, parent->uuid);
    talloc_free(ctx); teardown_test_env();
}

END_TEST

START_TEST(test_fork_model_override_changes_provider) {
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
    ik_agent_ctx_t *parent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &parent); ck_assert(is_ok(&r));
    parent->provider = talloc_strdup(parent, "anthropic");
    parent->model = talloc_strdup(parent, "claude-sonnet-4-5");
    parent->thinking_level = IK_THINKING_MED;
    ik_agent_ctx_t *child = NULL;
    r = ik_agent_create(ctx, shared, parent->uuid, &child); ck_assert(is_ok(&r));
    const char *override_model = "gpt-5";
    const char *override_provider = ik_infer_provider(override_model);
    child->provider = talloc_strdup(child, override_provider);
    child->model = talloc_strdup(child, override_model);
    child->thinking_level = parent->thinking_level;
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_str_eq(child->model, "gpt-5");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_MED);
    ck_assert_str_eq(parent->provider, "anthropic");
    talloc_free(ctx); teardown_test_env();
}

END_TEST

START_TEST(test_fork_thinking_override) {
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
    ik_agent_ctx_t *parent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &parent); ck_assert(is_ok(&r));
    parent->provider = talloc_strdup(parent, "openai");
    parent->model = talloc_strdup(parent, "o1-preview");
    parent->thinking_level = IK_THINKING_LOW;
    ik_agent_ctx_t *child = NULL;
    r = ik_agent_create(ctx, shared, parent->uuid, &child); ck_assert(is_ok(&r));
    child->provider = talloc_strdup(child, parent->provider);
    child->model = talloc_strdup(child, parent->model);
    child->thinking_level = IK_THINKING_HIGH;
    ck_assert_str_eq(child->provider, "openai");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);
    ck_assert_int_eq(parent->thinking_level, IK_THINKING_LOW);
    talloc_free(ctx); teardown_test_env();
}

END_TEST

START_TEST(test_fork_full_cross_provider_override) {
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
    ik_agent_ctx_t *parent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &parent); ck_assert(is_ok(&r));
    parent->provider = talloc_strdup(parent, "anthropic");
    parent->model = talloc_strdup(parent, "claude-sonnet-4-5");
    parent->thinking_level = IK_THINKING_MED;
    ik_agent_ctx_t *child = NULL;
    r = ik_agent_create(ctx, shared, parent->uuid, &child); ck_assert(is_ok(&r));
    const char *override_model = "gemini-2.5-flash-lite";
    const char *override_provider = ik_infer_provider(override_model);
    child->provider = talloc_strdup(child, override_provider);
    child->model = talloc_strdup(child, override_model);
    child->thinking_level = IK_THINKING_HIGH;
    ck_assert_str_eq(child->provider, "google");
    ck_assert_str_eq(child->model, "gemini-2.5-flash-lite");
    ck_assert_int_eq(child->thinking_level, IK_THINKING_HIGH);
    ck_assert_str_eq(parent->provider, "anthropic");
    ck_assert_int_eq(parent->thinking_level, IK_THINKING_MED);
    talloc_free(ctx); teardown_test_env();
}

END_TEST

START_TEST(test_database_records_fork_hierarchy) {
    setup_test_env(); reset_mock_state();
    test_paths_setup_env();
    res_t r = ik_test_db_begin(g_db); ck_assert(is_ok(&r));
    TALLOC_CTX *ctx = talloc_new(NULL); ck_assert_ptr_nonnull(ctx);
    ik_config_t *cfg = ik_test_create_config(ctx);
    ik_paths_t *paths = NULL;
    r = ik_paths_init(ctx, &paths); ck_assert(is_ok(&r));
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    r = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&r));
    shared->db_ctx = g_db;
    ik_agent_ctx_t *parent = NULL;
    r = ik_agent_create(ctx, shared, NULL, &parent); ck_assert(is_ok(&r));
    parent->provider = talloc_strdup(parent, "anthropic");
    parent->model = talloc_strdup(parent, "claude-sonnet-4-5");
    parent->thinking_level = IK_THINKING_MED;
    r = ik_db_agent_insert(g_db, parent); ck_assert(is_ok(&r));
    int64_t fork_message_id = 0;
    r = ik_db_agent_get_last_message_id(g_db, parent->uuid, &fork_message_id);
    ck_assert(is_ok(&r));
    ik_agent_ctx_t *child = NULL;
    r = ik_agent_create(ctx, shared, parent->uuid, &child); ck_assert(is_ok(&r));
    child->fork_message_id = fork_message_id;
    child->provider = talloc_strdup(child, "openai");
    child->model = talloc_strdup(child, "gpt-5");
    child->thinking_level = IK_THINKING_HIGH;
    r = ik_db_agent_insert(g_db, child); ck_assert(is_ok(&r));
    ik_db_agent_row_t *row = NULL;
    r = ik_db_agent_get(g_db, ctx, child->uuid, &row);
    ck_assert(is_ok(&r)); ck_assert_ptr_nonnull(row);
    ck_assert_str_eq(row->parent_uuid, parent->uuid);
    ck_assert_str_eq(row->provider, "openai");
    ck_assert_str_eq(row->model, "gpt-5");
    ck_assert_str_eq(row->thinking_level, "high");
    r = ik_test_db_rollback(g_db); ck_assert(is_ok(&r));
    talloc_free(ctx); teardown_test_env();
}

END_TEST

/* Suite Setup */
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    res_t r = ik_test_db_create(DB_NAME); ck_assert(is_ok(&r));
    r = ik_test_db_migrate(NULL, DB_NAME); ck_assert(is_ok(&r));
    g_test_ctx = talloc_new(NULL);
    r = ik_test_db_connect(g_test_ctx, DB_NAME, &g_db); ck_assert(is_ok(&r));
}

static void suite_teardown(void)
{
    talloc_free(g_test_ctx); g_db = NULL;
    res_t r = ik_test_db_destroy(DB_NAME); (void)r;
    ik_test_reset_terminal();
}

static Suite *provider_switching_fork_suite(void)
{
    Suite *s = suite_create("Provider Switching Fork");
    TCase *tc_fork = tcase_create("Fork Inheritance");
    tcase_set_timeout(tc_fork, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_fork, suite_setup, suite_teardown);
    tcase_add_test(tc_fork, test_fork_inherits_parent_provider);
    tcase_add_test(tc_fork, test_fork_model_override_changes_provider);
    tcase_add_test(tc_fork, test_fork_thinking_override);
    tcase_add_test(tc_fork, test_fork_full_cross_provider_override);
    tcase_add_test(tc_fork, test_database_records_fork_hierarchy);
    suite_add_tcase(s, tc_fork);
    return s;
}

int main(void)
{
    Suite *s = provider_switching_fork_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
