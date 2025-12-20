#include "agent.h"
#include "../../src/agent.h"
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../test_utils.h"

#include <check.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

static int mock_tty_fd = 100;
static int mock_open_fail = 0;
static int mock_tcgetattr_fail = 0;
static int mock_tcsetattr_fail = 0;
static int mock_tcflush_fail = 0;
static int mock_write_fail = 0;
static int mock_ioctl_fail = 0;
static int mock_pthread_mutex_init_fail = 0;
int posix_open_(const char *pathname, int flags);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
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
    if (mock_ioctl_fail) return -1;
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
    return curl_multi_strerror(code);
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
    (void)string;
    return list;
}

void curl_slist_free_all_(struct curl_slist *list)
{
    (void)list;
}

int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    if (mock_pthread_mutex_init_fail) {
        return 1;
    }
    return pthread_mutex_init(mutex, attr);
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

int pthread_create_(pthread_t *thread, const pthread_attr_t *attr,
                    void *(*start_routine)(void *), void *arg)
{
    return pthread_create(thread, attr, start_routine, arg);
}

int pthread_join_(pthread_t thread, void **retval)
{
    return pthread_join(thread, retval);
}

static void reset_mocks(void)
{
    mock_open_fail = 0;
    mock_tcgetattr_fail = 0;
    mock_tcsetattr_fail = 0;
    mock_tcflush_fail = 0;
    mock_write_fail = 0;
    mock_ioctl_fail = 0;
    mock_pthread_mutex_init_fail = 0;
}

START_TEST(test_repl_init) {
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_nonnull(repl->shared->term);
    ck_assert_ptr_nonnull(repl->shared->render);
    ck_assert_ptr_nonnull(repl->current->input_buffer);
    ck_assert_ptr_nonnull(repl->input_parser);
    ck_assert(!repl->quit);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}
END_TEST START_TEST(test_repl_cleanup_null)
{
    ik_repl_cleanup(NULL);
}

END_TEST START_TEST(test_repl_cleanup_null_term)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    repl->shared = shared;
    repl->shared->term = NULL;
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST START_TEST(test_repl_run)
{
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    repl->quit = true;
    res_t run_result = ik_repl_run(repl);
    ck_assert(is_ok(&run_result));
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST START_TEST(test_thread_infrastructure_init)
{
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(repl);
    ck_assert(!repl->current->tool_thread_running);
    ck_assert(!repl->current->tool_thread_complete);
    ck_assert_ptr_null(repl->current->tool_thread_ctx);
    ck_assert_ptr_null(repl->current->tool_thread_result);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST START_TEST(test_mutex_init_failure)
{
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    mock_pthread_mutex_init_fail = 1;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_err(&result));
    ck_assert_ptr_null(repl);
    talloc_free(result.err);
    talloc_free(ctx);
}

END_TEST START_TEST(test_transition_to_executing_tool)
{
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ik_agent_transition_to_waiting_for_llm(repl->current);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);
    ik_agent_transition_to_executing_tool(repl->current);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);
    ck_assert(repl->current->spinner_state.visible);
    ck_assert(!repl->current->input_buffer_visible);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST START_TEST(test_transition_from_executing_tool)
{
    reset_mocks();
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    result = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&result));
    ik_agent_transition_to_waiting_for_llm(repl->current);
    ik_agent_transition_to_executing_tool(repl->current);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);
    ik_agent_transition_from_executing_tool(repl->current);
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_repl_init_null_parent)
{
    ik_repl_ctx_t *repl = NULL;
    (void)ik_repl_init(NULL, NULL, &repl);
}

END_TEST START_TEST(test_repl_init_null_out)
{
    void *ctx = talloc_new(NULL);
    ik_cfg_t *cfg = ik_test_create_config(ctx);
    ik_shared_ctx_t *shared = NULL;
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t result = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&result));
    (void)ik_repl_init(ctx, shared, NULL);
    talloc_free(ctx);
}

END_TEST
#endif

static Suite *repl_suite(void)
{
    Suite *s = suite_create("REPL");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_repl_init);
    tcase_add_test(tc_core, test_repl_cleanup_null);
    tcase_add_test(tc_core, test_repl_cleanup_null_term);
    tcase_add_test(tc_core, test_repl_run);
    tcase_add_test(tc_core, test_thread_infrastructure_init);
    tcase_add_test(tc_core, test_mutex_init_failure);
    tcase_add_test(tc_core, test_transition_to_executing_tool);
    tcase_add_test(tc_core, test_transition_from_executing_tool);
    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30);
    tcase_add_test_raise_signal(tc_assertions, test_repl_init_null_parent, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_repl_init_null_out, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int32_t main(void)
{
    Suite *s = repl_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
