#include "agent.h"
/**
 * @file repl_tool_completion_polling_test.c
 * @brief Targeted test for tool thread completion polling in ik_repl_run
 *
 * This test covers lines 92-96 in src/repl.c by testing handle_tool_completion
 * which is what gets called when the polling detects completion.
 */

#include "../../test_utils.h"
#include "../../../src/agent.h"
#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>
#include "../../../src/repl.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/openai/client.h"
#include "../../../src/openai/client_multi.h"
#include "../../../src/tool.h"
#include "../../../src/scrollback.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../../src/render.h"

/* Mock write for rendering */
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count; /* Always succeed */
}

/* Mock select to simulate timeout with minimal delay */
int posix_select_(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout)
{
    (void)nfds;
    (void)timeout;
    /* Clear all fd_sets to simulate no events */
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds);
    /* Small delay to allow other threads to run */
    usleep(1000); /* 1ms */
    return 0; /* Timeout - no file descriptors ready */
}

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);

    /* Create shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    repl->shared = shared;

    /* Create config */
    shared->cfg = talloc_zero(ctx, ik_cfg_t);
    shared->cfg->max_tool_turns = 5;

    /* Create minimal terminal context for rendering */
    repl->shared->term = talloc_zero(repl, ik_term_ctx_t);
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;
    repl->shared->term->tty_fd = 1; /* stdout for mock write */

    /* Create render context */
    res_t render_res = ik_render_create(repl, 24, 80, 1, &repl->shared->render);
    ck_assert(!render_res.is_err);

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->repl = repl;
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);
    ck_assert_ptr_nonnull(repl->current->input_buffer);

    /* Create scrollback */
    repl->current->scrollback = ik_scrollback_create(repl, 10);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(!conv_res.is_err);
    repl->current->conversation = conv_res.ok;

    /* Create curl_multi handle */
    res_t multi_res = ik_openai_multi_create(repl);
    ck_assert(!multi_res.is_err);
    repl->current->multi = multi_res.ok;

    /* Initialize thread infrastructure */
    pthread_mutex_init_(&repl->current->tool_thread_mutex, NULL);
    repl->current->tool_thread_running = false;
    repl->current->tool_thread_complete = false;
    repl->current->tool_thread_result = NULL;
    repl->current->tool_thread_ctx = NULL;

    /* Set initial state to EXECUTING_TOOL to simulate active tool execution */
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    repl->current->tool_iteration_count = 0;
    repl->current->response_finish_reason = NULL;
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
}

/*
 * Thread function that simulates tool execution.
 * Sets result and marks complete, then exits immediately.
 * The main thread will join this thread and handle the completion.
 *
 * Note: This thread must NOT wait for state changes because the main
 * thread calls pthread_join during handle_tool_completion, which would
 * cause a deadlock if we waited here.
 */
static void *quick_complete_thread_func(void *arg)
{
    ik_repl_ctx_t *repl_ctx = (ik_repl_ctx_t *)arg;

    /* Set result immediately */
    repl_ctx->current->tool_thread_result = talloc_strdup(repl_ctx->current->tool_thread_ctx,
                                                 "{\"status\":\"success\",\"output\":\"test result\"}");

    /* Mark as complete - main thread will join us after seeing this */
    pthread_mutex_lock_(&repl_ctx->current->tool_thread_mutex);
    repl_ctx->current->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl_ctx->current->tool_thread_mutex);

    /* Exit immediately - main thread will handle the rest */
    return NULL;
}

/*
 * Thread function that waits for IDLE state then sets quit.
 * This runs separately from the tool thread to avoid deadlock.
 */
