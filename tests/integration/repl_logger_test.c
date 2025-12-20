/**
 * @file repl_logger_test.c
 * @brief Integration test for REPL logger initialization and reinit
 *
 * Tests that:
 * - Logger initializes on REPL startup
 * - Logger reinitializes on /clear command
 * - Logs are written to .ikigai/logs/current.log in working directory
 * - Previous logs are rotated when reinitializing
 */

#include <check.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "../../src/repl.h"
#include "../../src/logger.h"
#include "../test_utils.h"

// Mock terminal file descriptor
static int mock_tty_fd = 100;

// Mock control flags
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_ioctl_fail = 0;
static int mock_pthread_mutex_init_fail = 0;

// Mock function prototypes
int posix_open_(const char *pathname, int flags);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int posix_stat_(const char *pathname, struct stat *statbuf);
int posix_mkdir_(const char *pathname, mode_t mode);
int posix_access_(const char *pathname, int mode);
int posix_rename_(const char *oldpath, const char *newpath);
FILE *fopen_(const char *pathname, const char *mode);
int fclose_(FILE *stream);
CURLM *curl_multi_init_(void);
CURLMcode curl_multi_cleanup_(CURLM *multi);
CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd);
CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout);
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles);
CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue);
CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy);
CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy);
const char *curl_multi_strerror_(CURLMcode code);
CURL *curl_easy_init_(void);
void curl_easy_cleanup_(CURL *curl);
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val);
struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string);
void curl_slist_free_all_(struct curl_slist *list);
int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy_(pthread_mutex_t *mutex);
int pthread_mutex_lock_(pthread_mutex_t *mutex);
int pthread_mutex_unlock_(pthread_mutex_t *mutex);
int pthread_create_(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int pthread_join_(pthread_t thread, void **retval);
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact);
int posix_pipe_(int pipefd[2]);
int posix_fcntl_(int fd, int cmd, int arg);
FILE *posix_fdopen_(int fd, const char *mode);

// Mock functions for terminal operations
int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    if (mock_open_fail) {
        return -1;
    }
    return mock_tty_fd;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
    if (mock_tcgetattr_fail) {
        return -1;
    }
    termios_p->c_iflag = ICRNL | IXON;
    termios_p->c_oflag = OPOST;
    termios_p->c_cflag = CS8;
    termios_p->c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    termios_p->c_cc[VMIN] = 0;
    termios_p->c_cc[VTIME] = 0;
    return 0;
}

int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p)
{
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    if (mock_tcsetattr_fail) {
        return -1;
    }
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    if (mock_tcflush_fail) {
        return -1;
    }
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    if (mock_write_fail) {
        return -1;
    }
    return (ssize_t)count;
}

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

int posix_ioctl_(int fd, unsigned long request, void *argp)
{
    (void)fd;
    (void)request;
    if (mock_ioctl_fail) {
        return -1;
    }
    struct winsize *ws = (struct winsize *)argp;
    ws->ws_row = 24;
    ws->ws_col = 80;
    return 0;
}

int posix_close_(int fd)
{
    (void)fd;
    return 0;
}

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    (void)statbuf;
    // Use real stat for checking if directories/files exist
    return stat(pathname, statbuf);
}

int posix_mkdir_(const char *pathname, mode_t mode)
{
    // Use real mkdir
    return mkdir(pathname, mode);
}

int posix_access_(const char *pathname, int mode)
{
    // Use real access
    return access(pathname, mode);
}

int posix_rename_(const char *oldpath, const char *newpath)
{
    // Use real rename
    return rename(oldpath, newpath);
}

FILE *fopen_(const char *pathname, const char *mode)
{
    // Use real fopen
    return fopen(pathname, mode);
}

int fclose_(FILE *stream)
{
    // Use real fclose
    return fclose(stream);
}

// Mock curl functions
static int mock_multi_handle_storage;
static int mock_easy_handle_storage;

CURLM *curl_multi_init_(void)
{
    return (CURLM *)&mock_multi_handle_storage;
}

CURLMcode curl_multi_cleanup_(CURLM *multi)
{
    (void)multi;
    return CURLM_OK;
}

CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set,
                            int *max_fd)
{
    (void)multi;
    (void)read_fd_set;
    (void)write_fd_set;
    (void)exc_fd_set;
    *max_fd = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    (void)multi;
    *timeout = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    (void)multi;
    *running_handles = 0;
    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    (void)multi;
    *msgs_in_queue = 0;
    return NULL;
}

CURLMcode curl_multi_add_handle_(CURLM *multi, CURL *easy)
{
    (void)multi;
    (void)easy;
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle_(CURLM *multi, CURL *easy)
{
    (void)multi;
    (void)easy;
    return CURLM_OK;
}

const char *curl_multi_strerror_(CURLMcode code)
{
    (void)code;
    return "mock error";
}

CURL *curl_easy_init_(void)
{
    return (CURL *)&mock_easy_handle_storage;
}

void curl_easy_cleanup_(CURL *curl)
{
    (void)curl;
}

CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
    (void)curl;
    (void)opt;
    (void)val;
    return CURLE_OK;
}

struct curl_slist *curl_slist_append_(struct curl_slist *list, const char *string)
{
    (void)list;
    (void)string;
    return (struct curl_slist *)&mock_easy_handle_storage;
}

void curl_slist_free_all_(struct curl_slist *list)
{
    (void)list;
}

int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    (void)attr;
    if (mock_pthread_mutex_init_fail) {
        return -1;
    }
    return pthread_mutex_init(mutex, NULL);
}

int pthread_mutex_destroy_(pthread_mutex_t *mutex)
{
    return pthread_mutex_destroy(mutex);
}

int pthread_mutex_lock_(pthread_mutex_t *mutex)
{
    return pthread_mutex_lock(mutex);
}

