#include <check.h>
#include <curl/curl.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "../../src/agent.h"
#include "../../src/input_buffer/core.h"
#include "../../src/message.h"
#include "../../src/providers/provider_vtable.h"
#include "../../src/repl.h"
#include "../../src/repl_actions_internal.h"
#include "../../src/repl_event_handlers.h"
#include "../../src/repl_tool_completion.h"
#include "../../src/scrollback.h"
#include "../../src/shared.h"
#include "../test_utils_helper.h"

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

// Test: Handle interrupt request when IDLE (no-op)
START_TEST(test_interrupt_idle_state) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in IDLE state
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    repl->current = agent;

    // Call interrupt handler - should be no-op for IDLE state
    ik_repl_handle_interrupt_request(repl);

    // Verify state is still IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Handle interrupt request when WAITING_FOR_LLM
START_TEST(test_interrupt_waiting_for_llm) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in WAITING_FOR_LLM state
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;

    // No provider instance (cancel won't be called)
    agent->provider_instance = NULL;

    repl->current = agent;

    // Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Verify interrupt flag is set
    ck_assert(agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Handle interrupt request when EXECUTING_TOOL
START_TEST(test_interrupt_executing_tool) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in EXECUTING_TOOL state
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;

    // No child process (kill won't be called)
    agent->tool_child_pid = 0;

    repl->current = agent;

    // Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Verify interrupt flag is set
    ck_assert(agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Handle ESC during WAITING_FOR_LLM
START_TEST(test_escape_during_waiting_for_llm) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in WAITING_FOR_LLM state
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;
    agent->provider_instance = NULL;

    // Create input buffer
    agent->input_buffer = ik_input_buffer_create(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    repl->current = agent;

    // Call ESC handler
    res_t res = ik_repl_handle_escape_action(repl);

    // Should succeed and set interrupt flag
    ck_assert(!is_err(&res));
    ck_assert(agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Handle ESC during EXECUTING_TOOL
START_TEST(test_escape_during_executing_tool) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in EXECUTING_TOOL state
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = false;
    agent->tool_child_pid = 0;

    // Create input buffer
    agent->input_buffer = ik_input_buffer_create(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    repl->current = agent;

    // Call ESC handler
    res_t res = ik_repl_handle_escape_action(repl);

    // Should succeed and set interrupt flag
    ck_assert(!is_err(&res));
    ck_assert(agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Handle interrupted LLM completion
START_TEST(test_handle_interrupted_llm_completion) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = NULL;  // No database
    shared->session_id = 0;

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = true;

    // Create scrollback
    agent->scrollback = ik_scrollback_create(agent, 80);
    ck_assert_ptr_nonnull(agent->scrollback);

    // Add some messages to simulate a turn
    agent->messages = talloc_zero_(agent, sizeof(ik_message_t *) * 10);
    agent->message_count = 3;
    agent->message_capacity = 10;

    // User message
    agent->messages[0] = ik_message_create_text(agent, IK_ROLE_USER, "test");

    // Assistant response (partial)
    agent->messages[1] = ik_message_create_text(agent, IK_ROLE_ASSISTANT, "response");

    // Another user message
    agent->messages[2] = ik_message_create_text(agent, IK_ROLE_USER, "test2");

    repl->current = agent;

    // Call interrupted LLM completion handler
    ik_repl_handle_interrupted_llm_completion(repl, agent);

    // Verify:
    // 1. Interrupt flag is cleared
    ck_assert(!agent->interrupt_requested);

    // 2. State is IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);

    // 3. Messages rolled back to last user message
    ck_assert_uint_eq(agent->message_count, 2);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

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
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
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
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
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
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
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

// Test: Handle interrupted tool completion with contexts
START_TEST(test_handle_interrupted_tool_completion) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = NULL;  // No database
    shared->session_id = 0;

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in EXECUTING_TOOL state with interrupt requested
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = true;
    agent->tool_thread_running = true;
    agent->tool_thread_complete = false;
    agent->tool_child_pid = 12345;

    // Set tool_thread_ctx and pending_tool_call to test cleanup paths
    agent->tool_thread_ctx = talloc_zero_(agent, 1);  // Dummy context
    agent->pending_tool_call = talloc_zero_(agent, 1);  // Dummy tool call

    // Create scrollback
    agent->scrollback = ik_scrollback_create(agent, 80);
    ck_assert_ptr_nonnull(agent->scrollback);

    repl->current = agent;

    // Call interrupted tool completion handler
    ik_repl_handle_interrupted_tool_completion(repl, agent);

    // Verify:
    // 1. Interrupt flag is cleared
    ck_assert(!agent->interrupt_requested);

    // 2. State is IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);

    // 3. Thread state is reset
    ck_assert(!agent->tool_thread_running);
    ck_assert(!agent->tool_thread_complete);
    ck_assert_ptr_null(agent->tool_thread_result);

    // 4. Child PID is cleared
    ck_assert_int_eq(agent->tool_child_pid, 0);

    // 5. Contexts are freed
    ck_assert_ptr_null(agent->tool_thread_ctx);
    ck_assert_ptr_null(agent->pending_tool_call);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Interrupted tool completion through poll_tool_completions
START_TEST(test_poll_tool_completions_with_interrupt) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = NULL;
    shared->session_id = 0;

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent in EXECUTING_TOOL state with tool complete and interrupt requested
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = true;
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;  // Tool is complete
    agent->tool_child_pid = 0;

    agent->scrollback = ik_scrollback_create(agent, 80);
    ck_assert_ptr_nonnull(agent->scrollback);

    // Add agent to repl agents array
    repl->agent_count = 1;
    repl->agent_capacity = 10;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 10);
    repl->agents[0] = agent;
    repl->current = agent;

    // Call poll_tool_completions - should detect interrupt and call interrupted completion handler
    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(!is_err(&result));

    // Verify state transitioned to IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Interrupted tool completion for non-current agent
START_TEST(test_interrupted_tool_completion_non_current_agent) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = NULL;
    shared->session_id = 0;

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create two agents
    ik_agent_ctx_t *current_agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(current_agent);
    current_agent->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_init_(&current_agent->tool_thread_mutex, NULL);

    ik_agent_ctx_t *other_agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(other_agent);
    other_agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&other_agent->tool_thread_mutex, NULL);
    other_agent->interrupt_requested = true;
    other_agent->tool_thread_running = true;
    other_agent->tool_thread_complete = false;
    other_agent->tool_child_pid = 0;
    other_agent->scrollback = ik_scrollback_create(other_agent, 80);

    repl->current = current_agent;  // Current agent is different

    // Call interrupted tool completion for other_agent
    ik_repl_handle_interrupted_tool_completion(repl, other_agent);

    // Verify state transitioned to IDLE
    ck_assert_int_eq(other_agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!other_agent->interrupt_requested);

    pthread_mutex_destroy_(&current_agent->tool_thread_mutex);
    pthread_mutex_destroy_(&other_agent->tool_thread_mutex);
}

END_TEST

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

// Test: Interrupted tool completion with database
START_TEST(test_interrupted_tool_completion_with_database) {
    // Create REPL context with database
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = (void *)1;  // Fake database context
    shared->session_id = 123;     // Non-zero session ID

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = true;
    agent->tool_thread_running = true;
    agent->tool_thread_complete = false;
    agent->tool_child_pid = 0;
    char *uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->uuid = uuid;
    agent->scrollback = ik_scrollback_create(agent, 80);

    repl->current = agent;

    // Call interrupted tool completion - should log to database
    ik_repl_handle_interrupted_tool_completion(repl, agent);

    // Verify state transitioned to IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST

// Test: Interrupted tool completion through handle_agent_tool_completion
START_TEST(test_handle_agent_tool_completion_with_interrupt) {
    // Create minimal REPL context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = NULL;
    shared->session_id = 0;

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent as current with interrupt requested
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = true;
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;  // Tool is complete
    agent->tool_child_pid = 0;
    agent->scrollback = ik_scrollback_create(agent, 80);

    repl->current = agent;

    // Call handle_agent_tool_completion - should detect interrupt and call interrupted handler
    ik_repl_handle_agent_tool_completion(repl, agent);

    // Verify state transitioned to IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->interrupt_requested);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST


static Suite *interrupt_handling_suite(void)
{
    Suite *s = suite_create("InterruptHandling");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 10.0);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_interrupt_idle_state);
    tcase_add_test(tc_core, test_interrupt_waiting_for_llm);
    tcase_add_test(tc_core, test_interrupt_executing_tool);
    tcase_add_test(tc_core, test_escape_during_waiting_for_llm);
    tcase_add_test(tc_core, test_escape_during_executing_tool);
    tcase_add_test(tc_core, test_handle_interrupted_llm_completion);
    tcase_add_test(tc_core, test_interrupt_calls_provider_cancel);
    tcase_add_test(tc_core, test_interrupt_kills_child_process_immediate);
    tcase_add_test(tc_core, test_interrupt_kills_child_process_timeout);
    tcase_add_test(tc_core, test_handle_interrupted_tool_completion);
    tcase_add_test(tc_core, test_poll_tool_completions_with_interrupt);
    tcase_add_test(tc_core, test_interrupted_tool_completion_non_current_agent);
    tcase_add_test(tc_core, test_interrupted_tool_completion_with_database);
    tcase_add_test(tc_core, test_handle_agent_tool_completion_with_interrupt);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = interrupt_handling_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/interrupt_handling_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
