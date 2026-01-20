/**
 * @file repl_debug_response_test.c
 * @brief Unit tests for debug output in provider completion callback
 *
 * Tests the debug response metadata output when completion callback
 * fires with different response types and metadata.
 */

#include "repl.h"

#include "../../../src/agent.h"
#include "../../test_utils_helper.h"
#include "config.h"
#include "logger.h"
#include "providers/provider.h"
#include "repl_callbacks.h"
#include "scrollback.h"
#include "shared.h"
#include "tool.h"
#include "vendor/yyjson/yyjson.h"

#include <check.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <talloc.h>
#include <unistd.h>

static void *ctx;
static ik_repl_ctx_t *repl;

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    /* Create logger instance for testing - uses IKIGAI_LOG_DIR env var */
    shared->logger = ik_logger_create(shared, "/tmp");

    /* Create minimal REPL context for testing callback */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    repl->shared = shared;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->shared = shared;
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->current->streaming_line_buffer = NULL;
    repl->current->http_error_message = NULL;
    repl->current->response_model = NULL;
    repl->current->response_finish_reason = NULL;
    repl->current->response_input_tokens = 0;
    repl->current->response_output_tokens = 0;
    repl->current->response_thinking_tokens = 0;
    repl->current->pending_tool_call = NULL;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Helper function to read last JSONL entry from log file */
static yyjson_doc *read_last_log_entry(void)
{
    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    if (log_dir == NULL) {
        return NULL;
    }

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/current.log", log_dir);

    FILE *fp = fopen(log_path, "r");
    if (fp == NULL) {
        return NULL;
    }

    /* Read all lines, keep the last one */
    char *last_line = NULL;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (last_line != NULL) {
            free(last_line);
        }
        last_line = strdup(buffer);
    }
    fclose(fp);

    if (last_line == NULL) {
        return NULL;
    }

    /* Parse the JSON */
    yyjson_doc *doc = yyjson_read(last_line, strlen(last_line), 0);
    free(last_line);
    return doc;
}

/* Test: Debug output for successful response with metadata */
START_TEST(test_debug_output_response_success) {
    /* Create successful response with metadata */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "gpt-4o");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.input_tokens = 100;
    response->usage.output_tokens = 42;
    response->usage.thinking_tokens = 0;
    response->usage.total_tokens = 142;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create provider completion */
    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = 0,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Read and verify logger output */
    yyjson_doc *doc = read_last_log_entry();
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    /* Verify log structure */
    yyjson_val *level = yyjson_obj_get(root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert_str_eq(yyjson_get_str(level), "debug");

    yyjson_val *logline = yyjson_obj_get(root, "logline");
    ck_assert_ptr_nonnull(logline);

    /* Verify logline fields */
    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "provider_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "success");

    yyjson_val *model_val = yyjson_obj_get(logline, "model");
    ck_assert_ptr_nonnull(model_val);
    ck_assert_str_eq(yyjson_get_str(model_val), "gpt-4o");

    yyjson_val *input_tokens = yyjson_obj_get(logline, "input_tokens");
    ck_assert_ptr_nonnull(input_tokens);
    ck_assert_int_eq(yyjson_get_int(input_tokens), 100);

    yyjson_val *output_tokens = yyjson_obj_get(logline, "output_tokens");
    ck_assert_ptr_nonnull(output_tokens);
    ck_assert_int_eq(yyjson_get_int(output_tokens), 42);

    yyjson_doc_free(doc);
}

