#include "../../src/agent.h"
#include "../../src/history.h"
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../test_utils.h"

#include <check.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

int posix_open_(const char *, int); int posix_tcgetattr_(int, struct termios *); int posix_tcsetattr_(int,
                                                                                                      int,
                                                                                                      const struct
                                                                                                      termios *);
int posix_tcflush_(int, int); ssize_t posix_write_(int, const void *, size_t); ssize_t posix_read_(int, void *, size_t);
int posix_ioctl_(int, unsigned long, void *); int posix_close_(int); CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *); CURLMcode curl_multi_fdset_(CURLM *, fd_set *, fd_set *, fd_set *, int *);
CURLMcode curl_multi_timeout_(CURLM *, long *); CURLMcode curl_multi_perform_(CURLM *, int *);
CURLMsg *curl_multi_info_read_(CURLM *, int *); CURLMcode curl_multi_add_handle_(CURLM *, CURL *);
CURLMcode curl_multi_remove_handle_(CURLM *, CURL *); const char *curl_multi_strerror_(CURLMcode);
CURL *curl_easy_init_(void); void curl_easy_cleanup_(CURL *); CURLcode curl_easy_setopt_(CURL *,
                                                                                         CURLoption,
                                                                                         const void *);
struct curl_slist *curl_slist_append_(struct curl_slist *, const char *);
void curl_slist_free_all_(struct curl_slist *); int pthread_mutex_init_(pthread_mutex_t *, const pthread_mutexattr_t *);
int pthread_mutex_destroy_(pthread_mutex_t *); int pthread_mutex_lock_(pthread_mutex_t *);
int pthread_mutex_unlock_(pthread_mutex_t *); int pthread_create_(pthread_t *,
                                                                  const pthread_attr_t *,
                                                                  void *(*)(void *),
                                                                  void *); int pthread_join_(pthread_t, void **);
static int mock_tty_fd = 100, mock_multi_storage, mock_easy_storage;
static char test_dir[256];
static char orig_dir[1024];
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
    (void)fd; (void)r; struct winsize *w = (struct winsize *)a; w->ws_row = 24; w->ws_col = 80; return 0;
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
    (void)m; *t = -1; return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *m, int *r)
{
    (void)m; *r = 0; return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *m, int *q)
{
    (void)m; *q = 0; return NULL;
}

CURLMcode curl_multi_add_handle_(CURLM *m, CURL *e)
{
    (void)m; (void)e; return CURLM_OK;
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

static void setup_test_env(void)
{
    if (getcwd(orig_dir, sizeof(orig_dir)) == NULL) ck_abort_msg("getcwd failed");
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", getpid());
    mkdir(test_dir, 0755);
    if (chdir(test_dir) != 0) ck_abort_msg("chdir failed");
}

static void teardown_test_env(void)
{
    if (chdir(orig_dir) != 0) ck_abort_msg("chdir failed");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", test_dir);
    system(cmd);
}

static void cleanup_test_dir(void)
{
    unlink(".ikigai/history");
    rmdir(".ikigai");
}

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

START_TEST(test_history_respects_config_capacity) {
    setup_test_env();
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 3;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    res_t result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_uint_eq(repl->shared->history->capacity, 3);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}
END_TEST START_TEST(test_history_multiline_preserved)
{
    setup_test_env();
    cleanup_test_dir();

    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    const char *multiline = "line 1\nline 2\nline 3";
    result = ik_input_buffer_set_text(repl->current->input_buffer, multiline, strlen(multiline));
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    ck_assert_uint_eq(repl->shared->history->count, 1);
    ck_assert_str_eq(repl->shared->history->entries[0], "line 1\nline 2\nline 3");

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}

END_TEST START_TEST(test_history_submit_stops_browsing)
{
    setup_test_env();
    cleanup_test_dir();

    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));

    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));

    result = ik_input_buffer_set_text(repl->current->input_buffer, "command 1", 9);
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    ik_history_start_browsing(repl->shared->history, "");
    ck_assert(ik_history_is_browsing(repl->shared->history));

    result = ik_input_buffer_set_text(repl->current->input_buffer, "command 2", 9);
    ck_assert(is_ok(&result));
    result = ik_repl_submit_line(repl);
    ck_assert(is_ok(&result));

    ck_assert(!ik_history_is_browsing(repl->shared->history));

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
    teardown_test_env();
}

END_TEST

static Suite *history_navigation_suite(void)
{
    Suite *s = suite_create("History Navigation");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);
    tcase_add_test(tc_core, test_history_respects_config_capacity);
    tcase_add_test(tc_core, test_history_multiline_preserved);
    tcase_add_test(tc_core, test_history_submit_stops_browsing);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = history_navigation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
