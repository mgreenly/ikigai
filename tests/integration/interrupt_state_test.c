#include <check.h>
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

// Forward declarations for mock functions
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
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
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
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
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
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
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
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
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
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
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
    // Create REPL context with database
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = (void *)1;  // Fake database context
    shared->session_id = 123;

    ik_repl_ctx_t *repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;

    // Create agent
    ik_agent_ctx_t *agent = talloc_zero_(test_ctx, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->interrupt_requested = true;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");

    // Set error messages to test cleanup paths
    agent->http_error_message = talloc_strdup(agent, "HTTP error");
    agent->assistant_response = talloc_strdup(agent, "Partial response");

    // Create scrollback
    agent->scrollback = ik_scrollback_create(agent, 80);
    ck_assert_ptr_nonnull(agent->scrollback);

    // Add messages including a tool result to cover all render paths
    agent->messages = talloc_zero_(agent, sizeof(ik_message_t *) * 10);
    agent->message_count = 4;
    agent->message_capacity = 10;

    // User message
    agent->messages[0] = ik_message_create_text(agent, IK_ROLE_USER, "test");

    // Assistant response
    agent->messages[1] = ik_message_create_text(agent, IK_ROLE_ASSISTANT, "response");

    // Tool result message
    agent->messages[2] = ik_message_create_tool_result(agent, "call_123", "output", false);

    // Another user message
    agent->messages[3] = ik_message_create_text(agent, IK_ROLE_USER, "test2");

    repl->current = agent;

    // Call interrupted LLM completion handler
    ik_repl_handle_interrupted_llm_completion(repl, agent);

    // Verify:
    // 1. Interrupt flag is cleared
    ck_assert(!agent->interrupt_requested);

    // 2. State is IDLE
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);

    // 3. Error messages were cleaned up
    ck_assert_ptr_null(agent->http_error_message);
    ck_assert_ptr_null(agent->assistant_response);

    // 4. Messages are kept but last turn is marked as interrupted
    ck_assert_uint_eq(agent->message_count, 4);
    ck_assert(!agent->messages[0]->interrupted);
    ck_assert(!agent->messages[1]->interrupted);
    ck_assert(!agent->messages[2]->interrupted);
    ck_assert(agent->messages[3]->interrupted);

    pthread_mutex_destroy_(&agent->tool_thread_mutex);
}

END_TEST


static Suite *interrupt_state_suite(void)
{
    Suite *s = suite_create("InterruptState");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 10.0);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_interrupt_idle_state);
    tcase_add_test(tc_core, test_interrupt_waiting_for_llm);
    tcase_add_test(tc_core, test_interrupt_executing_tool);
    tcase_add_test(tc_core, test_escape_during_waiting_for_llm);
    tcase_add_test(tc_core, test_escape_during_executing_tool);
    tcase_add_test(tc_core, test_handle_interrupted_llm_completion);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = interrupt_state_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/interrupt_state_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
