#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <unistd.h>
#include "repl.h"
#include "openai/client.h"
#include "tool.h"
#include "scrollback.h"
#include "config.h"
#include "debug_pipe.h"
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
 * Test async tool execution with debug pipe that has NULL write_end
 * This tests the branch where openai_debug_pipe is NOT NULL but write_end IS NULL
 */
START_TEST(test_async_tool_debug_pipe_null_write_end) {
    /* Create debug pipe */
    res_t debug_res = ik_debug_pipe_create(ctx, "[openai]");
    ck_assert(!debug_res.is_err);
    ik_debug_pipe_t *debug_pipe = (ik_debug_pipe_t *)debug_res.ok;
    ck_assert_ptr_nonnull(debug_pipe);
    ck_assert_ptr_nonnull(debug_pipe->write_end);

    /* Close write_end and set to NULL */
    fclose(debug_pipe->write_end);
    debug_pipe->write_end = NULL;
    repl->openai_debug_pipe = debug_pipe;

    /* Start async tool execution */
    ik_repl_start_tool_execution(repl);

    /* Wait for thread to complete - increased timeout for tool execution */
    int max_wait = 200; /* 2 seconds total */
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        complete = repl->tool_thread_complete;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Complete execution - this should exercise the branch where
       openai_debug_pipe is NOT NULL but write_end IS NULL */
    ik_repl_complete_tool_execution(repl);

    /* Verify execution succeeded */
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    ck_assert_ptr_null(repl->pending_tool_call);
}
END_TEST
/*
 * Test async tool execution without debug pipe (NULL)
 * This tests the branch where openai_debug_pipe IS NULL
 */
START_TEST(test_async_tool_no_debug_pipe)
{
    /* Set debug pipe to NULL */
    repl->openai_debug_pipe = NULL;

    /* Start async tool execution */
    ik_repl_start_tool_execution(repl);

    /* Wait for thread to complete - increased timeout for tool execution */
    int max_wait = 200; /* 2 seconds total */
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
 * Test async tool execution with working debug pipe
 * This tests the branch where both openai_debug_pipe and write_end are NOT NULL
 */
START_TEST(test_async_tool_with_working_debug_pipe)
{
    /* Create debug pipe with working write_end */
    res_t debug_res = ik_debug_pipe_create(ctx, "[openai]");
    ck_assert(!debug_res.is_err);
    ik_debug_pipe_t *debug_pipe = (ik_debug_pipe_t *)debug_res.ok;
    ck_assert_ptr_nonnull(debug_pipe);
    ck_assert_ptr_nonnull(debug_pipe->write_end);
    repl->openai_debug_pipe = debug_pipe;

    /* Start async tool execution */
    ik_repl_start_tool_execution(repl);

    /* Wait for thread to complete - increased timeout for tool execution */
    int max_wait = 200; /* 2 seconds total */
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
static Suite *repl_tool_debug_branches_suite(void)
{
    Suite *s = suite_create("REPL Tool Debug Branch Coverage");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_async_tool_debug_pipe_null_write_end);
    tcase_add_test(tc_core, test_async_tool_no_debug_pipe);
    tcase_add_test(tc_core, test_async_tool_with_working_debug_pipe);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_debug_branches_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
