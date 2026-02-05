#include <check.h>
#include "apps/ikigai/wrapper_pthread.h"
#include <curl/curl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_actions_internal.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/repl_tool_completion.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "tests/helpers/test_utils_helper.h"

// Mock implementations
static int mock_tty_fd = 100;

// Mock tracking for kill/waitpid/usleep
static int mock_kill_call_count = 0;
static pid_t mock_kill_last_pid = 0;
static int mock_kill_last_sig = 0;
static int mock_waitpid_call_count = 0;
static pid_t mock_waitpid_result = -1;  // -1 = process terminated
static int mock_usleep_call_count = 0;
static bool mock_provider_cancel_called = false;

// Forward declarations for mock functions
int posix_open_(const char *pathname, int flags);
int posix_tcgetattr_(int fd, struct termios *termios_p);
int posix_tcsetattr_(int fd, int optional_actions, const struct termios *termios_p);
int posix_tcflush_(int fd, int queue_selector);
ssize_t posix_write_(int fd, const void *buf, size_t count);
ssize_t posix_read_(int fd, void *buf, size_t count);
int posix_ioctl_(int fd, unsigned long request, void *argp);
int posix_close_(int fd);
int kill_(pid_t pid, int sig);
pid_t waitpid_(pid_t pid, int *status, int options);
int usleep_(useconds_t usec);
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
res_t ik_db_message_insert_(void *db_ctx, int64_t session_id, const char *agent_uuid, const char *role, const char *content, const char *data);

// Mock render function
res_t ik_repl_render_frame_(void *repl);

int posix_open_(const char *pathname, int flags)
{
    (void)pathname;
    (void)flags;
    return mock_tty_fd;
}

int posix_tcgetattr_(int fd, struct termios *termios_p)
{
    (void)fd;
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
    return 0;
}

int posix_tcflush_(int fd, int queue_selector)
{
    (void)fd;
    (void)queue_selector;
    return 0;
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
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
    if (argp != NULL) {
        struct winsize *ws = (struct winsize *)argp;
        ws->ws_row = 24;
        ws->ws_col = 80;
    }
    return 0;
}

int posix_close_(int fd)
{
    (void)fd;
    return 0;
}

int kill_(pid_t pid, int sig)
{
    mock_kill_call_count++;
    mock_kill_last_pid = pid;
    mock_kill_last_sig = sig;
    return 0;
}

pid_t waitpid_(pid_t pid, int *status, int options)
{
    (void)pid;
    (void)options;
    mock_waitpid_call_count++;
    if (status != NULL) *status = 0;
    return mock_waitpid_result;
}

int usleep_(useconds_t usec)
{
    (void)usec;
    mock_usleep_call_count++;
    return 0;
}

CURLM *curl_multi_init_(void)
{
    return (CURLM *)1;
}

CURLMcode curl_multi_cleanup_(CURLM *multi)
{
    (void)multi;
    return CURLM_OK;
}

CURLMcode curl_multi_fdset_(CURLM *multi, fd_set *read_fd_set, fd_set *write_fd_set, fd_set *exc_fd_set, int *max_fd)
{
    (void)multi;
    (void)read_fd_set;
    (void)write_fd_set;
    (void)exc_fd_set;
    if (max_fd != NULL) *max_fd = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_timeout_(CURLM *multi, long *timeout)
{
    (void)multi;
    if (timeout != NULL) *timeout = -1;
    return CURLM_OK;
}

CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    (void)multi;
    if (running_handles != NULL) *running_handles = 0;
    return CURLM_OK;
}

CURLMsg *curl_multi_info_read_(CURLM *multi, int *msgs_in_queue)
{
    (void)multi;
    if (msgs_in_queue != NULL) *msgs_in_queue = 0;
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
    return (CURL *)1;
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
    return (struct curl_slist *)1;
}

void curl_slist_free_all_(struct curl_slist *list)
{
    (void)list;
}

