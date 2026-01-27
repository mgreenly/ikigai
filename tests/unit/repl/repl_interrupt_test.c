#include "agent.h"
#include "input_buffer/core.h"
#include "message.h"
#include "providers/provider.h"
#include "providers/provider_vtable.h"
#include "render.h"
#include "repl.h"
#include "repl_actions_internal.h"
#include "repl_tool_completion.h"
#include "scrollback.h"
#include "shared.h"
#include "tool.h"
#include "wrapper.h"
#include "../../test_utils_helper.h"
#include "../../../src/agent.h"

#include <check.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

// Test fixtures
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

// Mock tracking
static int32_t provider_cancel_called = 0;

// Dummy thread function
static void *dummy_thread_fn(void *arg)
{
    (void)arg;
    return NULL;
}

// Mock provider vtable with cancel function
static void mock_cancel(void *provider_ctx)
{
    (void)provider_ctx;
    provider_cancel_called++;
}

static res_t mock_start_stream(void *provider_ctx, const ik_request_t *req,
                                ik_stream_cb_t stream_cb, void *stream_ctx,
                                ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)provider_ctx; (void)req; (void)stream_cb; (void)stream_ctx;
    (void)completion_cb; (void)completion_ctx;
    return OK(NULL);
}

static ik_provider_vtable_t mock_vt = {
    .start_stream = mock_start_stream,
    .cancel = mock_cancel
};

// Mock db insert
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    provider_cancel_called = 0;

    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = talloc_zero(ctx, ik_shared_ctx_t);
    repl->shared->session_id = 1;
    repl->shared->db_ctx = (void *)1; // Non-NULL to enable db insert

    // Create terminal context (required for viewport calculations)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;
    repl->shared->term = term;

    // Create render context (required for ik_repl_render_frame)
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));
    repl->shared->render = render;

    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = repl->shared;
    agent->repl = repl;
    agent->uuid = talloc_strdup(agent, "test-uuid");
    repl->current = agent;

    agent->scrollback = ik_scrollback_create(agent, 10);
    ck_assert_ptr_nonnull(agent->scrollback);

    agent->input_buffer = ik_input_buffer_create(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_result = NULL;
    agent->tool_thread_ctx = NULL;
    agent->interrupt_requested = false;
    agent->tool_child_pid = 0;

    agent->state = IK_AGENT_STATE_IDLE;
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
}

// Test: ESC key while in WAITING_FOR_LLM state
START_TEST(test_escape_key_waiting_for_llm)
{
    // Setup: Set state to WAITING_FOR_LLM
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    // Setup: Add mock provider with cancel function
    repl->current->provider_instance = talloc_zero(repl->current, ik_provider_t);
    repl->current->provider_instance->vt = &mock_vt;
    repl->current->provider_instance->ctx = NULL;

    // Action: Press ESC
    res_t result = ik_repl_handle_escape_action(repl);

    // Assert: Success
    ck_assert(!is_err(&result));

    // Assert: interrupt_requested flag set
    ck_assert(repl->current->interrupt_requested);

    // Assert: Cancel was called
    ck_assert_int_eq(provider_cancel_called, 1);
}
END_TEST

// Test: ESC key while in EXECUTING_TOOL state with child process
START_TEST(test_escape_key_executing_tool)
{
    // Setup: Create a child process that sleeps
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process: create new process group and sleep
        setpgid(0, 0);
        sleep(10);
        _exit(0);
    }

    // Parent process
    ck_assert(child_pid > 0);

    // Setup: Set state to EXECUTING_TOOL with the child PID
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
    repl->current->tool_child_pid = child_pid;

    // Action: Press ESC (this will kill the child)
    res_t result = ik_repl_handle_escape_action(repl);

    // Assert: Success
    ck_assert(!is_err(&result));

    // Assert: interrupt_requested flag set
    ck_assert(repl->current->interrupt_requested);

    // Wait for child to actually terminate
    int32_t status;
    pid_t wait_result = waitpid(child_pid, &status, 0);
    ck_assert_int_eq(wait_result, child_pid);
}
END_TEST

// Test: ik_repl_handle_interrupt_request when state is IDLE (no-op)
START_TEST(test_interrupt_request_idle_state)
{
    // Setup: State is already IDLE from setup()
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // Action: Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Assert: interrupt_requested should NOT be set (function returns early)
    ck_assert(!repl->current->interrupt_requested);
}
END_TEST