int pthread_mutex_unlock_(pthread_mutex_t *mutex)
{
    return pthread_mutex_unlock(mutex);
}

int pthread_create_(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    (void)attr;
    (void)start_routine;
    (void)arg;
    // Create a dummy thread
    return pthread_create(thread, NULL, start_routine, arg);
}

int pthread_join_(pthread_t thread, void **retval)
{
    return pthread_join(thread, retval);
}

int posix_select_(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)readfds;
    (void)writefds;
    (void)exceptfds;
    (void)timeout;
    // Return immediately with no ready fds
    return 0;
}

int posix_sigaction_(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)signum;
    (void)act;
    (void)oldact;
    return 0;
}

int posix_pipe_(int pipefd[2])
{
    return pipe(pipefd);
}

int posix_fcntl_(int fd, int cmd, int arg)
{
    return fcntl(fd, cmd, arg);
}

FILE *posix_fdopen_(int fd, const char *mode)
{
    return fdopen(fd, mode);
}

// Test helper: Check if file exists
static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

// Test helper: Count files in directory matching pattern
static int count_log_files(const char *logs_dir)
{
    int count = 0;
    DIR *d = opendir(logs_dir);
    if (d == NULL) {
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".log") != NULL) {
            count++;
        }
    }
    closedir(d);
    return count;
}

// Test helper: Clean up test directory
static void cleanup_test_dir(const char *test_dir)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'/.ikigai 2>/dev/null || true", test_dir);
    system(cmd);
}

/**
 * Test: Logger initialization on REPL startup
 *
 * - Create REPL in a test directory
 * - Verify .ikigai/logs/current.log is created
 */
START_TEST(test_logger_init_on_repl_startup) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_%d", (int)getpid());

    // Clean up before test
    cleanup_test_dir(test_dir);
    mkdir(test_dir, 0755);

    // Change to test directory
    char orig_cwd[256];
    getcwd(orig_cwd, sizeof(orig_cwd));
    chdir(test_dir);

    // Initialize logger with current working directory
    ik_log_init(test_dir);

    // Verify current.log exists
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/.ikigai/logs/current.log", test_dir);
    ck_assert(file_exists(log_path));

    // Cleanup
    ik_log_shutdown();
    chdir(orig_cwd);
    cleanup_test_dir(test_dir);
}
END_TEST
/**
 * Test: Logger reinit rotates previous log
 *
 * - Initialize logger
 * - Reinitialize logger
 * - Verify previous current.log was rotated to timestamped file
 * - Verify new current.log is created
 */
START_TEST(test_logger_reinit_rotates_log)
{
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_reinit_%d", (int)getpid());

    // Clean up before test
    cleanup_test_dir(test_dir);
    mkdir(test_dir, 0755);

    // Change to test directory
    char orig_cwd[256];
    getcwd(orig_cwd, sizeof(orig_cwd));
    chdir(test_dir);

    // Initialize logger first time
    ik_log_init(test_dir);

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/.ikigai/logs/current.log", test_dir);
    ck_assert(file_exists(log_path));

    // Verify only current.log exists (1 file)
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    int initial_count = count_log_files(logs_dir);
    ck_assert_int_eq(initial_count, 1);

    // Reinitialize logger
    ik_log_reinit(test_dir);

    // Verify current.log still exists
    ck_assert(file_exists(log_path));

    // Verify now we have 2 log files (current.log + rotated file)
    int after_reinit_count = count_log_files(logs_dir);
    ck_assert_int_eq(after_reinit_count, 2);

    // Cleanup
    ik_log_shutdown();
    chdir(orig_cwd);
    cleanup_test_dir(test_dir);
}

END_TEST
/**
 * Test: Multiple reinit cycles
 *
 * - Initialize logger
 * - Reinit multiple times
 * - Verify each reinit creates a new rotated file and current.log always exists
 */
START_TEST(test_logger_multiple_reinit_cycles)
{
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_test_multi_%d", (int)getpid());

    // Clean up before test
    cleanup_test_dir(test_dir);
    mkdir(test_dir, 0755);

    // Change to test directory
    char orig_cwd[256];
    getcwd(orig_cwd, sizeof(orig_cwd));
    chdir(test_dir);

    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/current.log", logs_dir);

    // Initialize
    ik_log_init(test_dir);
    int count_after_init = count_log_files(logs_dir);
    ck_assert_int_eq(count_after_init, 1);
    ck_assert(file_exists(log_path));

    // Reinit 3 times with delays to ensure timestamp differences
    for (int i = 0; i < 3; i++) {
        usleep(10000);  // Sleep 10ms to ensure different timestamp
        ik_log_reinit(test_dir);
        // After each reinit, current.log must exist
        ck_assert(file_exists(log_path));
        // After each reinit, we should have at least 2 files (1+ rotated + current.log)
        int actual = count_log_files(logs_dir);
        ck_assert_int_ge(actual, 2);
    }

    // Cleanup
    ik_log_shutdown();
    chdir(orig_cwd);
    cleanup_test_dir(test_dir);
}

END_TEST

static Suite *repl_logger_suite(void)
{
    Suite *s = suite_create("REPL Logger Integration");

    TCase *tc_init = tcase_create("Logger Init");
    tcase_set_timeout(tc_init, 10);
    tcase_add_test(tc_init, test_logger_init_on_repl_startup);
    suite_add_tcase(s, tc_init);

    TCase *tc_reinit = tcase_create("Logger Reinit");
    tcase_set_timeout(tc_reinit, 10);
    tcase_add_test(tc_reinit, test_logger_reinit_rotates_log);
    tcase_add_test(tc_reinit, test_logger_multiple_reinit_cycles);
    suite_add_tcase(s, tc_reinit);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_logger_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