int pthread_mutex_init_(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    (void)attr;
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

int pthread_create_(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    return pthread_create(thread, attr, start_routine, arg);
}

int pthread_join_(pthread_t thread, void **retval)
{
    return pthread_join(thread, retval);
}

res_t ik_repl_render_frame_(void *repl)
{
    (void)repl;
    return OK(NULL);
}

// Mock database insert function
res_t ik_db_message_insert_(void *db_ctx, int64_t session_id, const char *agent_uuid, const char *role, const char *content, const char *data)
{
    (void)db_ctx;
    (void)session_id;
    (void)agent_uuid;
    (void)role;
    (void)content;
    (void)data;
    return OK(NULL);
}

// Test fixture
static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    mock_kill_call_count = 0;
    mock_kill_last_pid = 0;
    mock_kill_last_sig = 0;
    mock_waitpid_call_count = 0;
    mock_waitpid_result = -1;
    mock_usleep_call_count = 0;
    mock_provider_cancel_called = false;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Mock provider implementation for testing cancel
typedef struct {
    bool cancel_was_called;
} mock_provider_ctx_t;

static void mock_provider_cancel(void *ctx)
{
    mock_provider_ctx_t *mctx = (mock_provider_ctx_t *)ctx;
    mctx->cancel_was_called = true;
    mock_provider_cancel_called = true;
}

// Test: Provider cancel is called when interrupting WAITING_FOR_LLM with provider
START_TEST(test_interrupt_calls_provider_cancel) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in WAITING_FOR_LLM state
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;

    // Create mock provider instance with cancel function
    mock_provider_ctx_t *mock_ctx = talloc_zero_(test_ctx, sizeof(mock_provider_ctx_t));
    mock_ctx->cancel_was_called = false;

    // Use real ik_provider_vtable_t type
    ik_provider_vtable_t *vt = talloc_zero_(test_ctx, sizeof(ik_provider_vtable_t));
    vt->cancel = mock_provider_cancel;

    // Use real ik_provider_t type
    ik_provider_t *provider = talloc_zero_(test_ctx, sizeof(ik_provider_t));
    provider->name = "mock";
    provider->vt = vt;
    provider->ctx = mock_ctx;

    agent->provider_instance = provider;

    repl->current = agent;

    // Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Verify:
    // 1. Interrupt flag is set
    ck_assert(agent->interrupt_requested);

    // 2. Cancel was called
    ck_assert(mock_provider_cancel_called);
    ck_assert(mock_ctx->cancel_was_called);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Child process termination with immediate exit
START_TEST(test_interrupt_kills_child_process_immediate) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in EXECUTING_TOOL state with child process
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;
    agent->tool_child_pid = 12345;  // Fake PID

    // Configure mock waitpid to return child_pid (process terminated immediately)
    mock_waitpid_result = 12345;

    repl->current = agent;

    // Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Verify:
    // 1. Interrupt flag is set
    ck_assert(agent->interrupt_requested);

    // 2. kill was called once with SIGTERM
    ck_assert_int_eq(mock_kill_call_count, 1);
    ck_assert_int_eq(mock_kill_last_pid, -12345);  // Negative for process group
    ck_assert_int_eq(mock_kill_last_sig, SIGTERM);

    // 3. waitpid was called (process terminated immediately)
    ck_assert_int_ge(mock_waitpid_call_count, 1);

    // 4. SIGKILL was NOT sent (process terminated quickly)
    // Check that kill was only called once (SIGTERM only, no SIGKILL)
    ck_assert_int_eq(mock_kill_call_count, 1);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Child process termination with timeout requiring SIGKILL
START_TEST(test_interrupt_kills_child_process_timeout) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in EXECUTING_TOOL state with child process
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;
    agent->tool_child_pid = 12345;  // Fake PID

    // Configure mock waitpid to return 0 (process still running)
    mock_waitpid_result = 0;

    repl->current = agent;

    // Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Verify:
    // 1. Interrupt flag is set
    ck_assert(agent->interrupt_requested);

    // 2. kill was called twice (SIGTERM then SIGKILL)
    ck_assert_int_eq(mock_kill_call_count, 2);

    // 3. First kill was SIGTERM
    // (We can't check the exact order without more sophisticated mocking,
    // but the last call should be SIGKILL)
    ck_assert_int_eq(mock_kill_last_sig, SIGKILL);
    ck_assert_int_eq(mock_kill_last_pid, -12345);

    // 4. waitpid was called multiple times during the wait loop
    ck_assert_int_ge(mock_waitpid_call_count, 1);

    // 5. usleep was called during the wait loop
    ck_assert_int_ge(mock_usleep_call_count, 1);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST


static Suite *interrupt_action_suite(void)
{
    Suite *s = suite_create("InterruptAction");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 10.0);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_interrupt_calls_provider_cancel);
    tcase_add_test(tc_core, test_interrupt_kills_child_process_immediate);
    tcase_add_test(tc_core, test_interrupt_kills_child_process_timeout);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = interrupt_action_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/apps/ikigai/interrupt_action_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
