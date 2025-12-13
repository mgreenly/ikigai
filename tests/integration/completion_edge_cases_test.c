/**
 * @file completion_edge_cases_test.c
 * @brief Edge case tests for tab completion feature
 */

#include "../../src/agent.h"
#include "../../src/completion.h"
#include "../../src/input.h"
#include "../../src/repl.h"
#include "../../src/repl_actions.h"
#include "../../src/shared.h"

#include <check.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include "../../src/history.h"
#include "../../src/input_buffer/core.h"
#include "../test_utils.h"

static int mock_tty_fd = 100;

// Forward declarations
int posix_open_(const char *p, int f);
int posix_tcgetattr_(int fd, struct termios *t);
int posix_tcsetattr_(int fd, int a, const struct termios *t);
int posix_tcflush_(int fd, int q);
ssize_t posix_write_(int fd, const void *b, size_t c);
ssize_t posix_read_(int fd, void *b, size_t c);
int posix_ioctl_(int fd, unsigned long r, void *a);
int posix_close_(int fd);
CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *m);
CURLMcode curl_multi_fdset_(CURLM *m, fd_set *r, fd_set *w, fd_set *e, int *max);
CURLMcode curl_multi_timeout_(CURLM *m, long *t);
CURLMcode curl_multi_perform_(CURLM *m, int *r);
CURLMsg *curl_multi_info_read_(CURLM *m, int *q);
CURLMcode curl_multi_add_handle_(CURLM *m, CURL *e);
CURLMcode curl_multi_remove_handle_(CURLM *m, CURL *e);
const char *curl_multi_strerror_(CURLMcode c);
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *c);
CURLcode curl_easy_setopt_(CURL *c, CURLoption o, const void *v);
struct curl_slist *curl_slist_append_(struct curl_slist *l, const char *s);
void curl_slist_free_all_(struct curl_slist *l);
int pthread_mutex_init_(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int pthread_mutex_destroy_(pthread_mutex_t *m);
int pthread_mutex_lock_(pthread_mutex_t *m);
int pthread_mutex_unlock_(pthread_mutex_t *m);
int pthread_create_(pthread_t *t, const pthread_attr_t *a, void *(*s)(void *), void *g);
int pthread_join_(pthread_t t, void **r);

// Wrapper implementations
int posix_open_(const char *p, int f) { (void)p; (void)f; return mock_tty_fd; }
int posix_tcgetattr_(int fd, struct termios *t) {
    (void)fd;
    t->c_iflag = ICRNL|IXON; t->c_oflag = OPOST;
    t->c_cflag = CS8; t->c_lflag = ECHO|ICANON|IEXTEN|ISIG;
    t->c_cc[VMIN] = 0; t->c_cc[VTIME] = 0;
    return 0;
}
int posix_tcsetattr_(int fd, int a, const struct termios *t) { (void)fd; (void)a; (void)t; return 0; }
int posix_tcflush_(int fd, int q) { (void)fd; (void)q; return 0; }
ssize_t posix_write_(int fd, const void *b, size_t c) { (void)fd; (void)b; return (ssize_t)c; }
ssize_t posix_read_(int fd, void *b, size_t c) { (void)fd; (void)b; (void)c; return 0; }
int posix_ioctl_(int fd, unsigned long r, void *a) {
    (void)fd; (void)r;
    struct winsize *ws = (struct winsize *)a;
    ws->ws_row = 24; ws->ws_col = 80;
    return 0;
}
int posix_close_(int fd) { (void)fd; return 0; }

// Curl mocks
static int mock_multi, mock_easy;
CURLM *curl_multi_init_(void) { return (CURLM *)&mock_multi; }
CURLMcode curl_multi_cleanup_(CURLM *m) { (void)m; return CURLM_OK; }
CURLMcode curl_multi_fdset_(CURLM *m, fd_set *r, fd_set *w, fd_set *e, int *max) {
    (void)m; (void)r; (void)w; (void)e; *max = -1; return CURLM_OK;
}
CURLMcode curl_multi_timeout_(CURLM *m, long *t) { (void)m; *t = -1; return CURLM_OK; }
CURLMcode curl_multi_perform_(CURLM *m, int *r) { (void)m; *r = 0; return CURLM_OK; }
CURLMsg *curl_multi_info_read_(CURLM *m, int *q) { (void)m; *q = 0; return NULL; }
CURLMcode curl_multi_add_handle_(CURLM *m, CURL *e) { (void)m; (void)e; return CURLM_OK; }
CURLMcode curl_multi_remove_handle_(CURLM *m, CURL *e) { (void)m; (void)e; return CURLM_OK; }
const char *curl_multi_strerror_(CURLMcode c) { return curl_multi_strerror(c); }
CURL *curl_easy_init_(void) { return (CURL *)&mock_easy; }
void curl_easy_cleanup_(CURL *c) { (void)c; }
CURLcode curl_easy_setopt_(CURL *c, CURLoption o, const void *v) { (void)c; (void)o; (void)v; return CURLE_OK; }
struct curl_slist *curl_slist_append_(struct curl_slist *l, const char *s) { (void)s; return l; }
void curl_slist_free_all_(struct curl_slist *l) { (void)l; }

// Pthread mocks
int pthread_mutex_init_(pthread_mutex_t *m, const pthread_mutexattr_t *a) { return pthread_mutex_init(m, a); }
int pthread_mutex_destroy_(pthread_mutex_t *m) { return pthread_mutex_destroy(m); }
int pthread_mutex_lock_(pthread_mutex_t *m) { return pthread_mutex_lock(m); }
int pthread_mutex_unlock_(pthread_mutex_t *m) { return pthread_mutex_unlock(m); }
int pthread_create_(pthread_t *t, const pthread_attr_t *a, void *(*s)(void *), void *g) {
    return pthread_create(t, a, s, g);
}
int pthread_join_(pthread_t t, void **r) { return pthread_join(t, r); }

static void cleanup_test_dir(void) { unlink(".ikigai/history"); rmdir(".ikigai"); }

static void type_str(ik_repl_ctx_t *repl, const char *s) {
    ik_input_action_t a = {.type = IK_INPUT_CHAR};
    for (size_t i = 0; s[i]; i++) { a.codepoint = (uint32_t)s[i]; ik_repl_process_action(repl, &a); }
}
static void press_tab(ik_repl_ctx_t *r) { ik_input_action_t a = {.type = IK_INPUT_TAB}; ik_repl_process_action(r, &a); }
static void press_esc(ik_repl_ctx_t *r) { ik_input_action_t a = {.type = IK_INPUT_ESCAPE}; ik_repl_process_action(r, &a); }
static void press_space(ik_repl_ctx_t *r) { ik_input_action_t a = {.type = IK_INPUT_CHAR, .codepoint = ' '}; ik_repl_process_action(r, &a); }

/* Test: Tab accepts selection and dismisses completion */
START_TEST(test_completion_space_commits)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/m");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len >= 2);
    ck_assert_mem_eq(text, "/", 1);
    press_space(repl);
    text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    // Now should have more text (added space)
    ck_assert(len > 2);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: Multiple Tab presses - each press accepts and dismisses */
START_TEST(test_completion_tab_wraparound)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/debug ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 7);
    ck_assert_mem_eq(text, "/debug ", 7);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

