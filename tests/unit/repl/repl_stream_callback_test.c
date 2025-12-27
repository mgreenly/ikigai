/**
 * @file repl_stream_callback_test.c
 * @brief Unit tests for REPL provider stream callback
 *
 * Tests the ik_repl_stream_callback function which handles
 * provider streaming events during response generation.
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "scrollback.h"
#include "logger.h"
#include "providers/provider.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

static void *ctx;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->logger = ik_logger_create(ctx, "/tmp");

    /* Create agent context for streaming state */
    agent = talloc_zero(ctx, ik_agent_ctx_t);
    agent->shared = shared;
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->streaming_line_buffer = NULL;
    agent->assistant_response = NULL;
    agent->http_error_message = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: IK_STREAM_START event initializes state */
START_TEST(test_stream_start_initializes)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_START,
        .data.start.model = "test-model"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify state is initialized */
    ck_assert_ptr_null(agent->assistant_response);
}
END_TEST

/* Test: IK_STREAM_START clears existing assistant_response */
START_TEST(test_stream_start_clears_existing_response)
{
    /* Set up existing assistant_response */
    agent->assistant_response = talloc_strdup(agent, "old response");

    ik_stream_event_t event = {
        .type = IK_STREAM_START,
        .data.start.model = "test-model"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify old response was cleared */
    ck_assert_ptr_null(agent->assistant_response);
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA with text creates assistant_response */
START_TEST(test_text_delta_creates_response)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = "Hello"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify response was created */
    ck_assert_ptr_nonnull(agent->assistant_response);
    ck_assert_str_eq(agent->assistant_response, "Hello");
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA appends to existing response */
START_TEST(test_text_delta_appends_to_response)
{
    /* Set up existing response */
    agent->assistant_response = talloc_strdup(agent, "Hello");

    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = " world"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify text was appended */
    ck_assert_str_eq(agent->assistant_response, "Hello world");
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA with NULL text does nothing */
START_TEST(test_text_delta_null_text)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = NULL
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify no response was created */
    ck_assert_ptr_null(agent->assistant_response);
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA with newline flushes to scrollback */
START_TEST(test_text_delta_newline_flushes)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = "Line 1\n"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify line was flushed to scrollback */
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent->scrollback), 1);
    ck_assert_ptr_null(agent->streaming_line_buffer);
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA without newline buffers text */
START_TEST(test_text_delta_no_newline_buffers)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = "Partial line"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify text was buffered */
    ck_assert_ptr_nonnull(agent->streaming_line_buffer);
    ck_assert_str_eq(agent->streaming_line_buffer, "Partial line");
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent->scrollback), 0);
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA appends to existing buffer */
START_TEST(test_text_delta_appends_to_buffer)
{
    /* Set up existing buffer */
    agent->streaming_line_buffer = talloc_strdup(agent, "Partial");

    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = " line"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify text was appended to buffer */
    ck_assert_ptr_nonnull(agent->streaming_line_buffer);
    ck_assert_str_eq(agent->streaming_line_buffer, "Partial line");
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA with buffered text and newline flushes both */
START_TEST(test_text_delta_buffer_and_newline)
{
    /* Set up existing buffer */
    agent->streaming_line_buffer = talloc_strdup(agent, "Partial");

    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = " line\n"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify buffer was flushed to scrollback */
    ck_assert_ptr_null(agent->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent->scrollback), 1);
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA with just newline creates empty line */
START_TEST(test_text_delta_empty_line)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = "\n"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify empty line was added to scrollback */
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent->scrollback), 1);
}
END_TEST

/* Test: IK_STREAM_TEXT_DELTA with multiple newlines */
START_TEST(test_text_delta_multiple_newlines)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .data.delta.text = "Line 1\nLine 2\nLine 3"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify lines were flushed, last part buffered */
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent->scrollback), 2);
    ck_assert_ptr_nonnull(agent->streaming_line_buffer);
    ck_assert_str_eq(agent->streaming_line_buffer, "Line 3");
}
END_TEST