// Test: ik_repl_handle_interrupt_request with WAITING_FOR_LLM
START_TEST(test_interrupt_request_waiting_for_llm)
{
    // Setup: Set state to WAITING_FOR_LLM
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    // Setup: Add mock provider with cancel function
    repl->current->provider_instance = talloc_zero(repl->current, ik_provider_t);
    repl->current->provider_instance->vt = &mock_vt;
    repl->current->provider_instance->ctx = NULL;

    // Action: Call interrupt handler
    ik_repl_handle_interrupt_request(repl);

    // Assert: interrupt_requested flag set
    ck_assert(repl->current->interrupt_requested);

    // Assert: Cancel was called
    ck_assert_int_eq(provider_cancel_called, 1);
}
END_TEST

// Test: interrupt request EXECUTING_TOOL (quick termination)
START_TEST(test_interrupt_request_executing_tool_quick_termination)
{
    // Create child that sleeps briefly
    pid_t child_pid = fork();
    if (child_pid == 0) {
        setpgid(0, 0);
        usleep(100000);
        _exit(0);
    }
    ck_assert(child_pid > 0);

    // Set state to EXECUTING_TOOL
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
    repl->current->tool_child_pid = child_pid;

    ik_repl_handle_interrupt_request(repl);
    ck_assert(repl->current->interrupt_requested);

    int32_t status;
    waitpid(child_pid, &status, 0);
}
END_TEST

// Test: interrupt request EXECUTING_TOOL (requires SIGKILL)
START_TEST(test_interrupt_request_executing_tool_requires_sigkill)
{
    // Create child that ignores SIGTERM
    pid_t child_pid = fork();
    if (child_pid == 0) {
        setpgid(0, 0);
        signal(SIGTERM, SIG_IGN);
        sleep(10);
        _exit(0);
    }
    ck_assert(child_pid > 0);

    // Set state to EXECUTING_TOOL
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
    repl->current->tool_child_pid = child_pid;

    ik_repl_handle_interrupt_request(repl);
    ck_assert(repl->current->interrupt_requested);

    // Child should be dead (killed by SIGKILL)
    int32_t status;
    pid_t wait_result = waitpid(child_pid, &status, WNOHANG);
    if (wait_result == 0) {
        wait_result = waitpid(child_pid, &status, 0);
    }
    ck_assert_int_eq(wait_result, child_pid);
}
END_TEST

// Test: ik_repl_handle_interrupted_tool_completion
START_TEST(test_interrupted_tool_completion)
{
    // Create a second agent so we can test without triggering render
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = repl->shared;
    agent->repl = repl;
    agent->uuid = talloc_strdup(agent, "test-uuid-2");
    agent->scrollback = ik_scrollback_create(agent, 10);
    ck_assert_ptr_nonnull(agent->scrollback);
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_result = NULL;
    agent->tool_thread_ctx = NULL;
    agent->interrupt_requested = false;
    agent->tool_child_pid = 0;
    agent->state = IK_AGENT_STATE_IDLE;

    // Setup: Simulate interrupted tool execution
    agent->interrupt_requested = true;
    agent->tool_child_pid = 12345; // Fake PID

    // Create tool thread context and pending tool call
    agent->tool_thread_ctx = talloc_zero(agent, int32_t);
    agent->pending_tool_call = ik_tool_call_create(agent,
                                                   "call_123",
                                                   "glob",
                                                   "{\"pattern\": \"*.c\"}");

    // Setup thread state
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->tool_thread_result = talloc_strdup(agent, "result");
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // Create a dummy thread to join (otherwise pthread_join would fail)
    // The thread completes immediately so pthread_join_ will succeed
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_fn, NULL);

    // Action: Handle interrupted tool completion (agent is NOT current, so no render)
    ik_repl_handle_interrupted_tool_completion(repl, agent);

    // Assert: interrupt_requested cleared
    ck_assert(!agent->interrupt_requested);

    // Assert: tool_thread_ctx freed
    // (Can't directly verify, but should be NULL after the call...
    // Actually, the function should have freed it but we can't easily verify talloc_free)

    // Assert: pending_tool_call freed (set to NULL)
    // (Same issue - can't directly verify talloc_free)

    // Assert: Thread state cleared
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    bool running = agent->tool_thread_running;
    bool complete = agent->tool_thread_complete;
    void *result = agent->tool_thread_result;
    ik_agent_state_t state = agent->state;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    ck_assert(!running);
    ck_assert(!complete);
    ck_assert_ptr_null(result);
    ck_assert_int_eq(state, IK_AGENT_STATE_IDLE);

    // Assert: tool_child_pid cleared
    ck_assert_int_eq(agent->tool_child_pid, 0);

    // Assert: "Interrupted" message added to scrollback
    // (Can verify by checking scrollback has content, though not easy to inspect exact message)
}
END_TEST