START_TEST(test_completion_single_item)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/debug");
    press_tab(repl);
    // Tab accepts and dismisses completion
    ck_assert_ptr_null(repl->current->completion);

    // Input buffer should have the selected command
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 0);
    ck_assert_mem_eq(text, "/", 1);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: Tab accepts completion, ESC on empty input does nothing */
START_TEST(test_completion_escape_exact_revert)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/mar");
    size_t original_len = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &original_len);

    // Press Tab to accept completion
    press_tab(repl);
    // Completion is dismissed after Tab
    ck_assert_ptr_null(repl->current->completion);

    // Text should have changed to a completion match
    size_t new_len = 0;
    const char *new_text = ik_input_buffer_get_text(repl->current->input_buffer, &new_len);
    ck_assert(new_len >= original_len);

    // ESC after Tab has no effect (completion already dismissed)
    press_esc(repl);
    ck_assert_ptr_null(repl->current->completion);

    // Text stays as is after ESC
    size_t final_len = 0;
    const char *final_text = ik_input_buffer_get_text(repl->current->input_buffer, &final_len);
    ck_assert_uint_eq(final_len, new_len);
    ck_assert_mem_eq(final_text, new_text, final_len);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: Tab accepts and dismisses immediately */
START_TEST(test_completion_tab_cycle_then_space)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/debug ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 7);
    ck_assert_mem_eq(text, "/debug ", 7);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

START_TEST(test_completion_space_on_first_tab)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/d");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len >= 2);
    ck_assert_mem_eq(text, "/", 1);
    size_t len_before_space = len;
    press_space(repl);
    text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    // Should have added a character
    ck_assert_uint_eq(len, len_before_space + 1);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: Type after Tab adds to accepted completion */
START_TEST(test_completion_type_cancels)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/m");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len_before = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &len_before);
    ik_input_action_t a = {.type = IK_INPUT_CHAR, .codepoint = 'x'};
    ik_repl_process_action(repl, &a);

    // Check input buffer has new char
    size_t len_after = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &len_after);
    // Should have added the 'x'
    ck_assert_uint_eq(len_after, len_before + 1);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: /rewind command has no args without marks */
START_TEST(test_completion_rewind_args)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/rewind ");
    press_tab(repl);
    // No completion available
    ck_assert_ptr_null(repl->current->completion);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 8);  // "/rewind " with no completion added
    ck_assert_mem_eq(text, "/rewind ", 8);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: /mark command has no argument completion */
START_TEST(test_completion_mark_no_args)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/mark ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 6);  // "/mark "
    ck_assert_mem_eq(text, "/mark ", 6);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

/* Test: /help command has no argument completion */
START_TEST(test_completion_help_no_args)
{
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/help ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 6);  // "/help "
    ck_assert_mem_eq(text, "/help ", 6);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST

static Suite *completion_edge_cases_suite(void)
{
    Suite *s = suite_create("Completion Edge Cases");

    TCase *tc = tcase_create("Edge Cases");
    tcase_set_timeout(tc, 30);
    tcase_add_test(tc, test_completion_space_commits);
    tcase_add_test(tc, test_completion_tab_wraparound);
    tcase_add_test(tc, test_completion_single_item);
    tcase_add_test(tc, test_completion_escape_exact_revert);
    tcase_add_test(tc, test_completion_tab_cycle_then_space);
    tcase_add_test(tc, test_completion_space_on_first_tab);
    tcase_add_test(tc, test_completion_type_cancels);
    tcase_add_test(tc, test_completion_rewind_args);
    tcase_add_test(tc, test_completion_mark_no_args);
    tcase_add_test(tc, test_completion_help_no_args);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = completion_edge_cases_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    ik_test_reset_terminal();
    return (nf == 0) ? 0 : 1;
}
