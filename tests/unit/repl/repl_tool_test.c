#include "../../test_utils.h"
#include "../../../src/agent.h"
#include "../../../src/message.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "repl.h"
#include "shared.h"
#include "tool.h"
#include "scrollback.h"
#include "config.h"
#include "debug_pipe.h"
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
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;

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

    /* Create minimal repl context for testing */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);

    /* Create minimal shared context for test */
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->db_ctx = NULL;  /* No database by default - tests can override */
    repl->shared->session_id = 0;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Initialize messages array (new API) */
    repl->current->messages = NULL;
    repl->current->message_count = 0;
    repl->current->message_capacity = 0;

    /* Create scrollback */
    repl->current->scrollback = ik_scrollback_create(repl, 10);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    /* Create pending_tool_call with a simple glob call */
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                           "call_test123",
                                                           "glob",
                                                           "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);
}

static void teardown(void)
{
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
 * Tests for ik_repl_execute_pending_tool
 */

START_TEST(test_execute_pending_tool_basic) {
    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added to conversation */
    /* Should have 2 messages: tool_call (assistant) and tool_result (tool) */
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* First message should be assistant (tool_call) */
    ik_message_t *tc_msg = repl->current->messages[0];
    ck_assert(tc_msg->role == IK_ROLE_ASSISTANT);

    /* Second message should be tool (tool_result) */
    ik_message_t *result_msg = repl->current->messages[1];
    ck_assert(result_msg->role == IK_ROLE_TOOL);
}
END_TEST

START_TEST(test_execute_pending_tool_clears_pending) {
    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is NULL after execution */
    ck_assert_ptr_null(repl->current->pending_tool_call);
}

END_TEST

START_TEST(test_execute_pending_tool_conversation_messages) {
    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* First message should be assistant (tool_call) with content blocks */
    ik_message_t *tc_msg = repl->current->messages[0];
    ck_assert(tc_msg->role == IK_ROLE_ASSISTANT);
    ck_assert_uint_ge(tc_msg->content_count, 1);

    /* Second message should be tool (tool_result) with content blocks */
    ik_message_t *result_msg = repl->current->messages[1];
    ck_assert(result_msg->role == IK_ROLE_TOOL);
    ck_assert_uint_ge(result_msg->content_count, 1);
}

END_TEST

START_TEST(test_execute_pending_tool_file_read) {
    /* Change to file_read tool */
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                           "call_read123",
                                                           "file_read",
                                                           "{\"path\": \"/etc/hostname\"}");

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added */
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

START_TEST(test_execute_pending_tool_debug_output) {
    /* Note: This test previously verified debug pipe output.
     * Debug pipes have been replaced with JSONL logger calls.
     * The logger writes to .ikigai/logs/current.log instead of a pipe.
     * We verify that the function executes successfully without the debug pipe. */

    /* Execute pending tool call (now uses logger instead of debug pipe) */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

START_TEST(test_execute_pending_tool_no_debug_pipe) {
    /* Verify that when debug pipe is NULL, execution still works */
    repl->shared->openai_debug_pipe = NULL;

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

START_TEST(test_execute_pending_tool_debug_pipe_null_write_end) {
    /* Create debug pipe and set write_end to NULL to test that branch */
    res_t debug_res = ik_debug_pipe_create(ctx, "[openai]");
    ck_assert(!debug_res.is_err);
    ik_debug_pipe_t *debug_pipe = (ik_debug_pipe_t *)debug_res.ok;
    ck_assert_ptr_nonnull(debug_pipe);

    /* Verify pipe has write_end initially */
    ck_assert_ptr_nonnull(debug_pipe->write_end);

    /* Set write_end to NULL but keep pipe non-NULL */
    fclose(debug_pipe->write_end);
    debug_pipe->write_end = NULL;
    repl->shared->openai_debug_pipe = debug_pipe;

    /* Execute pending tool call - should not crash even with NULL write_end */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

START_TEST(test_execute_pending_tool_db_persistence) {
    /* Create a mock database context */
    repl->shared->db_ctx = (ik_db_ctx_t *)talloc_zero(repl, char);
    repl->shared->session_id = 42;

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify db insert was called twice (tool_call and tool_result) */
    ck_assert_int_eq(db_insert_call_count, 2);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added to conversation */
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

START_TEST(test_execute_pending_tool_no_db_ctx) {
    /* Set db_ctx to NULL - should not persist */
    repl->shared->db_ctx = NULL;
    repl->shared->session_id = 42;

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify db insert was NOT called */
    ck_assert_int_eq(db_insert_call_count, 0);

    /* Verify execution still succeeded */
    ck_assert_ptr_null(repl->current->pending_tool_call);
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

START_TEST(test_execute_pending_tool_no_session_id) {
    /* Set session_id to 0 - should not persist */
    repl->shared->db_ctx = (ik_db_ctx_t *)talloc_zero(repl, char);
    repl->shared->session_id = 0;

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify db insert was NOT called */
    ck_assert_int_eq(db_insert_call_count, 0);

    /* Verify execution still succeeded */
    ck_assert_ptr_null(repl->current->pending_tool_call);
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

/*
 * Test suite
 */

static Suite *repl_tool_suite(void)
{
    Suite *s = suite_create("REPL Tool Execution");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_execute_pending_tool_basic);
    tcase_add_test(tc_core, test_execute_pending_tool_clears_pending);
    tcase_add_test(tc_core, test_execute_pending_tool_conversation_messages);
    tcase_add_test(tc_core, test_execute_pending_tool_file_read);
    tcase_add_test(tc_core, test_execute_pending_tool_debug_output);
    tcase_add_test(tc_core, test_execute_pending_tool_no_debug_pipe);
    tcase_add_test(tc_core, test_execute_pending_tool_debug_pipe_null_write_end);
    tcase_add_test(tc_core, test_execute_pending_tool_db_persistence);
    tcase_add_test(tc_core, test_execute_pending_tool_no_db_ctx);
    tcase_add_test(tc_core, test_execute_pending_tool_no_session_id);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
