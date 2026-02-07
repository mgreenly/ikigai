#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"
/**
 * @file agent_tool_execution_internal_test.c
 * @brief Unit tests for agent-based tool execution - internal tools
 *
 * Tests that internal tool handlers are correctly invoked and results
 * are properly wrapped and stored.
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

/* Mock internal tool handler - returns success JSON */
static char *mock_internal_handler_success(TALLOC_CTX *handler_ctx, ik_agent_ctx_t *agent, const char *arguments_json)
{
    (void)agent;
    (void)arguments_json;
    return talloc_strdup(handler_ctx, "{\"ok\": true}");
}

/* Mock internal tool handler - returns NULL (failure) */
static char *mock_internal_handler_null(TALLOC_CTX *handler_ctx, ik_agent_ctx_t *agent, const char *arguments_json)
{
    (void)handler_ctx;
    (void)agent;
    (void)arguments_json;
    return NULL;
}

/* Helper: create a minimal internal tool schema doc */
static yyjson_doc *create_internal_tool_schema(TALLOC_CTX *alloc_ctx, const char *name)
{
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, root);
    yyjson_mut_obj_add_str(mdoc, root, "name", name);

    yyjson_alc alc = ik_make_talloc_allocator(alloc_ctx);
    yyjson_doc *idoc = yyjson_mut_doc_imut_copy(mdoc, &alc);
    yyjson_mut_doc_free(mdoc);
    return idoc;
}

/* Setup for internal tool tests with DB enabled */
static void setup_internal_tool_with_db(void)
{
    setup();

    /* Enable database path */
    agent_a->shared->db_ctx = (void *)0x1;
    agent_a->shared->session_id = 42;

    /* Create tool registry and register an internal tool */
    ik_tool_registry_t *registry = ik_tool_registry_create(agent_a->shared);
    yyjson_doc *schema = create_internal_tool_schema(registry, "test_internal");
    ik_tool_registry_add_internal(registry, "test_internal", schema,
                                   mock_internal_handler_success, NULL);
    agent_a->shared->tool_registry = registry;

    /* Set pending tool call to use the internal tool */
    talloc_free(agent_a->pending_tool_call);
    agent_a->pending_tool_call = ik_tool_call_create(agent_a,
                                                     "call_int123",
                                                     "test_internal",
                                                     "{\"key\": \"value\"}");
}

/**
 * Test: Internal tool handler returns success - exercises tool_thread_worker
 * internal branch (lines 47-51 of repl_tool.c)
 */
START_TEST(test_internal_tool_handler_success) {
    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called (tool_call + tool_result) */
    ck_assert_int_eq(db_insert_call_count, 2);

    /* Verify tool_result data_json shows success=true */
    ck_assert_ptr_nonnull(captured_tool_result_data_json);
    yyjson_doc *doc = yyjson_read(captured_tool_result_data_json,
                                   strlen(captured_tool_result_data_json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success_val = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success_val);
    ck_assert(yyjson_get_bool(success_val) == true);

    /* Verify output contains the wrapped result */
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    /* The output should contain tool_success: true from ik_tool_wrap_success */
    ck_assert(strstr(output_str, "tool_success") != NULL);

    yyjson_doc_free(doc);

    /* Verify messages were added */
    ck_assert_uint_eq(agent_a->message_count, 2);
    ck_assert_ptr_null(agent_a->pending_tool_call);
}
END_TEST

/**
 * Test: Internal tool handler returns NULL - exercises failure branch
 * (lines 52-53 of repl_tool.c)
 */
START_TEST(test_internal_tool_handler_null) {
    /* Replace the internal tool with one that returns NULL */
    ik_tool_registry_t *registry = agent_a->shared->tool_registry;
    yyjson_doc *schema = create_internal_tool_schema(registry, "test_null_tool");
    ik_tool_registry_add_internal(registry, "test_null_tool", schema,
                                   mock_internal_handler_null, NULL);

    /* Set pending tool call to use the NULL-returning tool */
    talloc_free(agent_a->pending_tool_call);
    agent_a->pending_tool_call = ik_tool_call_create(agent_a,
                                                     "call_null123",
                                                     "test_null_tool",
                                                     "{}");

    /* Execute tool */
    ik_agent_start_tool_execution(agent_a);
    wait_for_tool_completion(agent_a);
    ik_agent_complete_tool_execution(agent_a);

    /* Verify DB insert was called */
    ck_assert_int_eq(db_insert_call_count, 2);

    /* Verify tool_result data_json shows success=false (handler returned NULL) */
    ck_assert_ptr_nonnull(captured_tool_result_data_json);
    yyjson_doc *doc = yyjson_read(captured_tool_result_data_json,
                                   strlen(captured_tool_result_data_json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success_val = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success_val);
    ck_assert(yyjson_get_bool(success_val) == false);

    /* Verify output contains the failure message */
    yyjson_val *output = yyjson_obj_get(root, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert(strstr(output_str, "Handler returned NULL") != NULL);

    yyjson_doc_free(doc);

    /* Verify messages were added */
    ck_assert_uint_eq(agent_a->message_count, 2);
}
END_TEST

/**
 * Test suite
 */
static Suite *agent_tool_execution_internal_suite(void)
{
    Suite *s = suite_create("agent_tool_execution_internal");

    TCase *tc_internal = tcase_create("internal_tools");
    tcase_set_timeout(tc_internal, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_internal, setup_internal_tool_with_db, teardown);
    tcase_add_test(tc_internal, test_internal_tool_handler_success);
    tcase_add_test(tc_internal, test_internal_tool_handler_null);
    suite_add_tcase(s, tc_internal);

    return s;
}

int main(void)
{
    Suite *s = agent_tool_execution_internal_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/agent_tool_execution_internal_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
