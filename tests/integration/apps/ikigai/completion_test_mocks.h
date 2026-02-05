/**
#include "apps/ikigai/wrapper_pthread.h"
 * @file completion_test_mocks.h
 * @brief Shared mock infrastructure for completion integration tests
 */

#ifndef COMPLETION_TEST_MOCKS_H
#define COMPLETION_TEST_MOCKS_H

#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/stat.h>

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

// Test helper functions
static void cleanup_test_dir(void) { unlink(".ikigai/history"); rmdir(".ikigai"); }

static void type_str(ik_repl_ctx_t *repl, const char *s) {
    ik_input_action_t a = {.type = IK_INPUT_CHAR};
    for (size_t i = 0; s[i]; i++) { a.codepoint = (uint32_t)s[i]; ik_repl_process_action(repl, &a); }
}
static void __attribute__((unused)) press_tab(ik_repl_ctx_t *r) { ik_input_action_t a = {.type = IK_INPUT_TAB}; ik_repl_process_action(r, &a); }
static void __attribute__((unused)) press_esc(ik_repl_ctx_t *r) { ik_input_action_t a = {.type = IK_INPUT_ESCAPE}; ik_repl_process_action(r, &a); }

#endif // COMPLETION_TEST_MOCKS_H
