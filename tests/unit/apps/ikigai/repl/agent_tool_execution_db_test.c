#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"
/**
 * @file agent_tool_execution_db_test.c
 * @brief Unit tests for agent-based tool execution - database JSON building
 *
 * Tests that tool execution correctly builds data_json for database inserts
 * with various combinations of thinking, signatures, and redacted data.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/json_allocator.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"
#include "vendor/yyjson/yyjson.h"
#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Forward declarations for new functions we're testing */
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent);
void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent);

/* Captured data from mock for verification */
static char *captured_tool_call_data_json;
static char *captured_tool_result_data_json;
static int db_insert_call_count;

/* Mock for db message insert - captures data_json for verification */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid; (void)content;

    if (strcmp(kind, "tool_call") == 0) {
        if (captured_tool_call_data_json != NULL) {
            free(captured_tool_call_data_json);
        }
        captured_tool_call_data_json = data_json ? strdup(data_json) : NULL;
    } else if (strcmp(kind, "tool_result") == 0) {
        if (captured_tool_result_data_json != NULL) {
            free(captured_tool_result_data_json);
        }
        captured_tool_result_data_json = data_json ? strdup(data_json) : NULL;
    }
    db_insert_call_count++;
    return OK(NULL);
}

static void *ctx;
static ik_agent_ctx_t *agent_a;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Reset captured data */
    if (captured_tool_call_data_json != NULL) {
        free(captured_tool_call_data_json);
        captured_tool_call_data_json = NULL;
    }
    if (captured_tool_result_data_json != NULL) {
        free(captured_tool_result_data_json);
        captured_tool_result_data_json = NULL;
    }
    db_insert_call_count = 0;

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->db_ctx = NULL;
    shared->session_id = 0;

    /* Create minimal REPL context */
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;

    /* Create agent A */
    agent_a = talloc_zero(repl, ik_agent_ctx_t);
    agent_a->shared = shared;
    agent_a->repl = repl;
    agent_a->scrollback = ik_scrollback_create(agent_a, 80);
    atomic_store(&agent_a->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    /* Messages array starts empty in new API */
    agent_a->messages = NULL;
    agent_a->message_count = 0;
    agent_a->message_capacity = 0;

    pthread_mutex_init_(&agent_a->tool_thread_mutex, NULL);
    agent_a->tool_thread_running = false;
    agent_a->tool_thread_complete = false;
    agent_a->tool_thread_result = NULL;
    agent_a->tool_thread_ctx = NULL;

    /* Create pending tool call for agent A */
    agent_a->pending_tool_call = ik_tool_call_create(agent_a,
                                                     "call_a123",
                                                     "glob",
                                                     "{\"pattern\": \"*.c\"}");
}

static void teardown(void)
{
    talloc_free(ctx);

    /* Clean up captured data */
    if (captured_tool_call_data_json != NULL) {
        free(captured_tool_call_data_json);
        captured_tool_call_data_json = NULL;
    }
    if (captured_tool_result_data_json != NULL) {
        free(captured_tool_result_data_json);
        captured_tool_result_data_json = NULL;
    }
}

/* Helper: wait for tool completion */
static void wait_for_tool_completion(ik_agent_ctx_t *agent)
{
    int max_wait = 12000;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        complete = agent->tool_thread_complete;
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
}

/* Setup for database JSON tests - enables DB path */
static void setup_with_db(void)
{
    setup();

    /* Enable database path by setting non-NULL db_ctx and positive session_id */
    /* The mock doesn't actually need a real db, just non-NULL check passes */
    agent_a->shared->db_ctx = (void *)0x1;
    agent_a->shared->session_id = 42;
}

/**
 * Test: data_json includes thinking block with text
 */