static void *quit_after_idle_thread_func(void *arg)
{
    ik_repl_ctx_t *repl_ctx = (ik_repl_ctx_t *)arg;

    /* Wait for state to become IDLE (max 10 seconds) */
    for (int i = 0; i < 1000; i++) {
        /* Read state with mutex protection to avoid data race */
        pthread_mutex_lock_(&repl_ctx->current->tool_thread_mutex);
        ik_agent_state_t current_state = repl_ctx->current->state;
        pthread_mutex_unlock_(&repl_ctx->current->tool_thread_mutex);

        if (current_state == IK_AGENT_STATE_IDLE) {
            /* Give main loop one more iteration to stabilize */
            usleep(5000);
            break;
        }
        usleep(10000); /* 10ms */
    }

    /* Set quit flag to exit event loop */
    atomic_store(&repl_ctx->quit, true);

    return NULL;
}

/*
 * Test that event loop polls for tool thread completion (lines 91-96)
 * This test actually runs ik_repl_run and verifies it detects tool completion
 */
START_TEST(test_tool_completion_polling_and_handling) {
    /* Set up tool execution state before starting event loop */
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    repl->current->tool_thread_running = true;
    repl->current->tool_thread_complete = false;

    /* Create thread context */
    repl->current->tool_thread_ctx = talloc_new(repl);

    /* Create pending tool call */
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                  "call_test123",
                                                  "glob",
                                                  "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);

    /* Set finish reason to "stop" so we don't continue the tool loop */
    repl->current->response_finish_reason = talloc_strdup(repl, "stop");

    /* Start tool thread that will set completion flag */
    pthread_create_(&repl->current->tool_thread, NULL, quick_complete_thread_func, repl);

    /* Start a second thread that will set quit after state becomes IDLE */
    pthread_t quit_thread;
    pthread_create_(&quit_thread, NULL, quit_after_idle_thread_func, repl);

    /* Run event loop - it should detect completion and exit when quit is set */
    res_t result = ik_repl_run(repl);

    /* Join the quit thread */
    pthread_join_(quit_thread, NULL);

    /* Verify event loop ran successfully */
    ck_assert(!is_err(&result));

    /* Verify state transitioned to IDLE (because finish_reason was "stop") */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    /* Verify pending_tool_call was cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_ge(repl->current->conversation->message_count, 2);

    /* Verify thread was joined */
    ck_assert(!repl->current->tool_thread_running);
    ck_assert(!repl->current->tool_thread_complete);
}
END_TEST

/*
 * Thread function for test_tool_completion_with_continuation
 */
static void *completion_test_thread_func(void *arg)
{
    ik_repl_ctx_t *repl_ctx = (ik_repl_ctx_t *)arg;

    /* Set result */
    repl_ctx->current->tool_thread_result = talloc_strdup(repl_ctx->current->tool_thread_ctx,
                                                 "{\"status\":\"success\",\"output\":\"test\"}");

    /* Mark as complete */
    pthread_mutex_lock_(&repl_ctx->current->tool_thread_mutex);
    repl_ctx->current->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl_ctx->current->tool_thread_mutex);

    return NULL;
}

/*
 * Test tool completion with continuation by directly calling handle_tool_completion
 * (We can't test continuation via ik_repl_run easily because it requires HTTP mocking)
 */
START_TEST(test_tool_completion_with_continuation) {
    /* Set up tool execution state */
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    repl->current->tool_thread_running = true;
    repl->current->tool_thread_complete = false;
    repl->current->tool_iteration_count = 0;

    /* Create thread context */
    repl->current->tool_thread_ctx = talloc_new(repl);

    /* Create pending tool call */
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                  "call_test456",
                                                  "glob",
                                                  "{\"pattern\": \"*.h\"}");
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);

    /* Start thread that will complete */
    pthread_create_(&repl->current->tool_thread, NULL, completion_test_thread_func, repl);

    /* Wait for thread to complete */
    int max_wait = 200;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Set finish reason to "tool_calls" to trigger continuation */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Directly call handle_tool_completion */
    handle_tool_completion(repl);

    /* Verify pending_tool_call was cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify tool iteration count was incremented */
    ck_assert_int_eq(repl->current->tool_iteration_count, 1);
}
END_TEST