/* Test: IK_STREAM_THINKING_DELTA is handled (no-op) */
START_TEST(test_thinking_delta)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_THINKING_DELTA,
        .data.delta.text = "Thinking content"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify no scrollback changes (thinking not displayed during streaming) */
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent->scrollback), 0);
}
END_TEST

/* Test: IK_STREAM_TOOL_CALL_START is handled (no-op) */
START_TEST(test_tool_call_start)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TOOL_CALL_START,
        .data.tool_start.id = "call_123",
        .data.tool_start.name = "glob"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));
}
END_TEST

/* Test: IK_STREAM_TOOL_CALL_DELTA is handled (no-op) */
START_TEST(test_tool_call_delta)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TOOL_CALL_DELTA,
        .data.tool_delta.arguments = "{\"pattern\":"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));
}
END_TEST

/* Test: IK_STREAM_TOOL_CALL_DONE is handled (no-op) */
START_TEST(test_tool_call_done)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_TOOL_CALL_DONE
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));
}
END_TEST

/* Test: IK_STREAM_DONE stores token counts */
START_TEST(test_stream_done_stores_tokens)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_DONE,
        .data.done.finish_reason = IK_FINISH_STOP,
        .data.done.usage = {
            .input_tokens = 100,
            .output_tokens = 200,
            .thinking_tokens = 50
        }
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify tokens were stored */
    ck_assert_int_eq(agent->response_input_tokens, 100);
    ck_assert_int_eq(agent->response_output_tokens, 200);
    ck_assert_int_eq(agent->response_thinking_tokens, 50);
}
END_TEST

/* Test: IK_STREAM_ERROR stores error message */
START_TEST(test_stream_error_stores_message)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_ERROR,
        .data.error.category = IK_ERR_CAT_SERVER,
        .data.error.message = "Server error occurred"
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(agent->http_error_message);
    ck_assert_str_eq(agent->http_error_message, "Server error occurred");
}
END_TEST

/* Test: IK_STREAM_ERROR with NULL message does nothing */
START_TEST(test_stream_error_null_message)
{
    ik_stream_event_t event = {
        .type = IK_STREAM_ERROR,
        .data.error.category = IK_ERR_CAT_SERVER,
        .data.error.message = NULL
    };

    res_t result = ik_repl_stream_callback(&event, agent);
    ck_assert(is_ok(&result));

    /* Verify no error message was stored */
    ck_assert_ptr_null(agent->http_error_message);
}
END_TEST

/*
 * Test suite
 */

static Suite *repl_stream_callback_suite(void)
{
    Suite *s = suite_create("repl_stream_callback");

    TCase *tc_stream = tcase_create("stream_events");
    tcase_add_checked_fixture(tc_stream, setup, teardown);
    tcase_add_test(tc_stream, test_stream_start_initializes);
    tcase_add_test(tc_stream, test_stream_start_clears_existing_response);
    tcase_add_test(tc_stream, test_text_delta_creates_response);
    tcase_add_test(tc_stream, test_text_delta_appends_to_response);
    tcase_add_test(tc_stream, test_text_delta_null_text);
    tcase_add_test(tc_stream, test_text_delta_newline_flushes);
    tcase_add_test(tc_stream, test_text_delta_no_newline_buffers);
    tcase_add_test(tc_stream, test_text_delta_appends_to_buffer);
    tcase_add_test(tc_stream, test_text_delta_buffer_and_newline);
    tcase_add_test(tc_stream, test_text_delta_empty_line);
    tcase_add_test(tc_stream, test_text_delta_multiple_newlines);
    tcase_add_test(tc_stream, test_thinking_delta);
    tcase_add_test(tc_stream, test_tool_call_start);
    tcase_add_test(tc_stream, test_tool_call_delta);
    tcase_add_test(tc_stream, test_tool_call_done);
    tcase_add_test(tc_stream, test_stream_done_stores_tokens);
    tcase_add_test(tc_stream, test_stream_error_stores_message);
    tcase_add_test(tc_stream, test_stream_error_null_message);
    suite_add_tcase(s, tc_stream);

    return s;
}

int main(void)
{
    Suite *s = repl_stream_callback_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
