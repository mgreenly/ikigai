#include "tests/test_constants.h"
/**
 * @file repl_tool_execute_pending_test.c
 * @brief Unit tests for ik_repl_execute_pending_tool
 *
 * Tests the synchronous tool execution path.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/terminal.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"

#include <check.h>
#include <inttypes.h>
#include <talloc.h>

/* Mock db message insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->db_ctx = NULL;
    shared->session_id = 0;
    shared->cfg = talloc_zero(ctx, ik_config_t);
    shared->term = talloc_zero(ctx, ik_term_ctx_t);
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->tool_registry = NULL;
    shared->paths = NULL;

    /* Create REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agents = NULL;
    repl->agent_count = 0;

    /* Create agent */
    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->repl = repl;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->pending_tool_call = NULL;

    repl->current = agent;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: execute pending tool without database */
START_TEST(test_execute_pending_tool_no_db) {
    /* Create pending tool call */
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{\"command\":\"echo test\"}");

    /* Execute */
    ik_repl_execute_pending_tool(repl);

    /* Verify messages were added */
    ck_assert_uint_ge(agent->message_count, 2);  /* tool_call + tool_result */

    /* Verify pending tool call was cleared */
    ck_assert_ptr_null(agent->pending_tool_call);
}
END_TEST

/* Test: execute pending tool with database */
START_TEST(test_execute_pending_tool_with_db) {
    /* Enable database */
    repl->shared->db_ctx = (void *)0xDEADBEEF;  /* Non-NULL mock pointer */
    repl->shared->session_id = 42;

    /* Create pending tool call */
    agent->pending_tool_call = ik_tool_call_create(agent, "call_2", "bash", "{\"command\":\"ls\"}");

    /* Execute */
    ik_repl_execute_pending_tool(repl);

    /* Verify messages were added */
    ck_assert_uint_ge(agent->message_count, 2);

    /* Verify pending tool call was cleared */
    ck_assert_ptr_null(agent->pending_tool_call);
}
END_TEST

/**
 * Test suite
 */
static Suite *repl_tool_execute_pending_suite(void)
{
    Suite *s = suite_create("repl_tool_execute_pending");

    TCase *tc_core = tcase_create("core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_execute_pending_tool_no_db);
    tcase_add_test(tc_core, test_execute_pending_tool_with_db);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_execute_pending_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_tool_execute_pending_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