/*
 * Thread function that waits before quitting to allow multiple loop iterations
 */
static void *wait_then_quit_thread_func(void *arg)
{
    ik_repl_ctx_t *repl_ctx = (ik_repl_ctx_t *)arg;

    /* Wait a bit to allow event loop to run multiple iterations */
    usleep(50000); /* 50ms - enough for several select() calls */

    /* Set quit flag to exit event loop */
    atomic_store(&repl_ctx->quit, true);

    return NULL;
}

/*
 * Test state is EXECUTING_TOOL but complete is false
 * This covers the branch where the polling check sees the thread is still running
 */
START_TEST(test_polling_while_tool_executing_not_complete) {
    /* Set up tool execution state with thread NOT complete */
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    repl->current->tool_thread_running = true;
    repl->current->tool_thread_complete = false; /* Key: NOT complete yet */

    /* Create thread context */
    repl->current->tool_thread_ctx = talloc_new(repl);

    /* Create pending tool call */
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                  "call_test789",
                                                  "glob",
                                                  "{\"pattern\": \"*.h\"}");
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);

    /* Start a thread that will set quit after a delay to allow multiple loop iterations */
    pthread_t quit_thread;
    pthread_create_(&quit_thread, NULL, wait_then_quit_thread_func, repl);

    /* Run event loop - should poll multiple times and NOT call handle_tool_completion */
    res_t result = ik_repl_run(repl);

    /* Join the quit thread */
    pthread_join_(quit_thread, NULL);

    /* Verify event loop ran successfully */
    ck_assert(!is_err(&result));

    /* Verify state is still EXECUTING_TOOL (not transitioned) */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);

    /* Verify pending_tool_call is NOT cleared (because completion didn't happen) */
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);

    /* Clean up - manually mark as complete and join thread */
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    repl->current->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
}
END_TEST
/*
 * Test state is NOT EXECUTING_TOOL (e.g., IDLE)
 * This covers the branch where polling check sees we're not in tool execution state
 */
START_TEST(test_polling_when_idle_state)
{
    /* Set state to IDLE - not executing a tool */
    repl->current->state = IK_AGENT_STATE_IDLE;
    repl->current->tool_thread_running = false;
    repl->current->tool_thread_complete = false;

    /* Set quit immediately so we only do one iteration */
    atomic_store(&repl->quit, true);

    /* Run event loop - should NOT call handle_tool_completion because state is IDLE */
    res_t result = ik_repl_run(repl);

    /* Verify event loop ran successfully */
    ck_assert(!is_err(&result));

    /* Verify state is still IDLE */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);
}

END_TEST
/*
 * Test state is WAITING_FOR_LLM
 * This covers another case where state is NOT EXECUTING_TOOL
 */
START_TEST(test_polling_when_waiting_for_llm_state)
{
    /* Set state to WAITING_FOR_LLM - not executing a tool */
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->tool_thread_running = false;
    repl->current->tool_thread_complete = false;

    /* Set quit immediately so we only do one iteration */
    atomic_store(&repl->quit, true);

    /* Run event loop - should NOT call handle_tool_completion because state is not EXECUTING_TOOL */
    res_t result = ik_repl_run(repl);

    /* Verify event loop ran successfully */
    ck_assert(!is_err(&result));

    /* Verify state is still WAITING_FOR_LLM */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
}

END_TEST

/*
 * Test suite
 */
static Suite *repl_tool_completion_polling_suite(void)
{
    Suite *s = suite_create("REPL Tool Completion Polling");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_tool_completion_polling_and_handling);
    tcase_add_test(tc_core, test_tool_completion_with_continuation);
    tcase_add_test(tc_core, test_polling_while_tool_executing_not_complete);
    tcase_add_test(tc_core, test_polling_when_idle_state);
    tcase_add_test(tc_core, test_polling_when_waiting_for_llm_state);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_completion_polling_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
