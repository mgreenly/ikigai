/**
 * @file repl_event_handlers_test_3.c
 * @brief Unit tests for REPL event handler functions (Part 3) - Reduced size version
 *
 * This file now contains only basic setup and teardown utilities.
 * Tests were split into separate files:
 * - repl_event_handlers_test_3_persist.c (database persistence tests)
 * - repl_event_handlers_test_3_curl.c (curl event handling tests)
 */

#include "repl_event_handlers.h"
#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/scrollback.h"
#include "../../../src/config.h"
#include "../../../src/terminal.h"
#include "../../../src/input.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/common/http_multi.h"
#include "../../../src/scroll_detector.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/render.h"
#include "../../../src/tool.h"
#include "../../../src/message.h"
#include "../../../src/wrapper.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;
static ik_db_ctx_t *fake_db;

/* Mock database insert that succeeds */
res_t ik_db_message_insert_(void *db,
                            int64_t session_id,
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;
    return OK(NULL);
}

/* Mock render frame to avoid needing full render infrastructure */
res_t ik_repl_render_frame_(void *repl_ctx)
{
    (void)repl_ctx;
    return OK(NULL);
}


static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create fake database context */
    fake_db = talloc_zero(ctx, ik_db_ctx_t);

    /* Create shared context */
    shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 1;
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->db_ctx = fake_db;
    shared->session_id = 123;
    shared->logger = NULL;

    /* Create render context for tests that trigger render */
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    (void)res;  // Ignore result in test setup
    shared->render = render;

    /* Create REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agent_count = 0;
    repl->agents = NULL;
    repl->input_parser = NULL;
    repl->scroll_det = NULL;

    /* Create agent context */
    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->input_buffer = ik_input_buffer_create(agent);
    agent->curl_still_running = 0;
    agent->http_error_message = NULL;
    agent->assistant_response = NULL;
    agent->pending_tool_call = NULL;
    agent->provider_instance = NULL;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->tool_iteration_count = 0;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);

    /* Agent metadata for database tests */
    agent->uuid = talloc_strdup(agent, "test-uuid");
    agent->provider = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;

    /* Spinner state */
    agent->spinner_state.visible = false;
    agent->spinner_state.frame_index = 0;

    repl->current = agent;
}

static void teardown(void)
{
    pthread_mutex_destroy(&agent->tool_thread_mutex);
    talloc_free(ctx);
}

/* ========== Basic Test ========== */

START_TEST(test_basic_setup)
{
    /* Just verify that our setup/teardown works */
    ck_assert_ptr_nonnull(repl);
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(shared);
}

END_TEST

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_3");

    TCase *tc_basic = tcase_create("basic");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_basic_setup);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
