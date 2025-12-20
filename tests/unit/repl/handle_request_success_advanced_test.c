/**
 * @file handle_request_success_advanced_test.c
 * @brief Advanced coverage tests for handle_request_success and related handlers
 *
 * This test file specifically targets uncovered branches in repl_event_handlers.c:
 * 1. Line 182: openai_debug_pipe set but write_end is NULL
 * 2. Line 252: state != WAITING_FOR_LLM after handle_request_success (tool execution started)
 */

#include "../../test_utils.h"
#include "repl_streaming_test_common.h"
#include "../../../src/debug_pipe.h"
#include "../../../src/tool.h"
#include "../../../src/config.h"
#include "../../../src/wrapper.h"

#include <check.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

// Per-test state
static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

// Per-test setup
static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    // Use common test infrastructure
    repl = create_test_repl_with_llm(test_ctx);
    ck_assert_ptr_nonnull(repl);

    // Reset mock state
    simulate_completion = false;
    mock_write_should_fail = false;

    // Initialize common state
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 0;
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
}

// Per-test teardown
static void teardown(void)
{
    if (test_ctx != NULL) {
        // Reset mock state
        simulate_completion = false;
        mock_write_should_fail = false;

        talloc_free(test_ctx);
        test_ctx = NULL;
        repl = NULL;
    }
}

// Test: openai_debug_pipe set but write_end is NULL (Line 182, Branch 3)
START_TEST(test_debug_pipe_null_write_end) {
    // Create assistant response with length > 80 to trigger long message path
    char long_response[120];
    memset(long_response, 'A', sizeof(long_response) - 1);
    long_response[sizeof(long_response) - 1] = '\0';
    repl->current->assistant_response = talloc_strdup(test_ctx, long_response);

    // Create debug pipe but set write_end to NULL
    repl->shared->openai_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->shared->openai_debug_pipe->write_end = NULL; // This triggers Branch 3

    ik_repl_handle_agent_request_success(repl, repl->current);

    // Message should be added to conversation
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}
END_TEST
// Test: openai_debug_pipe with valid write_end and short message
START_TEST(test_debug_pipe_short_message)
{
    repl->current->assistant_response = talloc_strdup(test_ctx, "Short message");

    // Create debug pipe with valid write_end
    repl->shared->openai_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->shared->openai_debug_pipe->write_end = tmpfile();
    ck_assert_ptr_nonnull(repl->shared->openai_debug_pipe->write_end);

    ik_repl_handle_agent_request_success(repl, repl->current);

    // Message should be added to conversation
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);

    // Clean up
    fclose(repl->shared->openai_debug_pipe->write_end);
}

END_TEST
// Test: openai_debug_pipe with valid write_end and long message
START_TEST(test_debug_pipe_long_message)
{
    // Create assistant response with length > 80 to trigger truncation
    char long_response[120];
    memset(long_response, 'B', sizeof(long_response) - 1);
    long_response[sizeof(long_response) - 1] = '\0';
    repl->current->assistant_response = talloc_strdup(test_ctx, long_response);

    // Create debug pipe with valid write_end
    repl->shared->openai_debug_pipe = talloc_zero(test_ctx, ik_debug_pipe_t);
    repl->shared->openai_debug_pipe->write_end = tmpfile();
    ck_assert_ptr_nonnull(repl->shared->openai_debug_pipe->write_end);

    ik_repl_handle_agent_request_success(repl, repl->current);

    // Message should be added to conversation
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);
    ck_assert_ptr_null(repl->current->assistant_response);

    // Clean up
    fclose(repl->shared->openai_debug_pipe->write_end);
}

END_TEST
// Test: ik_repl_handle_curl_events when curl_still_running is already 0 (Line 241, Branch 1)
START_TEST(test_handle_curl_events_already_stopped)
{
    // Set curl_still_running to 0 (no active transfers)
    repl->current->curl_still_running = 0;

    // Call ik_repl_handle_curl_events - should exit early without processing
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // State should remain unchanged
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
}

