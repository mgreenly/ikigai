#include "../../test_utils_helper.h"
#include "../../../src/agent.h"
#include "../../../src/message.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "db/connection.h"
#include "db/message.h"
#include "debug_pipe.h"
#include "json_allocator.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"
#include "tool.h"
#include "tool_registry.h"
#include "wrapper.h"
#include "vendor/yyjson/yyjson.h"

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

START_TEST(test_execute_pending_tool_db_data_json_structure) {
    repl->shared->db_ctx = (ik_db_ctx_t *)talloc_zero(repl, char);
    repl->shared->session_id = 42;
    ik_repl_execute_pending_tool(repl);
    ck_assert_int_eq(db_insert_call_count, 2);
    yyjson_doc *doc = yyjson_read(last_insert_data_json, strlen(last_insert_data_json), 0);
    yyjson_val *root = yyjson_doc_get_root_(doc);
    ck_assert_str_eq(yyjson_get_str_(yyjson_obj_get_(root, "tool_call_id")), "call_test123");
    ck_assert_str_eq(yyjson_get_str_(yyjson_obj_get_(root, "name")), "glob");
    ck_assert(yyjson_is_str(yyjson_obj_get_(root, "output")));
    ck_assert(yyjson_is_bool(yyjson_obj_get_(root, "success")));
    yyjson_doc_free(doc);
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

START_TEST(test_execute_pending_tool_registry_null) {
    /* Test when tool_registry is NULL */
    repl->shared->tool_registry = NULL;

    /* Create a pending tool call for a non-existent tool */
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                           "call_ext123",
                                                           "external_tool",
                                                           "{}");

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added (tool_call + failure result) */
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* Second message should contain failure */
    ik_message_t *result_msg = repl->current->messages[1];
    ck_assert(result_msg->role == IK_ROLE_TOOL);
}

END_TEST

START_TEST(test_execute_pending_tool_not_found_in_registry) {
    /* Create a tool registry */
    repl->shared->tool_registry = ik_tool_registry_create(repl);

    /* Create a pending tool call for a non-existent tool */
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                           "call_ext123",
                                                           "nonexistent_tool",
                                                           "{}");

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added (tool_call + failure result) */
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* Second message should contain failure */
    ik_message_t *result_msg = repl->current->messages[1];
    ck_assert(result_msg->role == IK_ROLE_TOOL);
}

END_TEST

START_TEST(test_execute_pending_tool_external_success) {
    /* Create a temporary tool script that succeeds */
    char script_path[] = "/tmp/test_tool_success_XXXXXX";
    int32_t fd = mkstemp(script_path);
    ck_assert_int_ge(fd, 0);

    char script[] =
        "#!/bin/sh\n"
        "printf '{\"result\":\"success\"}'\n";
    ssize_t written = write(fd, script, strlen(script));
    ck_assert_int_eq(written, (ssize_t)strlen(script));
    close(fd);
    chmod(script_path, 0755);

    /* Create a tool registry and add the tool */
    repl->shared->tool_registry = ik_tool_registry_create(repl);
    ck_assert_ptr_nonnull(repl->shared->tool_registry);

    char schema_str[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(repl);
    yyjson_doc *schema_doc = yyjson_read_opts(schema_str, strlen(schema_str), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema_doc);

    res_t add_res = ik_tool_registry_add(repl->shared->tool_registry, "test_tool", script_path, schema_doc);
    ck_assert(is_ok(&add_res));

    /* Create a pending tool call for the external tool */
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                           "call_ext123",
                                                           "test_tool",
                                                           "{}");

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added */
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* Second message should contain success */
    ik_message_t *result_msg = repl->current->messages[1];
    ck_assert(result_msg->role == IK_ROLE_TOOL);

    /* Clean up */
    unlink(script_path);
}

END_TEST

START_TEST(test_execute_pending_tool_external_failure) {
    /* Create a temporary tool script that fails */
    char script_path[] = "/tmp/test_tool_fail_XXXXXX";
    int32_t fd = mkstemp(script_path);
    ck_assert_int_ge(fd, 0);

    char script[] =
        "#!/bin/sh\n"
        "exit 1\n";
    ssize_t written = write(fd, script, strlen(script));
    ck_assert_int_eq(written, (ssize_t)strlen(script));
    close(fd);
    chmod(script_path, 0755);

    /* Create a tool registry and add the tool */
    repl->shared->tool_registry = ik_tool_registry_create(repl);
    ck_assert_ptr_nonnull(repl->shared->tool_registry);

    char schema_str[] = "{\"name\":\"test_tool\"}";
    yyjson_alc allocator = ik_make_talloc_allocator(repl);
    yyjson_doc *schema_doc = yyjson_read_opts(schema_str, strlen(schema_str), 0, &allocator, NULL);
    ck_assert_ptr_nonnull(schema_doc);

    res_t add_res = ik_tool_registry_add(repl->shared->tool_registry, "test_tool", script_path, schema_doc);
    ck_assert(is_ok(&add_res));

    /* Create a pending tool call for the external tool */
    talloc_free(repl->current->pending_tool_call);
    repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                           "call_ext123",
                                                           "test_tool",
                                                           "{}");

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);

    /* Verify messages were added (tool_call + failure result) */
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* Second message should contain failure */
    ik_message_t *result_msg = repl->current->messages[1];
    ck_assert(result_msg->role == IK_ROLE_TOOL);

    /* Clean up */
    unlink(script_path);
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

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_execute_pending_tool_basic);
    tcase_add_test(tc_core, test_execute_pending_tool_clears_pending);
    tcase_add_test(tc_core, test_execute_pending_tool_conversation_messages);
    tcase_add_test(tc_core, test_execute_pending_tool_file_read);
    tcase_add_test(tc_core, test_execute_pending_tool_db_persistence);
    tcase_add_test(tc_core, test_execute_pending_tool_db_data_json_structure);
    tcase_add_test(tc_core, test_execute_pending_tool_no_db_ctx);
    tcase_add_test(tc_core, test_execute_pending_tool_no_session_id);
    tcase_add_test(tc_core, test_execute_pending_tool_registry_null);
    tcase_add_test(tc_core, test_execute_pending_tool_not_found_in_registry);
    tcase_add_test(tc_core, test_execute_pending_tool_external_success);
    tcase_add_test(tc_core, test_execute_pending_tool_external_failure);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_tool_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
