#include "agent.h"
/**
 * @file repl_debug_response_test.c
 * @brief Unit tests for debug output in HTTP completion callback
 *
 * Tests the debug response metadata output when completion callback
 * fires with different response types and metadata.
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "scrollback.h"
#include "config.h"
#include "openai/client_multi.h"
#include "tool.h"
#include "logger.h"
#include "vendor/yyjson/yyjson.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <unistd.h>
#include <stdio.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static char *log_file_path;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create temporary log file */
    log_file_path = talloc_strdup(ctx, "/tmp/repl_debug_response_test.log");
    unlink(log_file_path);  // Remove if exists from previous run

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    /* Create logger instance for testing */
    shared->logger = ik_logger_create(shared, "/tmp");

    /* Create minimal REPL context for testing callback */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    repl->shared = shared;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->current->streaming_line_buffer = NULL;
    repl->current->http_error_message = NULL;
    repl->current->response_model = NULL;
    repl->current->response_finish_reason = NULL;
    repl->current->response_completion_tokens = 0;
}

static void teardown(void)
{
    if (log_file_path != NULL) {
        unlink(log_file_path);
    }
    talloc_free(ctx);
}

/* Helper function to read last JSONL entry from log file */
static yyjson_doc *read_last_log_entry(void)
{
    FILE *fp = fopen("/tmp/.ikigai/logs/current.log", "r");
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
    /* Create successful completion with metadata */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "stop");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 42,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
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
    ck_assert_str_eq(yyjson_get_str(event), "openai_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "success");

    yyjson_val *model_val = yyjson_obj_get(logline, "model");
    ck_assert_ptr_nonnull(model_val);
    ck_assert_str_eq(yyjson_get_str(model_val), "gpt-4o");

    yyjson_val *finish_val = yyjson_obj_get(logline, "finish_reason");
    ck_assert_ptr_nonnull(finish_val);
    ck_assert_str_eq(yyjson_get_str(finish_val), "stop");

    yyjson_val *tokens = yyjson_obj_get(logline, "completion_tokens");
    ck_assert_ptr_nonnull(tokens);
    ck_assert_int_eq(yyjson_get_int(tokens), 42);

    yyjson_doc_free(doc);
}

END_TEST
/* Test: Debug output for error response */
START_TEST(test_debug_output_response_error)
{
    /* Create error completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 500 server error");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SERVER_ERROR,
        .http_code = 500,
        .curl_code = CURLE_OK,
        .error_message = error_msg,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
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
    ck_assert_str_eq(yyjson_get_str(event), "openai_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "error");

    yyjson_doc_free(doc);
}

END_TEST
/* Test: Debug output with tool_call information */
START_TEST(test_debug_output_response_with_tool_call)
{
    /* Create tool_call */
    ik_tool_call_t *tc = ik_tool_call_create(ctx, "call_123", "glob", "{\"pattern\":\"*.c\"}");

    /* Create successful completion with tool_call */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "tool_calls");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 50,
        .tool_call = tc
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
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
    ck_assert_str_eq(yyjson_get_str(event), "openai_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "success");

    yyjson_val *model_val = yyjson_obj_get(logline, "model");
    ck_assert_ptr_nonnull(model_val);
    ck_assert_str_eq(yyjson_get_str(model_val), "gpt-4o");

    yyjson_val *finish_val = yyjson_obj_get(logline, "finish_reason");
    ck_assert_ptr_nonnull(finish_val);
    ck_assert_str_eq(yyjson_get_str(finish_val), "tool_calls");

    yyjson_val *tokens = yyjson_obj_get(logline, "completion_tokens");
    ck_assert_ptr_nonnull(tokens);
    ck_assert_int_eq(yyjson_get_int(tokens), 50);

    /* Verify tool_call fields */
    yyjson_val *tool_name = yyjson_obj_get(logline, "tool_call_name");
    ck_assert_ptr_nonnull(tool_name);
    ck_assert_str_eq(yyjson_get_str(tool_name), "glob");

    yyjson_val *tool_args = yyjson_obj_get(logline, "tool_call_args");
    ck_assert_ptr_nonnull(tool_args);
    ck_assert_str_eq(yyjson_get_str(tool_args), "{\"pattern\":\"*.c\"}");

    yyjson_doc_free(doc);
}

END_TEST
/* Test: Debug output with NULL model and finish_reason */
START_TEST(test_debug_output_null_metadata)
{
    /* Create successful completion with NULL metadata */
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = NULL,
        .finish_reason = NULL,
        .completion_tokens = 0,
        .tool_call = NULL
    };

    /* Call callback */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
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
    ck_assert_str_eq(yyjson_get_str(event), "openai_response");

    yyjson_val *type = yyjson_obj_get(logline, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "success");

    yyjson_val *model_val = yyjson_obj_get(logline, "model");
    ck_assert_ptr_nonnull(model_val);
    ck_assert_str_eq(yyjson_get_str(model_val), "(null)");

    yyjson_val *finish_val = yyjson_obj_get(logline, "finish_reason");
    ck_assert_ptr_nonnull(finish_val);
    ck_assert_str_eq(yyjson_get_str(finish_val), "(null)");

    yyjson_val *tokens = yyjson_obj_get(logline, "completion_tokens");
    ck_assert_ptr_nonnull(tokens);
    ck_assert_int_eq(yyjson_get_int(tokens), 0);

    yyjson_doc_free(doc);
}

END_TEST
/* Test: No debug output when logger is NULL */
START_TEST(test_debug_output_no_logger)
{
    /* Set logger to NULL */
    repl->shared->logger = NULL;

    /* Create successful completion */
    char *model = talloc_strdup(ctx, "gpt-4o");
    char *finish_reason = talloc_strdup(ctx, "stop");
    ik_http_completion_t completion = {
        .type = IK_HTTP_SUCCESS,
        .http_code = 200,
        .curl_code = CURLE_OK,
        .error_message = NULL,
        .model = model,
        .finish_reason = finish_reason,
        .completion_tokens = 42,
        .tool_call = NULL
    };

    /* Call callback - should not crash with NULL logger */
    res_t result = ik_repl_http_completion_callback(&completion, repl);
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
