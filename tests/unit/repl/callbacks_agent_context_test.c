#include "../../test_constants.h"
/**
 * @file callbacks_agent_context_test.c
 * @brief Unit tests for agent context in REPL HTTP callbacks
 *
 * Tests that callbacks receive agent context and update the correct agent
 * even when repl->current points to a different agent.
 */

#include "agent.h"
#include "repl.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "scrollback.h"
#include "config.h"
#include "providers/provider.h"
#include "tool.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent_a;
static ik_agent_ctx_t *agent_b;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    /* Create minimal REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;

    /* Create two separate agents */
    agent_a = talloc_zero(repl, ik_agent_ctx_t);
    agent_a->scrollback = ik_scrollback_create(agent_a, 80);
    agent_a->streaming_line_buffer = NULL;
    agent_a->http_error_message = NULL;
    agent_a->response_model = NULL;
    agent_a->response_finish_reason = NULL;
    agent_a->response_input_tokens = 0;
    agent_a->response_output_tokens = 0;
    agent_a->response_thinking_tokens = 0;
    agent_a->assistant_response = NULL;
    agent_a->shared = shared;

    agent_b = talloc_zero(repl, ik_agent_ctx_t);
    agent_b->scrollback = ik_scrollback_create(agent_b, 80);
    agent_b->streaming_line_buffer = NULL;
    agent_b->http_error_message = NULL;
    agent_b->response_model = NULL;
    agent_b->response_finish_reason = NULL;
    agent_b->response_input_tokens = 0;
    agent_b->response_output_tokens = 0;
    agent_b->response_thinking_tokens = 0;
    agent_b->assistant_response = NULL;
    agent_b->shared = shared;

    /* Set repl->current to agent_b */
    repl->current = agent_b;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: Streaming callback updates agent A when called with agent A, not repl->current */
START_TEST(test_streaming_callback_uses_agent_context) {
    /* repl->current is agent_b, but we'll pass agent_a to callback */
    const char *chunk = "Hello from agent A\n";

    /* Create stream event */
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .index = 0,
        .data = {
            .delta = {
                .text = chunk
            }

        }

    };

    /* Call streaming callback with agent_a as context */
    res_t result = ik_repl_stream_callback(&event, agent_a);
    ck_assert(is_ok(&result));

    /* Verify agent_a was updated, not agent_b */
    ck_assert_ptr_nonnull(agent_a->assistant_response);
    ck_assert_str_eq(agent_a->assistant_response, chunk);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent_a->scrollback), 1);

    /* Verify agent_b was NOT updated */
    ck_assert_ptr_null(agent_b->assistant_response);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent_b->scrollback), 0);
}
END_TEST
/* Test: Completion callback updates agent A when called with agent A, not repl->current */
START_TEST(test_completion_callback_uses_agent_context) {
    /* repl->current is agent_b, but we'll pass agent_a to callback */
    char *model = talloc_strdup(ctx, "gpt-4");

    /* Create response structure */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = model;
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 42;
    response->content_blocks = NULL;
    response->content_count = 0;

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    /* Call completion callback with agent_a as context */
    res_t result = ik_repl_completion_callback(&completion, agent_a);
    ck_assert(is_ok(&result));

    /* Verify agent_a was updated, not agent_b */
    ck_assert_ptr_nonnull(agent_a->response_model);
    ck_assert_str_eq(agent_a->response_model, "gpt-4");
    ck_assert_int_eq(agent_a->response_output_tokens, 42);

    /* Verify agent_b was NOT updated */
    ck_assert_ptr_null(agent_b->response_model);
    ck_assert_int_eq(agent_b->response_output_tokens, 0);
}

END_TEST
/* Test: Streaming callback with partial line buffer updates correct agent */
START_TEST(test_streaming_partial_buffer_uses_agent_context) {
    /* Set repl->current to agent_b */
    repl->current = agent_b;

    /* Send partial chunk to agent_a (no newline) */
    const char *chunk1 = "Partial ";
    ik_stream_event_t event1 = {
        .type = IK_STREAM_TEXT_DELTA,
        .index = 0,
        .data = {
            .delta = {
                .text = chunk1
            }

        }

    };
    res_t result = ik_repl_stream_callback(&event1, agent_a);
    ck_assert(is_ok(&result));

    /* Verify agent_a has buffered content */
    ck_assert_ptr_nonnull(agent_a->streaming_line_buffer);
    ck_assert_str_eq(agent_a->streaming_line_buffer, "Partial ");

    /* Verify agent_b is unaffected */
    ck_assert_ptr_null(agent_b->streaming_line_buffer);

    /* Complete the line */
    const char *chunk2 = "line\n";
    ik_stream_event_t event2 = {
        .type = IK_STREAM_TEXT_DELTA,
        .index = 0,
        .data = {
            .delta = {
                .text = chunk2
            }

        }

    };
    result = ik_repl_stream_callback(&event2, agent_a);
    ck_assert(is_ok(&result));

    /* Verify agent_a flushed to scrollback */
    ck_assert_ptr_null(agent_a->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent_a->scrollback), 1);

    /* Verify agent_b still has no scrollback */
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent_b->scrollback), 0);
}

END_TEST
/* Test: Completion callback flushes buffer for correct agent */
START_TEST(test_completion_flushes_correct_agent_buffer) {
    /* Set repl->current to agent_b */
    repl->current = agent_b;

    /* Add buffered content to agent_a */
    agent_a->streaming_line_buffer = talloc_strdup(agent_a, "Incomplete");

    /* Create response structure */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create completion */
    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    /* Call completion callback with agent_a */
    res_t result = ik_repl_completion_callback(&completion, agent_a);
    ck_assert(is_ok(&result));

    /* Verify agent_a's buffer was flushed */
    ck_assert_ptr_null(agent_a->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent_a->scrollback), 2); /* content + blank */

    /* Verify agent_b is unaffected */
    ck_assert_ptr_null(agent_b->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(agent_b->scrollback), 0);
}

END_TEST

/*
 * Test suite
 */

static Suite *callbacks_agent_context_suite(void)
{
    Suite *s = suite_create("callbacks_agent_context");

    TCase *tc_core = tcase_create("agent_context");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_streaming_callback_uses_agent_context);
    tcase_add_test(tc_core, test_completion_callback_uses_agent_context);
    tcase_add_test(tc_core, test_streaming_partial_buffer_uses_agent_context);
    tcase_add_test(tc_core, test_completion_flushes_correct_agent_buffer);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = callbacks_agent_context_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