START_TEST(test_build_tool_call_data_json_with_thinking) {
    /* Set pending thinking data */
    agent_a->pending_thinking_text = talloc_strdup(agent_a, "Let me analyze this...");
    agent_a->pending_thinking_signature = NULL;
    agent_a->pending_redacted_data = NULL;

    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called */
    ck_assert_int_eq(db_insert_call_count, 2);
    ck_assert_ptr_nonnull(captured_tool_call_data_json);

    /* Parse and verify JSON structure */
    yyjson_doc *doc = yyjson_read(captured_tool_call_data_json, strlen(captured_tool_call_data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    /* Verify tool call fields */
    yyjson_val *tool_id = yyjson_obj_get(root, "tool_call_id");
    ck_assert_ptr_nonnull(tool_id);
    ck_assert_str_eq(yyjson_get_str(tool_id), "call_a123");

    yyjson_val *tool_name = yyjson_obj_get(root, "tool_name");
    ck_assert_ptr_nonnull(tool_name);
    ck_assert_str_eq(yyjson_get_str(tool_name), "glob");

    /* Verify thinking block */
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *thinking_text = yyjson_obj_get(thinking, "text");
    ck_assert_ptr_nonnull(thinking_text);
    ck_assert_str_eq(yyjson_get_str(thinking_text), "Let me analyze this...");

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: data_json includes thinking block with signature
 */
START_TEST(test_build_tool_call_data_json_with_signature) {
    /* Set pending thinking data with signature */
    agent_a->pending_thinking_text = talloc_strdup(agent_a, "Thinking text here");
    agent_a->pending_thinking_signature = talloc_strdup(agent_a, "EqQBCgIYAhIMbase64signature");
    agent_a->pending_redacted_data = NULL;

    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called */
    ck_assert_ptr_nonnull(captured_tool_call_data_json);

    /* Parse and verify JSON structure */
    yyjson_doc *doc = yyjson_read(captured_tool_call_data_json, strlen(captured_tool_call_data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Verify thinking block has signature */
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *thinking_text = yyjson_obj_get(thinking, "text");
    ck_assert_ptr_nonnull(thinking_text);
    ck_assert_str_eq(yyjson_get_str(thinking_text), "Thinking text here");

    yyjson_val *thinking_sig = yyjson_obj_get(thinking, "signature");
    ck_assert_ptr_nonnull(thinking_sig);
    ck_assert_str_eq(yyjson_get_str(thinking_sig), "EqQBCgIYAhIMbase64signature");

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: data_json without thinking (clean JSON)
 */
START_TEST(test_build_tool_call_data_json_no_thinking) {
    /* No pending thinking data */
    agent_a->pending_thinking_text = NULL;
    agent_a->pending_thinking_signature = NULL;
    agent_a->pending_redacted_data = NULL;

    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called */
    ck_assert_ptr_nonnull(captured_tool_call_data_json);

    /* Parse and verify JSON structure */
    yyjson_doc *doc = yyjson_read(captured_tool_call_data_json, strlen(captured_tool_call_data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Verify tool call fields are present */
    yyjson_val *tool_id = yyjson_obj_get(root, "tool_call_id");
    ck_assert_ptr_nonnull(tool_id);

    yyjson_val *tool_name = yyjson_obj_get(root, "tool_name");
    ck_assert_ptr_nonnull(tool_name);

    yyjson_val *tool_args = yyjson_obj_get(root, "tool_args");
    ck_assert_ptr_nonnull(tool_args);

    /* Verify NO thinking block */
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_null(thinking);

    /* Verify NO redacted_thinking block */
    yyjson_val *redacted = yyjson_obj_get(root, "redacted_thinking");
    ck_assert_ptr_null(redacted);

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: data_json includes redacted_thinking block
 */
START_TEST(test_build_tool_call_data_json_redacted) {
    /* Set pending redacted thinking data */
    agent_a->pending_thinking_text = NULL;
    agent_a->pending_thinking_signature = NULL;
    agent_a->pending_redacted_data = talloc_strdup(agent_a, "EmwKAhgBEgyencrypteddata");

    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called */
    ck_assert_ptr_nonnull(captured_tool_call_data_json);

    /* Parse and verify JSON structure */
    yyjson_doc *doc = yyjson_read(captured_tool_call_data_json, strlen(captured_tool_call_data_json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Verify NO thinking block */
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_null(thinking);

    /* Verify redacted_thinking block */
    yyjson_val *redacted = yyjson_obj_get(root, "redacted_thinking");
    ck_assert_ptr_nonnull(redacted);

    yyjson_val *redacted_data = yyjson_obj_get(redacted, "data");
    ck_assert_ptr_nonnull(redacted_data);
    ck_assert_str_eq(yyjson_get_str(redacted_data), "EmwKAhgBEgyencrypteddata");

    yyjson_doc_free(doc);
}

END_TEST

/**
 * Test: data_json includes thinking and redacted_thinking together
 */
START_TEST(test_build_tool_call_data_json_thinking_and_redacted) {
    /* Set both thinking and redacted data */
    agent_a->pending_thinking_text = talloc_strdup(agent_a, "Some thinking");
    agent_a->pending_thinking_signature = talloc_strdup(agent_a, "sig123");
    agent_a->pending_redacted_data = talloc_strdup(agent_a, "redacted_blob");

    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called */
    ck_assert_ptr_nonnull(captured_tool_call_data_json);

    /* Parse and verify JSON structure */
    yyjson_doc *doc = yyjson_read(captured_tool_call_data_json, strlen(captured_tool_call_data_json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);

    /* Verify thinking block with signature */
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(thinking, "text")), "Some thinking");
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(thinking, "signature")), "sig123");

    /* Verify redacted_thinking block */
    yyjson_val *redacted = yyjson_obj_get(root, "redacted_thinking");
    ck_assert_ptr_nonnull(redacted);
    ck_assert_str_eq(yyjson_get_str(yyjson_obj_get(redacted, "data")), "redacted_blob");

    yyjson_doc_free(doc);

    /* Verify pending fields were cleared */
    ck_assert_ptr_null(agent_a->pending_thinking_text);
    ck_assert_ptr_null(agent_a->pending_thinking_signature);
    ck_assert_ptr_null(agent_a->pending_redacted_data);
}
END_TEST

/**
 * Test suite
 */
static Suite *agent_tool_execution_db_suite(void)
{
    Suite *s = suite_create("agent_tool_execution_db");

    TCase *tc_db_json = tcase_create("db_json");
    tcase_set_timeout(tc_db_json, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_db_json, setup_with_db, teardown);
    tcase_add_test(tc_db_json, test_build_tool_call_data_json_with_thinking);
    tcase_add_test(tc_db_json, test_build_tool_call_data_json_with_signature);
    tcase_add_test(tc_db_json, test_build_tool_call_data_json_no_thinking);
    tcase_add_test(tc_db_json, test_build_tool_call_data_json_redacted);
    tcase_add_test(tc_db_json, test_build_tool_call_data_json_thinking_and_redacted);
    suite_add_tcase(s, tc_db_json);

    return s;
}

int main(void)
{
    Suite *s = agent_tool_execution_db_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/agent_tool_execution_db_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
