/**
 * @file repl_tool_completion_polling_test.c
 * @brief Targeted test for tool thread completion polling in ik_repl_run
 *
 * This test covers lines 92-96 in src/repl.c by testing handle_tool_completion
 * which is what gets called when the polling detects completion.
 */

#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/repl_event_handlers.h"
#include "../../../src/openai/client.h"
#include "../../../src/openai/client_multi.h"
#include "../../../src/tool.h"
#include "../../../src/scrollback.h"
#include "../../../src/config.h"
#include "../../../src/wrapper.h"
#include "../../../src/render.h"

/* Mock write for rendering */
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count; /* Always succeed */
}

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
    repl = talloc_zero(ctx, ik_repl_ctx_t);

    /* Create minimal terminal context for rendering */
    repl->term = talloc_zero(repl, ik_term_ctx_t);
    repl->term->screen_rows = 24;
    repl->term->screen_cols = 80;
    repl->term->tty_fd = 1; /* stdout for mock write */

    /* Create render context */
    res_t render_res = ik_render_create(repl, 24, 80, 1, &repl->render);
    ck_assert(!render_res.is_err);

    /* Create input buffer */
    repl->input_buffer = ik_input_buffer_create(repl);
    ck_assert_ptr_nonnull(repl->input_buffer);

    /* Create scrollback */
    repl->scrollback = ik_scrollback_create(repl, 10);
    ck_assert_ptr_nonnull(repl->scrollback);

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(!conv_res.is_err);
    repl->conversation = conv_res.ok;

    /* Create curl_multi handle */
    res_t multi_res = ik_openai_multi_create(repl);
    ck_assert(!multi_res.is_err);
    repl->multi = multi_res.ok;

    /* Create config */
    repl->cfg = talloc_zero(repl, ik_cfg_t);
    repl->cfg->max_tool_turns = 5;

    /* Initialize thread infrastructure */
    pthread_mutex_init_(&repl->tool_thread_mutex, NULL);
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;
    repl->tool_thread_result = NULL;
    repl->tool_thread_ctx = NULL;

    /* Set initial state to EXECUTING_TOOL to simulate active tool execution */
    repl->state = IK_REPL_STATE_EXECUTING_TOOL;
    repl->tool_iteration_count = 0;
    repl->response_finish_reason = NULL;
}

static void teardown(void)
{
    if (repl != NULL) {
        pthread_mutex_destroy_(&repl->tool_thread_mutex);
    }
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
}

/*
 * Thread function that sets completion flag immediately and then sets quit
 * This allows the event loop to run once, detect completion, and exit
 */
static void *quick_complete_thread_func(void *arg)
{
    ik_repl_ctx_t *repl_ctx = (ik_repl_ctx_t *)arg;

    /* Set result immediately */
    repl_ctx->tool_thread_result = talloc_strdup(repl_ctx->tool_thread_ctx,
        "{\"status\":\"success\",\"output\":\"test result\"}");

    /* Mark as complete */
    pthread_mutex_lock_(&repl_ctx->tool_thread_mutex);
    repl_ctx->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl_ctx->tool_thread_mutex);

    /* Wait for event loop to detect completion and transition to IDLE */
    /* Max wait: 300 iterations * 10ms = 3 seconds */
    /* This needs to be longer than the select() timeout to ensure the event loop */
    /* has time to wake up, detect completion, and transition to IDLE */
    for (int i = 0; i < 300; i++) {
        if (repl_ctx->state == IK_REPL_STATE_IDLE) {
            break;
        }
        usleep(10000); /* 10ms */
    }

    /* Set quit flag to exit event loop after completion is handled */
    atomic_store(&repl_ctx->quit, true);

    return NULL;
}

/*
 * Test that event loop polls for tool thread completion (lines 91-96)
 * This test actually runs ik_repl_run and verifies it detects tool completion
 */
START_TEST(test_tool_completion_polling_and_handling)
{
    /* Set up tool execution state before starting event loop */
    repl->state = IK_REPL_STATE_EXECUTING_TOOL;
    repl->tool_thread_running = true;
    repl->tool_thread_complete = false;

    /* Create thread context */
    repl->tool_thread_ctx = talloc_new(repl);

    /* Create pending tool call */
    repl->pending_tool_call = ik_tool_call_create(repl,
                                                   "call_test123",
                                                   "glob",
                                                   "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->pending_tool_call);

    /* Set finish reason to "stop" so we don't continue the tool loop */
    repl->response_finish_reason = talloc_strdup(repl, "stop");

    /* Start thread that will complete and then set quit flag */
    pthread_create_(&repl->tool_thread, NULL, quick_complete_thread_func, repl);

    /* Run event loop - it should detect completion and exit */
    res_t result = ik_repl_run(repl);

    /* Verify event loop ran successfully */
    ck_assert(!is_err(&result));

    /* Verify state transitioned to IDLE (because finish_reason was "stop") */
    ck_assert_int_eq(repl->state, IK_REPL_STATE_IDLE);

    /* Verify pending_tool_call was cleared */
    ck_assert_ptr_null(repl->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_ge(repl->conversation->message_count, 2);

    /* Verify thread was joined */
    ck_assert(!repl->tool_thread_running);
    ck_assert(!repl->tool_thread_complete);
}
END_TEST

/*
 * Thread function for test_tool_completion_with_continuation
 */
static void *completion_test_thread_func(void *arg)
{
    ik_repl_ctx_t *repl_ctx = (ik_repl_ctx_t *)arg;

    /* Set result */
    repl_ctx->tool_thread_result = talloc_strdup(repl_ctx->tool_thread_ctx,
        "{\"status\":\"success\",\"output\":\"test\"}");

    /* Mark as complete */
    pthread_mutex_lock_(&repl_ctx->tool_thread_mutex);
    repl_ctx->tool_thread_complete = true;
    pthread_mutex_unlock_(&repl_ctx->tool_thread_mutex);

    return NULL;
}

/*
 * Test tool completion with continuation by directly calling handle_tool_completion
 * (We can't test continuation via ik_repl_run easily because it requires HTTP mocking)
 */
START_TEST(test_tool_completion_with_continuation)
{
    /* Set up tool execution state */
    repl->state = IK_REPL_STATE_EXECUTING_TOOL;
    repl->tool_thread_running = true;
    repl->tool_thread_complete = false;
    repl->tool_iteration_count = 0;

    /* Create thread context */
    repl->tool_thread_ctx = talloc_new(repl);

    /* Create pending tool call */
    repl->pending_tool_call = ik_tool_call_create(repl,
                                                   "call_test456",
                                                   "glob",
                                                   "{\"pattern\": \"*.h\"}");
    ck_assert_ptr_nonnull(repl->pending_tool_call);

    /* Start thread that will complete */
    pthread_create_(&repl->tool_thread, NULL, completion_test_thread_func, repl);

    /* Wait for thread to complete */
    int max_wait = 200;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        complete = repl->tool_thread_complete;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Set finish reason to "tool_calls" to trigger continuation */
    repl->response_finish_reason = talloc_strdup(repl, "tool_calls");

    /* Directly call handle_tool_completion */
    handle_tool_completion(repl);

    /* Verify pending_tool_call was cleared */
    ck_assert_ptr_null(repl->pending_tool_call);

    /* Verify tool iteration count was incremented */
    ck_assert_int_eq(repl->tool_iteration_count, 1);
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
