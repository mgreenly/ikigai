#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <unistd.h>
#include "repl.h"
#include "openai/client.h"
#include "tool.h"
#include "scrollback.h"
#include "config.h"
#include "wrapper.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
    repl = talloc_zero(ctx, ik_repl_ctx_t);

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(!conv_res.is_err);
    repl->conversation = conv_res.ok;

    /* Create scrollback */
    repl->scrollback = ik_scrollback_create(repl, 10);
    ck_assert_ptr_nonnull(repl->scrollback);

    /* Initialize thread infrastructure */
    pthread_mutex_init_(&repl->tool_thread_mutex, NULL);
    repl->tool_thread_running = false;
    repl->tool_thread_complete = false;
    repl->tool_thread_result = NULL;
    repl->tool_thread_ctx = NULL;

    /* Set initial state */
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;

    /* Create pending_tool_call with a simple glob call */
    repl->pending_tool_call = ik_tool_call_create(repl,
                                                  "call_test123",
                                                  "glob",
                                                  "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->pending_tool_call);
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
 * Test async tool execution start
 */
START_TEST(test_start_tool_execution) {
    /* Start async tool execution */
    ik_repl_start_tool_execution(repl);

    /* Verify thread was started */
    ck_assert(repl->tool_thread_running);
    ck_assert(!repl->tool_thread_complete);

    /* Verify state transition */
    ck_assert_int_eq(repl->state, IK_REPL_STATE_EXECUTING_TOOL);

    /* Verify thread context was created */
    ck_assert_ptr_nonnull(repl->tool_thread_ctx);

    /* Wait for thread to complete */
    int max_wait = 200; // 2 seconds max
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        complete = repl->tool_thread_complete;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);
        if (complete) break;
        usleep(10000); // 10ms
    }

    /* Thread should complete within timeout */
    ck_assert(complete);

    /* Verify result was set */
    ck_assert_ptr_nonnull(repl->tool_thread_result);

    /* Clean up thread to prevent leak */
    ik_repl_complete_tool_execution(repl);
}
END_TEST
/*
 * Test async tool execution completion
 */
START_TEST(test_complete_tool_execution)
{
    /* Start async tool execution first */
    ik_repl_start_tool_execution(repl);

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

    /* Complete the execution */
    ik_repl_complete_tool_execution(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    /* First message should be tool_call */
    ik_openai_msg_t *tc_msg = repl->conversation->messages[0];
    ck_assert_str_eq(tc_msg->role, "tool_call");

    /* Second message should be tool_result */
    ik_openai_msg_t *result_msg = repl->conversation->messages[1];
    ck_assert_str_eq(result_msg->role, "tool_result");

    /* Verify thread state was reset */
    ck_assert(!repl->tool_thread_running);
    ck_assert(!repl->tool_thread_complete);
    ck_assert_ptr_null(repl->tool_thread_result);

    /* Verify state transition back to WAITING_FOR_LLM */
    ck_assert_int_eq(repl->state, IK_REPL_STATE_WAITING_FOR_LLM);
}

END_TEST
/*
 * Test async execution with file_read tool
 */
START_TEST(test_async_tool_file_read)
{
    /* Change to file_read tool */
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = ik_tool_call_create(repl,
                                                  "call_read123",
                                                  "file_read",
                                                  "{\"path\": \"/etc/hostname\"}");

    /* Start and wait */
    ik_repl_start_tool_execution(repl);

    /* Wait up to 30 seconds for completion (valgrind is very slow) */
    int max_wait = 3000;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        complete = repl->tool_thread_complete;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Complete execution */
    ik_repl_complete_tool_execution(repl);

    /* Verify messages were added */
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    ck_assert_ptr_null(repl->pending_tool_call);
}

END_TEST
/*
 * Test async execution with debug pipe
 */
START_TEST(test_async_tool_with_debug_pipe)
{
    /* Create debug pipe */
    res_t debug_res = ik_debug_pipe_create(ctx, "[openai]");
    ck_assert(!debug_res.is_err);
    ik_debug_pipe_t *debug_pipe = (ik_debug_pipe_t *)debug_res.ok;
    repl->openai_debug_pipe = debug_pipe;

    /* Start and wait */
    ik_repl_start_tool_execution(repl);

    /* Wait up to 30 seconds for completion (valgrind is very slow) */
    int max_wait = 3000;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        complete = repl->tool_thread_complete;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Complete execution */
    ik_repl_complete_tool_execution(repl);

    /* Verify execution succeeded */
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    ck_assert_ptr_null(repl->pending_tool_call);
}

END_TEST

/*
 * Test suite
 */
static Suite *repl_async_tool_suite(void)
{
    Suite *s = suite_create("REPL Async Tool Execution");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_start_tool_execution);
    tcase_add_test(tc_core, test_complete_tool_execution);
    tcase_add_test(tc_core, test_async_tool_file_read);
    tcase_add_test(tc_core, test_async_tool_with_debug_pipe);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_async_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
