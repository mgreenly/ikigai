#include "../../test_utils.h"
#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <unistd.h>
#include "repl.h"
#include "openai/client.h"
#include "tool.h"
#include "scrollback.h"
#include "config.h"
#include "db/connection.h"
#include "db/message.h"
#include "wrapper.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

/* Mock tracking for db message insert */
static int32_t db_insert_call_count = 0;
static char *last_insert_kind = NULL;
static char *last_insert_content = NULL;
static char *last_insert_data_json = NULL;

/* Mock implementation of ik_db_message_insert_ */
res_t ik_db_message_insert_(void *db,
                             int64_t session_id,
                             const char *kind,
                             const char *content,
                             const char *data_json) {
    (void)db;
    (void)session_id;

    db_insert_call_count++;

    // Store last call parameters for verification
    if (last_insert_kind != NULL) {
        free(last_insert_kind);
    }
    last_insert_kind = strdup(kind);

    if (last_insert_content != NULL) {
        free(last_insert_content);
    }
    last_insert_content = content ? strdup(content) : NULL;

    if (last_insert_data_json != NULL) {
        free(last_insert_data_json);
    }
    last_insert_data_json = data_json ? strdup(data_json) : NULL;

    return OK(NULL);
}

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Reset mock state */
    db_insert_call_count = 0;
    if (last_insert_kind != NULL) {
        free(last_insert_kind);
        last_insert_kind = NULL;
    }
    if (last_insert_content != NULL) {
        free(last_insert_content);
        last_insert_content = NULL;
    }
    if (last_insert_data_json != NULL) {
        free(last_insert_data_json);
        last_insert_data_json = NULL;
    }

    repl = talloc_zero(ctx, ik_repl_ctx_t);

    /* Create shared context */
    repl->shared = talloc_zero(ctx, ik_shared_ctx_t);

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

    /* Clean up mock state */
    if (last_insert_kind != NULL) {
        free(last_insert_kind);
        last_insert_kind = NULL;
    }
    if (last_insert_content != NULL) {
        free(last_insert_content);
        last_insert_content = NULL;
    }
    if (last_insert_data_json != NULL) {
        free(last_insert_data_json);
        last_insert_data_json = NULL;
    }
}

/*
 * Test async tool execution start
 */
START_TEST(test_start_tool_execution) {
    /* Start async tool execution */
    ik_repl_start_tool_execution(repl);

    /* Verify thread was started - read under mutex to avoid data race */
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    bool running = repl->tool_thread_running;
    bool initial_complete = repl->tool_thread_complete;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);
    ck_assert(running);
    ck_assert(!initial_complete);

    /* Verify state transition */
    ck_assert_int_eq(repl->state, IK_REPL_STATE_EXECUTING_TOOL);

    /* Verify thread context was created */
    ck_assert_ptr_nonnull(repl->tool_thread_ctx);

    /* Wait for thread to complete (120s for helgrind) */
    int max_wait = 12000;
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

    /* Wait for thread to complete (120s for helgrind) */
    int max_wait = 12000;
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
    ik_msg_t *tc_msg = repl->conversation->messages[0];
    ck_assert_str_eq(tc_msg->kind, "tool_call");

    /* Second message should be tool_result */
    ik_msg_t *result_msg = repl->conversation->messages[1];
    ck_assert_str_eq(result_msg->kind, "tool_result");

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

    /* Wait up to 120 seconds for completion (helgrind is extremely slow with parallel tests) */
    int max_wait = 12000;
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

    /* Wait up to 120 seconds for completion (helgrind is extremely slow with parallel tests) */
    int max_wait = 12000;
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
 * Test async execution with database persistence
 */
START_TEST(test_async_tool_db_persistence)
{
    /* Set up database context */
    repl->shared->db_ctx = (ik_db_ctx_t *)talloc_zero(repl, char);
    repl->shared->session_id = 42;

    /* Start and wait */
    ik_repl_start_tool_execution(repl);

    /* Wait for completion (120s for helgrind) */
    int max_wait = 12000;
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

    /* Verify db insert was called twice (tool_call and tool_result) */
    ck_assert_int_eq(db_insert_call_count, 2);

    /* Verify execution succeeded */
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    ck_assert_ptr_null(repl->pending_tool_call);
}

END_TEST

/*
 * Test async execution without database context
 */
START_TEST(test_async_tool_no_db_ctx)
{
    /* Set db_ctx to NULL - should not persist */
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 42;

    /* Start and wait */
    ik_repl_start_tool_execution(repl);

    /* Wait for completion (120s for helgrind) */
    int max_wait = 12000;
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

    /* Verify db insert was NOT called */
    ck_assert_int_eq(db_insert_call_count, 0);

    /* Verify execution still succeeded */
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    ck_assert_ptr_null(repl->pending_tool_call);
}

END_TEST

/*
 * Test async execution without session ID
 */
START_TEST(test_async_tool_no_session_id)
{
    /* Set session_id to 0 - should not persist */
    repl->shared->db_ctx = (ik_db_ctx_t *)talloc_zero(repl, char);
    repl->shared->session_id = 0;

    /* Start and wait */
    ik_repl_start_tool_execution(repl);

    /* Wait for completion (120s for helgrind) */
    int max_wait = 12000;
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

    /* Verify db insert was NOT called */
    ck_assert_int_eq(db_insert_call_count, 0);

    /* Verify execution still succeeded */
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
    tcase_add_test(tc_core, test_async_tool_db_persistence);
    tcase_add_test(tc_core, test_async_tool_no_db_ctx);
    tcase_add_test(tc_core, test_async_tool_no_session_id);

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