END_TEST
/* Test: Debug output for error response */
START_TEST(test_debug_output_response_error) {
    /* Create error completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 500 server error");
    ik_provider_completion_t completion = {
        .success = false,
        .http_status = 500,
        .response = NULL,
        .error_category = IK_ERR_CAT_SERVER,
        .error_message = error_msg,
        .retry_after_ms = -1
    };

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Read and verify logger output */
    yyjson_doc *doc = read_last_log_entry();
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    /* Verify log structure */
    yyjson_val *level = yyjson_obj_get(root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert_str_eq(yyjson_get_str(level), "debug");

    yyjson_val *logline = yyjson_obj_get(root, "logline");
    ck_assert_ptr_nonnull(logline);

    /* Verify logline fields */
    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "provider_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "error");

    yyjson_doc_free(doc);
}

END_TEST
/* Test: Debug output with tool_call information */
START_TEST(test_debug_output_response_with_tool_call) {
    /* Create response with tool call content block */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "gpt-4o");
    response->finish_reason = IK_FINISH_TOOL_USE;
    response->usage.input_tokens = 100;
    response->usage.output_tokens = 50;
    response->usage.thinking_tokens = 0;
    response->usage.total_tokens = 150;

    /* Add tool call content block */
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    response->content_blocks[0].data.tool_call.id = talloc_strdup(response, "call_123");
    response->content_blocks[0].data.tool_call.name = talloc_strdup(response, "glob");
    response->content_blocks[0].data.tool_call.arguments = talloc_strdup(response, "{\"pattern\":\"*.c\"}");

    /* Create provider completion */
    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = 0,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify that pending_tool_call was set */
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);
    ck_assert_str_eq(repl->current->pending_tool_call->name, "glob");
    ck_assert_str_eq(repl->current->pending_tool_call->arguments, "{\"pattern\":\"*.c\"}");

    /* Read and verify logger output */
    yyjson_doc *doc = read_last_log_entry();
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *logline = yyjson_obj_get(root, "logline");
    ck_assert_ptr_nonnull(logline);

    /* Verify logline fields */
    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "provider_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "success");

    yyjson_val *model_val = yyjson_obj_get(logline, "model");
    ck_assert_ptr_nonnull(model_val);
    ck_assert_str_eq(yyjson_get_str(model_val), "gpt-4o");

    yyjson_val *output_tokens = yyjson_obj_get(logline, "output_tokens");
    ck_assert_ptr_nonnull(output_tokens);
    ck_assert_int_eq(yyjson_get_int(output_tokens), 50);

    yyjson_doc_free(doc);
}

END_TEST
/* Test: Debug output with NULL model */
START_TEST(test_debug_output_null_metadata) {
    /* Create response with NULL model */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;  /* NULL model */
    response->finish_reason = IK_FINISH_STOP;
    response->usage.input_tokens = 0;
    response->usage.output_tokens = 0;
    response->usage.thinking_tokens = 0;
    response->usage.total_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create provider completion */
    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = 0,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Read and verify logger output */
    yyjson_doc *doc = read_last_log_entry();
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *logline = yyjson_obj_get(root, "logline");
    ck_assert_ptr_nonnull(logline);

    /* Verify logline fields */
    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "provider_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "success");

    yyjson_val *model_val = yyjson_obj_get(logline, "model");
    ck_assert_ptr_nonnull(model_val);
    ck_assert_str_eq(yyjson_get_str(model_val), "(null)");

    yyjson_val *output_tokens = yyjson_obj_get(logline, "output_tokens");
    ck_assert_ptr_nonnull(output_tokens);
    ck_assert_int_eq(yyjson_get_int(output_tokens), 0);

    yyjson_doc_free(doc);
}

END_TEST
/* Test: No debug output when logger is NULL */
START_TEST(test_debug_output_no_logger) {
    /* Set logger to NULL */
    repl->shared->logger = NULL;

    /* Create successful response */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "gpt-4o");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.input_tokens = 100;
    response->usage.output_tokens = 42;
    response->usage.thinking_tokens = 0;
    response->usage.total_tokens = 142;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create provider completion */
    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = 0,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    /* Call callback - should not crash with NULL logger */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));
}

END_TEST

/*
 * Test suite
 */

static Suite *repl_debug_response_suite(void)
{
    Suite *s = suite_create("repl_debug_response");

    TCase *tc_debug = tcase_create("debug_output");
    tcase_set_timeout(tc_debug, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_debug, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_debug, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_debug, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_debug, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_debug, suite_setup, NULL);
    tcase_add_checked_fixture(tc_debug, setup, teardown);
    tcase_add_test(tc_debug, test_debug_output_response_success);
    tcase_add_test(tc_debug, test_debug_output_response_error);
    tcase_add_test(tc_debug, test_debug_output_response_with_tool_call);
    tcase_add_test(tc_debug, test_debug_output_null_metadata);
    tcase_add_test(tc_debug, test_debug_output_no_logger);
    suite_add_tcase(s, tc_debug);

    return s;
}

int main(void)
{
    Suite *s = repl_debug_response_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