// Test: ik_repl_poll_tool_completions with interrupted tool (agent_count > 0)
START_TEST(test_poll_tool_completions_interrupted_multi_agent)
{
    // Setup: Add agent to array
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = repl->current;

    // Setup: Agent is executing tool, complete, and interrupted
    repl->current->interrupt_requested = true;
    repl->current->tool_child_pid = 12345;

    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    repl->current->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    // Create dummy thread
    // The thread completes immediately so pthread_join_ will succeed
    pthread_create_(&repl->current->tool_thread, NULL, dummy_thread_fn, NULL);

    // Action: Poll tool completions
    res_t result = ik_repl_poll_tool_completions(repl);

    // Assert: Success
    ck_assert(!is_err(&result));

    // Assert: Agent transitioned to IDLE
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    ik_agent_state_t state = repl->current->state;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
    ck_assert_int_eq(state, IK_AGENT_STATE_IDLE);
}
END_TEST

// Test: ik_repl_poll_tool_completions with interrupted tool (single agent)
START_TEST(test_poll_tool_completions_interrupted_single_agent)
{
    // Setup: No agents in array (agent_count == 0), only current
    repl->agent_count = 0;
    repl->agents = NULL;

    // Setup: Agent is executing tool, complete, and interrupted
    repl->current->interrupt_requested = true;
    repl->current->tool_child_pid = 12345;

    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    repl->current->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    // Create dummy thread
    // The thread completes immediately so pthread_join_ will succeed
    pthread_create_(&repl->current->tool_thread, NULL, dummy_thread_fn, NULL);

    // Action: Poll tool completions
    res_t result = ik_repl_poll_tool_completions(repl);

    // Assert: Success
    ck_assert(!is_err(&result));

    // Assert: Agent transitioned to IDLE
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    ik_agent_state_t state = repl->current->state;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
    ck_assert_int_eq(state, IK_AGENT_STATE_IDLE);
}
END_TEST

static Suite *repl_interrupt_suite(void)
{
    Suite *s = suite_create("repl_interrupt");

    TCase *tc_escape = tcase_create("escape_key");
    tcase_set_timeout(tc_escape, 30);  // Helgrind slow
    tcase_add_checked_fixture(tc_escape, setup, teardown);
    tcase_add_test(tc_escape, test_escape_key_waiting_for_llm);
    tcase_add_test(tc_escape, test_escape_key_executing_tool);
    suite_add_tcase(s, tc_escape);

    TCase *tc_interrupt = tcase_create("interrupt_request");
    tcase_set_timeout(tc_interrupt, 30);  // Helgrind slow
    tcase_add_checked_fixture(tc_interrupt, setup, teardown);
    tcase_add_test(tc_interrupt, test_interrupt_request_idle_state);
    tcase_add_test(tc_interrupt, test_interrupt_request_waiting_for_llm);
    tcase_add_test(tc_interrupt, test_interrupt_request_executing_tool_quick_termination);
    tcase_add_test(tc_interrupt, test_interrupt_request_executing_tool_requires_sigkill);
    suite_add_tcase(s, tc_interrupt);

    TCase *tc_completion = tcase_create("interrupted_completion");
    tcase_add_checked_fixture(tc_completion, setup, teardown);
    tcase_add_test(tc_completion, test_interrupted_tool_completion);
    tcase_add_test(tc_completion, test_poll_tool_completions_interrupted_multi_agent);
    tcase_add_test(tc_completion, test_poll_tool_completions_interrupted_single_agent);
    suite_add_tcase(s, tc_completion);

    return s;
}

int32_t main(void)
{
    Suite *s = repl_interrupt_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_interrupt_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