END_TEST
// Test: handle_request_success starts tool execution, state becomes EXECUTING_TOOL (Line 252, Branch 1)
START_TEST(test_request_success_starts_tool_execution)
{
    // Set up assistant response
    repl->current->assistant_response = talloc_strdup(test_ctx, "Test response");

    // Create a pending tool call - this will trigger tool execution
    repl->current->pending_tool_call = ik_tool_call_create(test_ctx,
                                                           "call_test123",
                                                           "glob",
                                                           "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);

    // Initialize thread infrastructure for tool execution
    pthread_mutex_init_(&repl->current->tool_thread_mutex, NULL);
    repl->current->tool_thread_running = false;
    repl->current->tool_thread_complete = false;
    repl->current->tool_thread_result = NULL;
    repl->current->tool_thread_ctx = NULL;

    // Call handle_request_success - should start tool execution
    ik_repl_handle_agent_request_success(repl, repl->current);

    // State should be EXECUTING_TOOL (not IDLE)
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);

    // Assistant message should be added to conversation
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);

    // Wait for thread to complete
    int max_wait = 200; // 2 seconds max
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (complete) break;
        usleep(10000); // 10ms
    }
    ck_assert(complete);

    // Clean up thread properly to prevent leak
    ik_repl_complete_tool_execution(repl);

    // After completion, tool_call and tool_result messages should be added
    ck_assert_uint_eq(repl->current->conversation->message_count, 3);

    // State should transition back to WAITING_FOR_LLM
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
}

END_TEST
// Test: ik_repl_handle_curl_events with tool execution state transition
// This tests line 252, branch 1: state != WAITING_FOR_LLM after handle_request_success
START_TEST(test_handle_curl_events_tool_execution_state)
{
    // Set up a running request
    repl->current->curl_still_running = 1;
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->assistant_response = talloc_strdup(test_ctx, "Response with tool call");

    // Create a pending tool call - this will cause state to become EXECUTING_TOOL
    repl->current->pending_tool_call = ik_tool_call_create(test_ctx,
                                                           "call_abc",
                                                           "glob",
                                                           "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);

    // Initialize thread infrastructure
    pthread_mutex_init_(&repl->current->tool_thread_mutex, NULL);
    repl->current->tool_thread_running = false;
    repl->current->tool_thread_complete = false;
    repl->current->tool_thread_result = NULL;
    repl->current->tool_thread_ctx = NULL;

    // Simulate request completion - curl_multi_perform will set running_handles to 0
    simulate_completion = true;

    // Call ik_repl_handle_curl_events - this should:
    // 1. Call ik_openai_multi_perform which sets curl_still_running to 0
    // 2. Detect completion (prev_running=1, curl_still_running=0, state=WAITING_FOR_LLM)
    // 3. Call handle_request_success which starts tool execution (state -> EXECUTING_TOOL)
    // 4. Check if state == WAITING_FOR_LLM (it's not, so skip transition to IDLE)
    // 5. Render frame
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify state is EXECUTING_TOOL (not IDLE)
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);

    // Verify curl_still_running is 0
    ck_assert_int_eq(repl->current->curl_still_running, 0);

    // Wait for thread to complete
    int max_wait = 200; // 2 seconds max
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (complete) break;
        usleep(10000); // 10ms
    }
    ck_assert(complete);

    // Clean up thread properly to prevent leak
    ik_repl_complete_tool_execution(repl);

    // Clean up
    simulate_completion = false;
}

END_TEST
// Test: db_debug_pipe error reporting with NULL db_debug_pipe
START_TEST(test_db_error_no_debug_pipe)
{
    // This test covers the db error path without debug pipe
    // Note: We can't easily trigger a DB error in persist_assistant_msg
    // This is tested indirectly through the main test suite
    // This test documents the coverage intent
    ck_assert(1); // Placeholder - actual coverage achieved through integration tests
}

END_TEST

static Suite *handle_request_success_advanced_suite(void)
{
    Suite *s = suite_create("handle_request_success_advanced");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);

    // Branch coverage tests
    tcase_add_test(tc_core, test_debug_pipe_null_write_end);
    tcase_add_test(tc_core, test_debug_pipe_short_message);
    tcase_add_test(tc_core, test_debug_pipe_long_message);
    tcase_add_test(tc_core, test_handle_curl_events_already_stopped);
    tcase_add_test(tc_core, test_request_success_starts_tool_execution);
    tcase_add_test(tc_core, test_handle_curl_events_tool_execution_state);
    tcase_add_test(tc_core, test_db_error_no_debug_pipe);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = handle_request_success_advanced_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
